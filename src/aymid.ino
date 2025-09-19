//  AYMID player & remix routine 0.1
//
//  note: don't use AY3_REGISTERS 16! for 2 reasons:
// - 8bit IO PortA, PortB are unused and it's not needed to update or initialize as sound parameters
// - update these registers to 0 interferes with the LED matrix (CFG_CLOCKTYPE mode, LED matrix switches off)
//

#define AY3VOICES 3
#define AY3CHIPS 2
#define AY3_REGISTERS 14

#define POT_VALUE_TO_AYMID_ENV_PERIOD(x) (x << 6)
#define POT_VALUE_TO_AYMID_NOISE_PERIOD(x) (x)
#define POT_VALUE_TO_AYMID_FINETUNE(x) ((value >> 4) + 978) // TODO!:
// Finetune is multiplication value to be divided by 1024. This means +/-53
// cents but from center position -26 (to fix PAL C64 vs 1MHz clock)
// Thereby range = -80/+28. (i.e from 978/1024 to 1042/1024)


// AY_EMUL stereo separation (0..255) to REG (0..15) by >> 4:
//
// 255 --> 15
// 170 --> 10
//  85 --> 5
//  13 --> 1/0

// Stereo separation and channel cross talk
// POT 0.. 512..1023 - variable mix from L/R stereo sep. (15/0, 10/10, 0/15) to mono mix (10, 10, 10)
//
// VAL 15..15..10   (STEREO CHANNEL REAL)    A --> R, C --> L, limited to 10 (>POT NOON 512)
// VAL 0 .. 5..10   (STEREO CHANNEL COPY)    A --> L, C --> R, from 0 to 5 (POT NOON) to 10
// VAL 10..10..10   (MONO CHANNEL)           B --> L, B --> R, limited to 10

// but requested AMP REG changes have to calculate in ratio
// 
//  A1          B1          C1          A2          B2          C2      Mode (-/NOON/+)
//  15          10 B1*10/15  5 C2*5/15   5 A1*5/15  10 B2*10/15 15      NOON
//  14           9           5           5           9          14      
//  13           9           4           4           9          13
//  12           8           4           4           8          12
//  11           7           4           3           8          11
//  10           7           3
//   9           6           3
//   8           5           3
//   7           5           2
//   6           4           2
//   5           3           2
//   4           3           1
//   3           2           1
//   2           1           1
//   1           1           0
//   0           0           0


enum class ChannelState { AY3FILE, ABC, ACB, BAC, BCA, CAB, CBA };  // i wanna set A B/2 to the 
enum class AmpState { AY3FILE, M, L3, L2, L1, L0 };
enum class EnvTypeState { AY3FILE, CONT, ATT, ALT, HOLD };
enum class OverrideState { AY3FILE, ON, OFF };

union selected_AY3s_t {
    byte all;
    struct bits {
        byte AY3 : 1;
        byte AY32 : 1;
    } b;
};

struct aymidState_t {
    bool enabled;

    bool muteChannel[AY3CHIPS][AY3VOICES]; // includes ton, noise, envelope

    ChannelState overrideChannel[AY3CHIPS];
    EnvTypeState overrideEnvType[AY3CHIPS];

    AmpState overrideAmp[AY3CHIPS][AY3VOICES];
    OverrideState overrideTon[AY3CHIPS][AY3VOICES];
    OverrideState overrideNoise[AY3CHIPS][AY3VOICES];

    int adjustOctave[AY3CHIPS][AY3VOICES];
    int adjustFine[AY3CHIPS][AY3VOICES];

    int adjustEnvPeriod[AY3CHIPS];
    int adjustNoisePeriod[AY3CHIPS];

    bool isRemixed[AY3CHIPS];

    selected_AY3s_t selectedAY3s;

    bool isCleanMode;

/*  bool isShiftMode; */
    bool isChipSelectionMode;
/*  bool isCutoffAdjustModeScaling;
    byte displayState;
*/

    byte lastAY3values[AY3_REGISTERS];

    volatile long timer;
    volatile byte slowTimer;
};

aymidState_t aymidState;


#define AY3_TONA_PITCH_LO   0x00
#define AY3_TONA_PITCH_HI   0x01

#define AY3_NOISE           0x06
#define AY3_MIXER           0x07

