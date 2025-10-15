#include <MIDI.h>
#include <EEPROM.h>
#include <assert.h>

/***********************************
    -= AY3 version 4 (4.00) =-
      ~-~-= phoenix =-~-~-

        + synth engine overhaul
        + AYMID support

    twisted electrons  (c) 2025
***********************************/

enum class InitState { ALL, TONE, NOISE, MIXER, AMP, PITCH, ENVELOPE, ENVTYPE, ALLSTATES };
enum class ChannelState { AY3FILE, ABC, ACB, BAC, BCA, CAB, CBA };
enum class OverrideState { AY3FILE, ON, OFF };
enum class PitchType { TONE, NOISE, ENVELOPE };

//
// DEFINES
//

// flags                            // PRODUCTION MODE: ALL ENABLED
#define FACTORYRESET    1           // ENABLE FACTORY RESET
#define LEDSUPPRESSION  1           // ENABLE LED FLICKER SUPPRESSION
#define ZXTUNING        1           // ADAPT NOTES/ENV VALUES AS FOR ATARI CLOCK
#define CLOCK_LOW_EMU   1           // CLOCK LOW FREQ ADAPTION (500Hz), old firmware emulation

// timing (!don't touch!)
#define COUNT_DELAY_ZX  2           // time-critical (sync: 0..4)           <<< initial offset <<< ?
#define ASYNC_DELAY     62          // time-critical (sync: 31, 62, 124..)  >>> fine offset >>> ?
#define ENC_TIMER       300
#define SAMPLE_CYCLE    30          // sampling of pitches & glides
#define PROCESS_CYCLE   6           // processing of midi clock, arp, lfo, calculate pitches
#define SEQ_CYCLES      190         // processing of sequencer steps by internal clock, mixer (no midi)
#define SELECT_DELAY    7           // prevents sound & gui interferences for AYMID
#define INIT_LOAD_CYCLE 17000       // prevents sound errors by a loading delay of 3000 cycles (20000-x)


// config
#define CFG_MIDICHANNEL 1           // MASTER CHANNEL
#define CFG_REVISION    2           // REV.A, REV.B, (REV.C?)
#define CFG_CLOCKTYPE   3           // ATARI, ZX, 500, (EXT?)
#define CFG_ENVPDTYPE   4           // LOG/LIN, (OLD)LUT

#define BOARD_REVA      0           // Timer A clock (2 Mhz, 500 Khz)
#define BOARD_REVB      1           // Timer B clock (2 Mhz, 1.777 Mhz, 500 Khz)
#define BOARD_REVC      2           // Timer B clock (2 Mhz, 1.777 Mhz, 500 Khz, 1.777 Mhz external [jumpered?])

#define CLOCK_ATARI     0           // 2 Mhz
#define CLOCK_ZX        1           // 1.777 Mhz
#define CLOCK_LOW       2           // 500 Khz
#define CLOCK_ZXEXT     3           // 1.773 Mhz by external source

#define MAX_SEQSTEP     15
#define MAX_REVISION    1
#define MAX_CLOCKTYPE   2
#define MAX_ENVPDTYPE   1
#define MAX_LEDPICCOUNT 18000

#define ENCPINA         11          // CLK
#define ENCPINB         10          // DATA

// consts
#define AY3VOICES       3
#define AY3CHIPS        2
#define AY3_REGISTERS   14

#define MASTER          0           // MASTER INDEX (for held, arpStep, arpOct, noteMem)

#define ARP_UP          1           // ARPEGGIATOR MODES
#define ARP_DOWN        2
#define ARP_UPSCALE     3
#define ARP_RANDOM      4

#define LFO_SHAPE1      1           // LFO MODES
#define LFO_SHAPE2      2
#define LFO_SHAPE3      3
#define LFO_SHAPE4      4

#define VOICE_ENABLE    1
#define VOICE_VOLUME    2
#define VOICE_TUNING    3

#define TGL_AY3FILE_OFF 1
#define TGL_AY3FILE_ON  2
#define RST_AY3FILE     3


// debug
#define TONETEST        0           // DEBUGGING BY TEST TONE (0, 1, 2)

#if TONETEST
    uint32_t cc         = 0;
    byte fine           = 0xff;     // whole byte
    byte cors           = 0x08;     // low nibble
#endif

//
// GLOBALS / HELPER
//

void  (*updateAy32)();              // update function pointer (rev.)

// EEPROM
byte lastPreset         = 0;
byte lastBank           = 0;
byte preset             = 0;        // located at 3800
byte bank               = 0;        // located at 3801
byte masterChannel;                 // located at 3802
byte boardRevision;                 // located at 3803
byte clockType;                     // located at 3804
byte envPeriodType;                 // located at 3805

// general
byte writeConfig        = 0;
int8_t selectedChip     = -1;       // -1 both, 0 chip1, 1 chip2
int8_t lastSingleChip   = 0;
byte seqSetup           = 1;
byte mode               = 1;
byte loadRequest        = true;

// ay3
byte ay3Reg1[14];
byte ay3Reg2[14];
byte ay3Reg1Last[14];
byte ay3Reg2Last[14];
byte BDIRPin            = 14;
byte data7A             = B11111111;
byte data7B             = B11111111;
byte data7ALast         = B11111111;
byte data7BLast         = B11111111;

// counter / race conditions
byte tickcc             = 0;
byte ledtickcc          = 0;
byte btntickcc          = 0;
byte pottickcc          = 0;
byte seqtickcc          = 0;
byte analogcc           = 0;
byte clockcc            = 0;
byte countDown          = 0;
bool inputToggle        = false;
uint16_t maincc         = 0;
uint16_t encodercc      = 0;
uint16_t seqsetupcc     = 0;
uint16_t displaycc      = INIT_LOAD_CYCLE;
uint16_t samplecc       = 0;
uint16_t arpcc          = 0;
uint16_t processcc      = 0;
byte selectedcc         = 0;
byte seqcc              = 0;
unsigned long resetcc;
uint16_t memPointer;

// encoder
int encTimer            = 0;
int lastStateCLK        = -1;       // init -1 to ignore first poll

// buttons
byte butt[20];
byte buttLast[20];
byte selectRow          = 0;
byte pressedRow         = 0;
byte pressedCol         = 9;
byte voiceMode          = 0;
bool seqPressed;
bool encPressed;
bool voicePressed;

// pots
int potLast[5]          = { -1, -1, -1, -1, -1 };

// led
byte ledNumber          = 1;
byte oldNumber;
byte oldMatrix[7];
byte ledMatrix[7];
byte ledMatrixPic[7];

// synth
int base[7]             = { 0, 0, 0, 0, 0, 0, 0 };
byte staggered[]        = { 0, 1, 4, 2, 5, 3, 6 };
byte slots[7]           = { 0, 0, 0, 0, 0, 0, 0 };
byte vol[7]             = { 6, 6, 6, 6, 6, 6, 6 };
byte tune[7]            = { 100, 100, 100, 100, 100, 100, 100 };
int8_t offsetFreq[7];
int8_t offsetNote[7];
int destiPitch[7];
int pitchLast[7];
int pitch[7]            = { 0, 0, 0, 0, 0, 0, 0 };
int bend[7];
byte held[7]            = { 0, 0, 0, 0, 0, 0, 0 };  // MASTER, 1, 2, 3, 4, 5, 6
byte arpStep[7]         = { 1, 1, 1, 1, 1, 1, 1 };  // MASTER, 1, 2, 3, 4, 5, 6
byte arpOct[7]          = { 0, 0, 0, 0, 0, 0, 0 };  // MASTER, 1, 2, 3, 4, 5, 6
byte noteMem[7][20];                                // MASTER, 1, 2, 3, 4, 5, 6
byte noiseFreq[2]       = { 16, 16 };
byte noiseMode          = 0;
byte envNumber[2]       = { 1, 1 };
byte envShape[2];
byte lastEnvSpeedLUT;
uint16_t lastEnvSpeed;
byte envMode            = 0;
byte arpRange           = 1;
byte arpSpeed           = 1;
float lfo               = 0;
float lfoPhase          = 0;
bool clockSynced        = false;
byte lfoSpeed           = 1;
byte lfoShape           = 1;
byte lfoDepth           = 1;
byte detune             = 1;
byte chord              = 1;
byte glide              = 1;
int bender              = 64;
byte clockSpeeds[]      = { 0, 1, 2, 3, 4, 6, 8, 12, 16, 18, 24, 32, 36, 40, 48, 56 };