#define AY3_AMPA            0x08
#define AY3_AMPB            0x09
#define AY3_AMPC            0x0A

#define AY3_ENVELOPE_LO     0x0B
#define AY3_ENVELOPE_HI     0x0C
#define AY3_ENVTYPE         0x0D

#define FINETUNE_0_CENTS 31 + 978 // corresponds to -26 cents, to simulate PAL C64 on TS 1MHz // TODO....

#define POT_NOON 512

/* */
void aymidInit(int chip) { // TODO

    byte first  = chip < 0 ? 0 : chip;
    byte last   = chip < 0 ? 1 : chip;

    for (byte chip = first; chip <= last; chip++) {

        for (byte i = 0; i < AY3VOICES; i++)
            aymidInitVoice(chip, i, InitState::ALL);

        aymidState.overrideChannel[chip] = ChannelState::AY3FILE;
        aymidState.overrideEnvType[chip] = EnvTypeState::AY3FILE;

        aymidState.adjustEnvPeriod[chip] = POT_VALUE_TO_AYMID_ENV_PERIOD(POT_NOON);
        aymidState.adjustNoisePeriod[chip] = POT_VALUE_TO_AYMID_NOISE_PERIOD(POT_NOON);

        aymidState.isRemixed[chip] = false;
        //dotSet(chip, false); // TODO...GUI update
    }

    if (chip < 0) {
        aymidState.selectedAY3s.all = 0;
        aymidState.isChipSelectionMode = false;
    }

    aymidState.isCleanMode = false;
    aymidState.slowTimer = 0;
}

/*
 * Initialize AY3s single voice
 */
void aymidInitVoice(int chip, byte voice, InitState init) {

    byte first  = chip < 0 ? 0 : chip;
    byte last   = chip < 0 ? 1 : chip;

    for (byte chip = first; chip <= last; chip++) {

        bool initTone       = init == InitState::ALL || init == InitState::TONE;
        bool initNoise      = init == InitState::ALL || init == InitState::NOISE;
        bool initAmplitude  = init == InitState::ALL || init == InitState::AMP;

        if (initTone) {
            aymidState.muteChannel[chip][voice] = false;
            aymidState.overrideTon[chip][voice] = OverrideState::AY3FILE;
            aymidState.adjustOctave[chip][voice] = 0;
            aymidState.adjustFine[chip][voice] = FINETUNE_0_CENTS;
        }

        if (initNoise)
            aymidState.overrideNoise[chip][voice] = OverrideState::AY3FILE;

        if (initAmplitude)
            aymidState.overrideAmp[chip][voice] = AmpState::AY3FILE;
    }
}

/*
 * Initialize AY3s to known state, used first time entering AYMID mode
 */
void initializeAY3s() {
    // numerator (array size) / denominator (first element size) -> reg < 14bytes/1byte
    for (size_t reg = 0; reg < sizeof(aymidState.lastAY3values) / (sizeof(*aymidState.lastAY3values)); reg++) {
        aymidState.lastAY3values[reg] = 0;
    }

    //aymidState.lastAY3values[AY3_AMPA] = 0xF;
    //aymidState.lastAY3values[AY3_AMPB] = 0xF;
    //aymidState.lastAY3values[AY3_AMPC] = 0xF;
}

void setAY3Register(byte chip, byte address, byte data) {

    // MID reduction doesn't work
    /*if (address == AY3_AMPB) {
        if (data & 0x10) 
            AY3(AY3_ENVTYPE, ay3Reg1[AY3_ENVTYPE] - 5); // reduce env
        else {
            byte ampB = data & 0xF; // filter amp data
            data &= 0x10;           // erase except 5.bit
            data |= ampB - 5;       // reduce and copy
        }
    }*/

    if (!chip) { // 1st chip

        // A B C (ignore C)
        //if (address == AY3_TONA_PITCH_LO+4 || address == AY3_TONA_PITCH_HI+4) return;

        // SYNCED
        AY3(address, data);
    
    } else { // 2nd chip

        // A B C (ignore A)
        //if (address == AY3_TONA_PITCH_LO || address == AY3_TONA_PITCH_HI) return;

        // SYNCED
        AY32(address, data);
    }
}