float lfoIncrements[]   = { 0.01041, 0.01388, 0.02083, 0.04166, 
                            0.08333, 0.16666, 0.33333, 0.5 };

// preset
byte presetTune[7];

// sequencer
byte seq                = 0;
byte key                = 0;
byte seqStep            = 0;
int8_t selectedStep     = 0;
bool seqNoise[16];
bool seqVoice[16];
byte seqNoise1;
byte seqNoise2;
byte seqVoice1;
byte seqVoice2;
byte seqSpeed;
byte seqNote[]          = { 0, 4, 7, 4, 0, 4, 7, 4, 0, 4, 7, 4, 0, 4, 7, 4, 
                            0, 4, 7, 4, 0, 4, 7, 4, 0, 4, 7, 4, 0, 4, 7, 4, 
                            0, 7, 8, 3, 1, 6, 2, 4, 6, 7, 4, 2, 6, 7, 0 };

// envelope speed lookup table - frequencies related to midi notes (16 bit)
// it is only used by the CLOCK_LOW type to approximate the old firmware (3.4)

const int envTp[] = {                                               //              C-Notes
    15289,  14431,  13621,  12856,  12135,  11454,  10811,  10204,  // 0   - 7      2
     9631,   9091,   8581,   8099,   7645,   7215,   6810,   6428,  // 8   - 15     14
     6067,   5727,   5405,   5102,   4816,   4545,   4290,   4050,  // 16  - 23
     3822,   3608,   3405,   3214,   3034,   2863,   2703,   2551,  // 24  - 31     26
     2408,   2273,   2145,   2025,   1911,   1804,   1703,   1607,  // 32  - 39     38
     1517,   1432,   1351,   1276,   1204,   1136,   1073,   1012,  // 40  - 47
      956,    902,    851,    804,    758,    716,    676,    638,  // 48  - 55     50
      602,    568,    536,    506,    478,    451,    426,    402,  // 56  - 63     62
      379,    358,    338,    319,    301,    284,    268,    253,  // 64  - 71
      239,    225,    213,    201,    190,    179,    169,    159,  // 72  - 79     74
      150,    142,    134,    127,    119,    113,    106,    100,  // 80  - 87     86
       95,     89,     84,     80,     75,     71,     67,     63,  // 88  - 95
       60,     56,     53,     50,     47,     45,     42,     40,  // 96  - 103    98
       38,     36,     34,     32,     30,     28,     27,     25,  // 104 - 111    110
       24,     22,     21,     20,     19,     18,     17,     16,  // 112 - 119
       15,     14,     13,     13,     12,     11,     11,     10,  // 120 - 127    122
        0 };                                                        // off



// pitch lookup table - frequencies related to midi notes (12 bit) 
//  used for notes & env-notes (except for CLOCK_LOW type)
//  used also as old envelope speed lookup table for frequencies (old LUT)

const int noteTp[] = {                                  //              C-Notes     C-Note ZX (+2)  C-Noise (-3)
    3822, 3608, 3405, 3214, 3034, 2863, 2703, 2551,     // 0   - 7      2           4               -1
    2408, 2273, 2145, 2025, 1911, 1804, 1703, 1607,     // 8   - 15     14          16              11
    1517, 1432, 1351, 1276, 1204, 1136, 1073, 1012,     // 16  - 23
     956,  902,  851,  804,  758,  716,  676,  638,     // 24  - 31     26          ..              ..
     602,  568,  536,  506,  478,  451,  426,  402,     // 32  - 39     38
     379,  358,  338,  319,  301,  284,  268,  253,     // 40  - 47
     239,  225,  213,  201,  190,  179,  169,  159,     // 48  - 55     50
     150,  142,  134,  127,  119,  113,  106,  100,     // 56  - 63     62
      95,   89,   84,   80,   75,   71,   67,   63,     // 64  - 71
      60,   56,   53,   50,   47,   45,   42,   40,     // 72  - 79     74
      38,   36,   34,   32,   30,   28,   27,   25,     // 80  - 87     86
      24,   22,   21,   20,   19,   18,   17,   16,     // 88  - 95
      15,   14,   13,   13,   12,   11,   11,   10,     // 96  - 103    98
       9,    9,    8,    8,    7,    7,    7,    6,     // 104 - 111    110
       6,    6,    5,    5,    5,    4,    4,    4,     // 112 - 119
       4,    4,    3,    3,    3,    3,    3,    2 };   // 120 - 127    122