void updateLastAY3Values(int chip) {

    for (size_t reg = 0; reg < AY3_REGISTERS; reg++) {

        if (chip <= 0)              setAY3Register(0, reg, aymidState.lastAY3values[reg]); // 1st chip
        if (chip < 0 || chip == 1)  setAY3Register(1, reg, aymidState.lastAY3values[reg]); // 2nd chip
    }
}

/*
 * Restore the most important/seldom changed registers, used when
 * pressing RESET briefly
 */
void aymidRestore(int chip) {
    aymidInit(chip);
    updateLastAY3Values(chip);
    //showFilterModeLEDs(aymidState.lastAY3values[SID_FILTER_MODE_VOLUME]);
    //showFilterResoLEDs(aymidState.lastAY3values[SID_FILTER_RESONANCE_ROUTING]);
    //showFilterRouteLEDs(aymidState.lastAY3values[SID_FILTER_RESONANCE_ROUTING]);
    //showFilterCutoffLEDs(aymidState.lastAY3values[SID_FILTER_CUTOFF_HI]);
}

void aymidUpdatePitch(bool pitchUpdate[], byte maskBytes[], byte dataBytes[], byte chip) {
    for (byte i = 0; i < AY3VOICES; i++) {
        if (pitchUpdate[i]) {

            // Current original pitch
            long pitch = (aymidState.lastAY3values[AY3_TONA_PITCH_HI + 2 * i] << 8) +
                         aymidState.lastAY3values[AY3_TONA_PITCH_LO + 2 * i];
/*
            // Calculate fine tune, about +/- 53 cents, range -80 to 28 cents to adjust
            // for C64 PAL vs Therapchip clock differences. // TODO....
            int fine = aymidState.isCleanMode ? FINETUNE_0_CENTS : aymidState.adjustFine[chip][i];
            pitch = (pitch * fine) >> 10;

            // Change octave if not a pure noise playing
            if (!aymidState.isCleanMode && ((aymidState.lastAY3values[AY3_MIXER] & 0x3F) != (0x08 << i))) {
                // Calculate octave switch (-1, 0, +1)
                if (aymidState.adjustOctave[chip][i] > 0)
                    pitch <<= aymidState.adjustOctave[chip][i];
                else if (aymidState.adjustOctave[chip][i] < 0)
                    pitch >>= (-aymidState.adjustOctave[chip][i]);
            }

            // Scale down to even octave if pitch is higher than possible
            while (pitch > 4096) pitch >>= 1;
*/
            // Store in AYMID structure (2 bytes per voice)
            dataBytes[i * 2] = pitch & 0xff;
            dataBytes[i * 2 + 1] = (pitch >> 8) & 0x0f;

            // Set mask bits accordingly per voice
            switch (i) {
                case 0: maskBytes[0] |= 0x03; break;
                case 1: maskBytes[0] |= 0x0C; break;
                case 2: maskBytes[0] |= 0x30; break;
            }
        }
    }
}

bool runNoise(byte chip, byte, byte*) {

    return true;
}

bool runMixer(byte chip, byte, byte*) {

    return true;
}

bool runAmp(byte chip, byte voice, byte*) {

    return true;
}

bool runEnvelopeLO(byte chip, byte, byte*) {

    return true;
}

bool runEnvelopeHI(byte chip, byte, byte*) {

    return true;
}

bool runEnvType(byte chip, byte, byte*) {

    return true;
}