//
// PROTOTYPES
//

//void setEnvSpeed(uint16_t, uint16_t = 0);   // e.g. 1st required, 2nd optional

//
// SETUP
//

// the setup routine runs once when you press reset:
void setup()
{
    // EEPROM
    preset              = EEPROM.read(3800);
    bank                = EEPROM.read(3801);
    masterChannel       = EEPROM.read(3802);
    boardRevision       = EEPROM.read(3803);
    clockType           = EEPROM.read(3804);
    envPeriodType       = EEPROM.read(3805);

    // validation
    if (preset > 7) preset  = 0;
    if (bank > 7)   bank    = 0;

    if (!masterChannel || masterChannel > 16) masterChannel = 1;

    if (boardRevision) { updateAy32 = updateAy32B; BDIRPin = 15; }
    else               { updateAy32 = updateAy32A; }

    pinMode(15, OUTPUT);
    DDRD = B11111100;

    // AY3
    pinMode(12, OUTPUT);
    pinMode(13, OUTPUT);
    pinMode(14, OUTPUT);

    // encoder
    digitalWrite(ENCPINA, HIGH); // enable pull-ups
    digitalWrite(ENCPINB, HIGH);

    // Matrix -
    pinMode(0, OUTPUT);
    pinMode(1, OUTPUT);
    pinMode(2, OUTPUT);
    pinMode(3, OUTPUT);
    pinMode(4, OUTPUT);
    pinMode(5, OUTPUT);
    pinMode(6, OUTPUT);
    pinMode(7, OUTPUT);

    // Matrix +
    pinMode(16, OUTPUT);
    pinMode(17, OUTPUT);
    pinMode(18, OUTPUT);
    pinMode(19, OUTPUT);
    pinMode(20, OUTPUT);
    pinMode(21, OUTPUT);
    pinMode(22, OUTPUT);
    pinMode(23, OUTPUT);

    pinMode(5, INPUT);
    digitalWrite(5, HIGH);
    pinMode(6, INPUT);
    digitalWrite(6, HIGH);
    pinMode(7, INPUT);
    digitalWrite(7, HIGH);
    pinMode(25, OUTPUT);
    pinMode(26, OUTPUT);
    pinMode(27, OUTPUT);

    // A1 - butt reads
    // A2 - butt reads
    // A3 - butt reads
    // 12 - butt read


    PORTC = 0;

    //
    // states by held buttons
    //

    PORTA |= _BV(1);  // digitalWrite (A1-25, HIGH);
    PORTA &= ~_BV(2); // digitalWrite (A2-26, LOW);
    PORTA |= _BV(3);  // digitalWrite (A3-27, HIGH);
    PORTC |= _BV(0);  // digitalWrite (16, HIGH);
    delay(1);

    // ENTER: MIDI CHANNEL SETUP
    if (!digitalRead(5)) // boot +seq btn
    {
        writeConfig = CFG_MIDICHANNEL;
        seqSetup = 0;
        selectedStep = masterChannel - 1;
    }

    // ENTER: CLOCK TYPE SETUP
    if (!digitalRead(6)) // boot +arp btn
    {
        writeConfig = CFG_CLOCKTYPE;
        seqSetup = 0;
        selectedStep = clockType;
    }

    PORTA |= _BV(1);  // digitalWrite (A1-25, HIGH);
    PORTA |= _BV(2);  // digitalWrite (A2-26, HIGH);
    PORTA &= ~_BV(3); // digitalWrite (A3-27, LOW);
    PORTC |= _BV(0);  // digitalWrite (16, HIGH);
    delay(1);

    // ENVPERIOD TYPE
    if (!digitalRead(5)) // boot +env btn
    {
        writeConfig = CFG_ENVPDTYPE;
        seqSetup = 0;
        selectedStep = envPeriodType;
    }

    // ENTER: BOARD REVISION SETUP
    if (!digitalRead(6)) // boot +voice btn
    {
        writeConfig = CFG_REVISION;
        seqSetup = 0;
        selectedStep = boardRevision;
    }

    PORTA |= _BV(1);  // digitalWrite (A1-25, HIGH);
    PORTA |= _BV(2);  // digitalWrite (A2-26, HIGH);
    PORTA |= _BV(3);  // digitalWrite (A3-27, HIGH);
    PORTC &= ~_BV(0); // digitalWrite (16, LOW);
    delay(1);

#if FACTORYRESET
    // FACTORY RECOVERY
    if (!digitalRead(5) && !digitalRead(7)) // boot +noise btn +e btn
        factoryReset(); // reinit EEPROM with factory data, shows a "F"
#endif

    //
    // initiate hardware timer
    //

    initAY3Clock();

    //
    // time-critical section start!
    //

    if      (clockType == CLOCK_ZX)     setupTimerZX();
    else if (clockType == CLOCK_ATARI)  setupTimerAtari();
    else                                setupTimer();

    // BDIR chip 2 - REVA: PD6 (14), >REVB: PD7 (15)
    digitalWrite(BDIRPin, LOW); // time-critical, depends on the parameter passed!

    //
    // time-critical section end!
    //

    digitalWrite(12, LOW); // BC1 both chips
    digitalWrite(13, LOW); // BDIR chip 1

    // reset pin
    pinMode(9, OUTPUT);
    digitalWrite(9, HIGH);

    // not required & time-critical:
    // resetAY();

    //
    // initate midi communication
    //

    Serial.begin(31250);

#if TONETEST
    // disable Matrix -
    pinMode(0, INPUT);
    pinMode(1, INPUT);
    pinMode(2, INPUT);
    pinMode(3, INPUT);
    pinMode(4, INPUT);
    pinMode(5, INPUT);
    pinMode(6, INPUT);
    pinMode(7, INPUT);

    setAY3Register(0, 0, fine); // CH A
    setAY3Register(0, 1, cors); // CH A
    setAY3Register(0, 7, 0x08); // deactivate NOISE for CH A 
    setAY3Register(0, 8, 0x0f); // VOL UP

    setAY3Register(1, 2, fine); // CH B
    setAY3Register(1, 3, cors); // CH B
    setAY3Register(1, 7, 0x10); // deactivate NOISE for CH B 
    setAY3Register(1, 9, 0x0f); // VOL UP
#endif
}