void handleAymidFrameUpdate(const byte* buffer) {

    // location of AY3 register within the AYMID package (identical order)
    //
    // 0x00, 0x01,  // 12bit Tone A (lo 8bit fine, hi 4Bit coarse)
    // 0x02, 0x03,  // 12bit Tone B (lo 8bit fine, hi 4Bit coarse)
    // 0x04, 0x05,  // 12bit Tone C (lo 8bit fine, hi 4Bit coarse)
    // 0x06,        //  5bit Noise (period ctrl)
    // 0x07,        //  8bit Mixer (aka 'enable') (IOB, IOA, Noise:C, B, A, Ton:C, B, A)
    // 0x08,        //  5bit Amp A (M, L3, L2, L1, L0)
    // 0x09,        //  5bit Amp B (M, L3, L2, L1, L0)
    // 0x0a,        //  5bit Amp C (M, L3, L2, L1, L0)
    // 0x0b, 0x0c,  // 16bit Envelope (lo 8bit fine, hi 8Bit coarse)
    // 0x0d         //  4bit EnvType (ake 'shape/cycle') (CONT, ATT, ALT, HOLD)
    // 0x0e,        //  8bit IO PortA (unused)
    // 0x0f         //  8bit IO PortB (unused)

    static byte newMaskBytes[AY3CHIPS][2];
    static byte newAY3Data[AY3CHIPS][AY3_REGISTERS];
    typedef bool (*runRegFunc_t)(byte chip, byte voice, byte* data);

    struct regconfig_t {
        byte voice;
        runRegFunc_t runRegFunction;
        bool isPitch;
    };

    static struct regconfig_t regConfig[] = {
        {0, NULL, true}, {0, NULL, true},   // Tone A
        {1, NULL, true}, {1, NULL, true},   // Tone B
        {2, NULL, true}, {2, NULL, true},   // Tone C

        {0, runNoise, false},               // Noise
        {0, runMixer, false},               // Mixer

        {0, runAmp, false},                 // Amp A
        {1, runAmp, false},                 // Amp B
        {2, runAmp, false},                 // Amp C

        {0, runEnvelopeLO, false},          // Env LO
        {0, runEnvelopeHI, false},          // Env HI

        {0, runEnvType, false},             // Env shape
    };

    byte reg = 0;           // aymid register location in buffer (no mapping is needed)
    byte dataIdx = 2 + 2;   // data location in buffer (masks, msbs first)
    byte data, field, chip;

    bool pitchUpdate[AY3VOICES];

    // not running? enable ..
    if (!aymidState.enabled) {
        aymidState.enabled = true;
        initializeAY3s();
        aymidRestore(-1); // TODO
    }

    // TODO - ANY DISPLAY UPDATE FLAGS

    // Default - no pitches received
    for (byte i = 0; i < AY3VOICES; i++) {
        pitchUpdate[i] = false;
    }

    // Clean out mask bytes to prepare for new values per chip
    for (chip = 0; chip < AY3CHIPS; chip++) {
        for (byte z = 0; z < 2; z++) {
            newMaskBytes[chip][z] = 0;
        }
    }

    // Build up the 8-bit data from scattered pieces
    // First comes two 7-bit mask bytes, followed by two 7-bit MSB bytes,
    // then the data according to enabled mask
    byte maskByte;
    for (maskByte = 0; maskByte < 2; maskByte++) {

        field = 0x01;
        for (byte bit = 0; bit < 7; bit++) {
            if ((buffer[maskByte] & field) == field) {
                // It is a hit. Build the complete data byte
                data = buffer[dataIdx++];
                if (buffer[maskByte + 2] & field) {
                    // MSB was set
                    data += 0x80;
                }

// debugging enable register R7 (all off)
/*if (maskByte == 1 && bit == 0) {

newMaskBytes[0][maskByte] |= field;
newAY3Data[0][reg] = B11111111;
newMaskBytes[1][maskByte] |= field;
newAY3Data[1][reg] = B11111111;

// Move to next register in AYMID message
field <<= 1;
reg++;
continue;
}*/

                assert(reg < AY3_REGISTERS);

                // Save original values for later
                aymidState.lastAY3values[reg] = data;

                // Store AY3 parameters per chip (with modifications according
                // to performance controls, if applicable)
                if (reg < AY3_REGISTERS) {
                    regconfig_t* regConf = &(regConfig[reg]);
                    if (regConf->isPitch) {
                        // No need to store pitch data as we're updating it
                        // from source later. Retune always needed due to
                        // AY3 clock differences
                        pitchUpdate[regConf->voice] = true;
                    } else if (regConf->runRegFunction) {
                        // The regular case, run a modification function and find
                        // if value should be kept. New structure per chip created.
                        for (chip = 0; chip < AY3CHIPS; chip++) {
                            bool useData = true;
                            byte modData = data;
                            if (!aymidState.isCleanMode) {
                                useData = regConf->runRegFunction(chip, regConf->voice, &modData);
                            }
                            if (useData) {
                                newMaskBytes[chip][maskByte] |= field;
                                newAY3Data[chip][reg] = modData;
                            }
                        }
                    }
                }
            }

            // Move to next register in AYMID message
            field <<= 1;
            reg++;
        }
    }

    // Recalculate pitches
    for (chip = 0; chip < AY3CHIPS; chip++) {
        aymidUpdatePitch(pitchUpdate, newMaskBytes[chip], newAY3Data[chip], chip);
    }

    // Send all updated AY3 registers
    for (chip = 0; chip < AY3CHIPS; chip++) {
        reg = 0;
        for (maskByte = 0; maskByte < 2; maskByte++) {
            field = 0x01;
            for (byte bit = 0; bit < 7; bit++) {
                if ((newMaskBytes[chip][maskByte] & field) == field) {
                    setAY3Register(chip, reg, newAY3Data[chip][reg]);
                }

                // Move to next register in AYMID struct
                field <<= 1;
                reg++;
            }
        }
    }
// TODO:
/*          // Visualize everything - done after sound is produced to maximize good timing.
    // Chip2 is shown if corresponding button held down by the user, otherwise chip1
    chip = aymidState.selectedAY3s.b.sid2 && !aymidState.selectedAY3s.b.sid1 ? 1 : 0;

    // Refresh some seldom used registers, to display when AY3 select buttons held
    if (aymidState.slowTimer == 0) {
        aymidState.slowTimer = 4; // 39Hz/4 => 100ms response time
        // Get the latest used SID register data
        newAY3Data[chip][20] = sid_chips[chip].get_current_register(SID_FILTER_RESONANCE_ROUTING);
        newAY3Data[chip][21] = sid_chips[chip].get_current_register(SID_FILTER_MODE_VOLUME);
        newAY3Data[chip][19] = sid_chips[chip].get_current_register(SID_FILTER_CUTOFF_HI);
        // Force masks on corresponding to the above
        newMaskBytes[chip][2] |= 0x60;
        newMaskBytes[chip][3] |= 0x01;
    }

    // Do the actual LED update
    reg = 0;
    for (maskByte = 0; maskByte < 2; maskByte++) {
        field = 0x01;
        for (byte bit = 0; bit < 7; bit++) {
            if ((newMaskBytes[chip][maskByte] & field) == field) {
                data = newAY3Data[chip][reg];
                if (reg == SID_FILTER_MODE_VOLUME) {
                    showFilterModeLEDs(data);
                } else if (reg == SID_FILTER_RESONANCE_ROUTING) {
                    showFilterResoLEDs(data);
                    showFilterRouteLEDs(data);
                } else if (reg == SID_FILTER_CUTOFF_HI) {
                    showFilterCutoffLEDs(data);
                } else if ((reg - SID_VC_CONTROL) % 7 == 0) {
                    showWaveformLEDs((reg - SID_VC_CONTROL) / 7, data);
                }
            }

            // Move to next register in ASID struct
            field <<= 1;
            reg++;
        }
    }
*/
}

void aymidProcessMessage(const byte* buffer, unsigned int size) {

    switch (buffer[2]) {
        case 0x4c:
            // Start play
            if (!aymidState.enabled) {
                aymidState.enabled = true;
                initializeAY3s();
                aymidRestore(-1); // TODO
            }

            break;

        case 0x4d:
            // Stop playback
            // reset registers raw, so remix state is kept
            aymidState.enabled = false;
            aymidResetAY3Chip(-1);
            break;

        case 0x4f:
            // Display data on LCD
            // Not implemented.
            break;

        case 0x4e:
    
            // Must be at least one 3 header (ID), 2 mask bytes and 2 msb bytes and 1 data byte, end flag [8]
            if (size < 9) break;

            // handle with skipped header
            handleAymidFrameUpdate(&buffer[3]);

            // LED visualization
            break;
    }
}

/* Called at 10kHz */
void aymidTick()
{
    aymidState.timer++;
    if ((byte)aymidState.timer == 0) {
        // Slow timer decreased at about 39Hz
        if (aymidState.slowTimer > 0) {
            aymidState.slowTimer--;
        }
    }
}

/*
 * Mute the AY3 chip by clearing all bits
 */
void aymidResetAY3Chip(int chip) {

    // clear bits
    initializeAY3s();

    // update chips
    updateLastAY3Values(chip);
}