//
// LOOP
//

void loop()
{
#if TONETEST

    // TESTING
    #if TONETEST == 1
        if (cc > 3000) {
            fine -= 1;
            setAY3Register(0, 0, fine);
            setAY3Register(1, 2, fine);

            if (!fine) {

                if (cors) cors -= 1; else cors = 0x08;

                setAY3Register(0, 1, cors);
                setAY3Register(1, 3, cors);
            }
            cc = 0;
        }
    #else
        if (cc > 3000000) {
            if      (cors == 0x08) cors = 0x04;
            else if (cors == 0x04) cors = 0x02;
            else if (cors == 0x02) cors = 0x08;

            setAY3Register(0, 1, cors);
            setAY3Register(1, 3, cors);
            cc = 0;
        }
    #endif
    cc++;
#else

    //
    // SAMPLING DIVIDER
    //

    if (samplecc >= SAMPLE_CYCLE) {
        samplecc = 0;
        doSample();
    }
    samplecc++;


    //
    // PROCESS DIVIDER (CLOCK, LFO, ARP)
    //

    if (processcc >= PROCESS_CYCLE) {
        processcc = 0;
        doProcess();
    }
    processcc++;


    //
    // SEQUENCING DIVIDER
    //

    if (seqtickcc >= SEQ_CYCLES) {
        seqtickcc = 0;
        doSequencer();
    }
    seqtickcc++;


    //
    // TICK
    //

    tick();


    //
    // READ
    //

    if (!writeConfig && !selectRow) readMidi();

#endif
}