//
//  AYMID player & remix routine 1.0
//  --------------------------------
//
//  notes:
//      - channels are referred to as voices [0, 1, 2]
//      - channels are used for addressing 1, 2, 3
//      - term "voice" differs from "synth engine" and always refers to channels by addressing 0, 1, 2
//
//
//  cautions:
//      - don't use AY3_REGISTERS 16! for 2 reasons:
//          - 8bit IO PortA, PortB are unused and it's not needed to update or initialize as sound parameters
//          - update these registers to 0 interferes with the LED matrix (CFG_CLOCKTYPE mode, LED matrix switches off)
//
//

#define POT_VALUE_TO_AYMID_LOG15(x) (rescaleLog(x >> 5, 0, 15, 0, 15))                                  // 0..511 --> 0..15 (log)

#define POT_VALUE_TO_AYMID_VOLUME(x) (POT_VALUE_TO_AYMID_LOG15(x))                                      // 0..center (0..511/1023 --> 15/31)
#define POT_VALUE_TO_AYMID_PAN_LEFT(x) (min(15, max(0, POT_VALUE_TO_AYMID_LOG15(x))))                   // 0..15 (log)
#define POT_VALUE_TO_AYMID_PAN_RIGHT(x) (min(31, max(16, 31-POT_VALUE_TO_AYMID_LOG15((511+(512-x))))))  // 16..31 (rev.log)
#define POT_VALUE_TO_AYMID_FINETUNE(x) (x >> 1)
#define POT_VALUE_TO_AYMID_FINETUNE_ENV(x) (x << 1)
#define POT_VALUE_TO_AYMID_NOISE_PERIOD(x) (x >> 5)

#define PAN_NOON        15
#define VOLUME_AY3FILE  16
#define NOISE_AY3FILE   32

union selected_AY3s_t {
    byte selection;
    struct bits {
        byte AY3 : 1;
        byte AY32 : 1;
    } b;
};

struct aymidState_t {
    bool enabled;

    bool muteVoice[AY3CHIPS][AY3VOICES]; // includes tone, noise, envelope // unused

    ChannelState overrideChannel[AY3CHIPS];
    byte overrideEnvType[AY3CHIPS];

    OverrideState overrideTone[AY3CHIPS][AY3VOICES];
    OverrideState overrideNoise[AY3CHIPS][AY3VOICES];
    OverrideState overrideEnv[AY3CHIPS][AY3VOICES];

    OverrideState overrideToneBuf[AY3CHIPS][AY3VOICES];
    OverrideState overrideNoiseBuf[AY3CHIPS][AY3VOICES];
    OverrideState overrideEnvBuf[AY3CHIPS][AY3VOICES];

    int adjustVolume[AY3CHIPS][AY3VOICES];
    int adjustVolumeBuf[AY3CHIPS][AY3VOICES];

    int adjustPan[AY3VOICES];

    int adjustOctave[AY3CHIPS][AY3VOICES];
    int adjustFine[AY3CHIPS][AY3VOICES];

    int adjustOctaveEnv[AY3CHIPS];
    int adjustOctaveEnvAlt[AY3CHIPS];
    int adjustFineEnv[AY3CHIPS];
    byte adjustNoisePeriod[AY3CHIPS];

    bool isRemixed[AY3CHIPS]; // unused

    selected_AY3s_t selectedAY3s;

    bool isCleanMode;
    bool isShiftMode;
    bool isAltMode;
    bool isCtrlMode;

    byte copyVoice;

    bool isChipSelectionMode; // unused

    byte lastAY3values[AY3_REGISTERS];

    bool incomingInd;
    byte preparedEnvType;
};

aymidState_t aymidState;


#define AY3_TONEA_PITCH_LO  0x00
#define AY3_TONEA_PITCH_HI  0x01

#define AY3_NOISE           0x06
#define AY3_MIXER           0x07

#define AY3_AMPA            0x08
#define AY3_AMPB            0x09
#define AY3_AMPC            0x0A

#define AY3_ENVELOPE_LO     0x0B
#define AY3_ENVELOPE_HI     0x0C
#define AY3_ENVTYPE         0x0D

#define POT_NOON 512

// Calculate finetune steps by semitone
// To get from one semitone to the next, you have to add a twelfth of an octave 
// to it and therefore multiply its frequency by 2^(1 ⁄ 12) = 12√2 ≈ 1.05946.
// https://mint-zirkel.de/2023/10/10/musik-papier-und-die-zwoelfte-dimension/

long calculateFinetune(long pitch, int value, int noonValue) {
    if (value != noonValue) {

        long clock;
        switch (clockType) {
            
            case CLOCK_LOW:     // PWM 500Khz
                                clock = 500000;
                                break;

            case CLOCK_ZX:      // PWM 1.777Mhz for rev.B, otherwise use default
                                if (boardRevision > BOARD_REVA) {
                                    clock = 1777777;
                                    break;
                                }

            case CLOCK_ATARI:   // PWM 2Mhz
            default:            clock = 2000000;
                                break;

            case CLOCK_ZXEXT:   // EXT 1.7734Mhz
                                clock = 1773400;
                                break;
        }

        long freq   = clock / (16 * pitch);
        float semi = 1.05946 * (noonValue - value);
        pitch = clock / (16 * (freq - semi));
    }

    return pitch;
}


/* */
void aymidInit(int chip) { // TODO

    byte first  = chip < 0 ? 0 : chip;
    byte last   = chip < 0 ? 1 : chip;

    for (byte chip = first; chip <= last; chip++) {

        for (byte i = 0; i < AY3VOICES; i++)
            aymidInitVoice(chip, i, InitState::ALL);

        aymidState.overrideChannel[chip] = ChannelState::AY3FILE;
        aymidState.overrideEnvType[chip] = 0;

        aymidState.adjustOctaveEnv[chip] = 0;
        aymidState.adjustOctaveEnvAlt[chip] = 0;
        aymidState.adjustFineEnv[chip] = POT_VALUE_TO_AYMID_FINETUNE_ENV(POT_NOON);

        aymidState.adjustNoisePeriod[chip] = NOISE_AY3FILE;

        aymidState.isRemixed[chip] = false;
        //dotSet(chip, false); // TODO...GUI update
    }

    if (chip < 0) {
        aymidState.selectedAY3s.selection = 0;
        aymidState.isChipSelectionMode = false;
    }

    aymidState.isShiftMode = false;
    aymidState.isAltMode = false;

    aymidState.copyVoice = 0;

    aymidState.incomingInd = false;
    aymidState.preparedEnvType = 0;
}

/*
 * Initialize AY3s single voice (a b c)
 */
void aymidInitVoice(int chip, byte voice, InitState init) {

    byte first  = chip < 0 ? 0 : chip;
    byte last   = chip < 0 ? 1 : chip;

    for (byte chip = first; chip <= last; chip++) {

        bool initAll        = init == InitState::ALL || init == InitState::ALLSTATES;
        bool initTone       = init == InitState::ALL || init == InitState::TONE;
        bool initNoise      = init == InitState::ALL || init == InitState::NOISE;
        bool initEnvelope   = init == InitState::ALL || init == InitState::ENVELOPE;
        bool initAmplitude  = init == InitState::ALL || init == InitState::AMP;

        if (initTone) {
            aymidState.muteVoice[chip][voice] = false;
            aymidState.overrideTone[chip][voice] = OverrideState::AY3FILE;
            aymidState.adjustOctave[chip][voice] = 0;
            aymidState.adjustFine[chip][voice] = POT_VALUE_TO_AYMID_FINETUNE(POT_NOON);
        }

        if (initNoise)
            aymidState.overrideNoise[chip][voice] = OverrideState::AY3FILE;

        if (initEnvelope)
            aymidState.overrideEnv[chip][voice] = OverrideState::AY3FILE;

        if (initAmplitude)
            aymidState.adjustVolume[chip][voice] = VOLUME_AY3FILE;

        if (initAll) {
            aymidState.overrideToneBuf[chip][voice] = OverrideState::AY3FILE;
            aymidState.overrideNoiseBuf[chip][voice] = OverrideState::AY3FILE;
            aymidState.overrideEnvBuf[chip][voice] = OverrideState::AY3FILE;
            aymidState.adjustVolumeBuf[chip][voice] = VOLUME_AY3FILE;
            aymidState.adjustPan[voice] = PAN_NOON;
        }
    }
}

/*
 * Initialize AY3s to known state, used first time entering AYMID mode
 */
void initializeAY3s() {
    // numerator (array size) / denominator (first element size) -> reg < 14bytes/1byte
    for (size_t reg = 0; reg < sizeof(aymidState.lastAY3values) / (sizeof(*aymidState.lastAY3values)); reg++) {
        aymidState.lastAY3values[reg] = (reg == AY3_MIXER) ? 0xff : 0;
    }
}

void setAY3Register(byte chip, byte address, byte data) {
    if (!chip)  AY3(address, data);     // CHIP I
    else        AY32(address, data);    // CHIP II
}

void updateLastAY3Values(int chip, byte voice, InitState init) {

    for (size_t reg = 0; reg < sizeof(aymidState.lastAY3values) / (sizeof(*aymidState.lastAY3values)); reg++) {

        // cast size_t to byte
        byte r = static_cast<byte>(reg);
        byte voiceBase = voice * 2;

        // If specific register not covered by requested init, skip it
        switch (init) {

            case InitState::ALL:
                // default
                break;

            case InitState::TONE:
                if (r != voiceBase + AY3_TONEA_PITCH_LO && r != voiceBase + AY3_TONEA_PITCH_HI &&
                    r != AY3_AMPA + voice &&
                    r != AY3_MIXER) {
                    continue;
                }
                break;

            case InitState::NOISE:
                if (r != AY3_NOISE && r != AY3_MIXER) {
                    continue;
                }
                break;

            case InitState::MIXER:
                if (r != AY3_MIXER) {
                    continue;
                }
                break;

            case InitState::AMP:
                if (r != AY3_AMPA && r != AY3_AMPB && r != AY3_AMPC) {
                    continue;
                }
                break;

            case InitState::ENVELOPE:
                if (r != AY3_ENVELOPE_LO && r != AY3_ENVELOPE_HI &&
                    r != AY3_ENVTYPE &&
                    r != AY3_MIXER) {
                    continue;
                }
                break;

            case InitState::PITCH:
                if (r != voiceBase + AY3_TONEA_PITCH_LO && r != voiceBase + AY3_TONEA_PITCH_HI) {
                    continue;
                }
                break;


            case InitState::ENVTYPE:
                if (r != AY3_ENVTYPE) {
                    continue;
                }
                break;

            case InitState::ALLSTATES:
                if (r != AY3_AMPA && r != AY3_AMPB && r != AY3_AMPC &&
                    r != AY3_MIXER) {
                    continue;
                }
                break;
        }

        sendImmediateAY3Value(chip, reg, aymidState.lastAY3values[reg]);
    }
}

void sendImmediateAY3Value(int chip, size_t reg, byte data) {
    if (chip <= 0)              setAY3Register(0, reg, data); // 1st chip
    if (chip < 0 || chip == 1)  setAY3Register(1, reg, data); // 2nd chip
}

/*
 * Restore the most important/seldom changed registers
 */
void aymidRestore(int chip) {
    aymidInit(chip);
    updateLastAY3Values(chip, 0, InitState::ALL);
}

/*
 * Restore the most important/seldom changed registers: affects voice(s) (a b c)
 */
void aymidRestoreVoice(int chip, int voice, InitState init) {

    byte first  = voice > -1 ? voice : 0;
    byte last   = voice > -1 ? voice : AY3VOICES - 1;

    for (byte voice = first; voice <= last; voice++)
        aymidInitVoice(chip, voice, init);

    if (init == InitState::ALL) {
        aymidRestoreEnvelope(chip);
        aymidRestoreNoisePeriod(chip);
    }

    updateLastAY3Values(chip, voice, init);
}

/*
 * overrides (by mode) of a voice [AY3FILE, ON, OFF]
 */
void aymidOverrideVoice(int chip, byte voice, OverrideState buttonState[][AY3VOICES], byte mode) {

    // increment example (not used):
    // buttonState[chip][voice] = static_cast<OverrideState>(static_cast<int>(buttonState[chip][voice]) + 1);

    OverrideState state = (mode == TGL_AY3FILE_ON) ? OverrideState::ON : OverrideState::OFF;

    byte first  = chip > -1 ? chip : 0;
    byte last   = chip > -1 ? chip : AY3CHIPS - 1;

    for (byte chip = first; chip <= last; chip++) {
        if (buttonState[chip][voice] == state || mode == RST_AY3FILE)
            buttonState[chip][voice] = OverrideState::AY3FILE;
        else buttonState[chip][voice] = state;
    }
}

void aymidFindInitToneOverrideOn(int chip) {

    // chip 1 validation
    byte selected = (chip > -1 && chip < AY3CHIPS) ? chip : 0;

    // search first voice with override state ON
    byte overcc = 0;
    byte cvoice = 0;
    for (byte i = 0; i < AY3VOICES; i++) {
        if (aymidState.overrideTone[selected][i] == OverrideState::ON) {
            overcc++;
            if (overcc == 1)
                cvoice = i+1;
        }
    }

    // store first found or reset
    if (overcc < 2) aymidState.copyVoice = cvoice;
}

/*
 * Restore envelope adjustments
 */
void aymidRestoreEnvelope(int chip) {

    byte first  = chip > -1 ? chip : 0;
    byte last   = chip > -1 ? chip : AY3CHIPS - 1;

    for (byte chip = first; chip <= last; chip++) {

        aymidState.overrideEnvType[chip] = 0;

        aymidState.adjustOctaveEnv[chip] = 0;
        aymidState.adjustFineEnv[chip] = POT_VALUE_TO_AYMID_FINETUNE_ENV(POT_NOON);
    }
}

/*
 * Restore noise period
 */
void aymidRestoreNoisePeriod(int chip) {

    byte first  = chip > -1 ? chip : 0;
    byte last   = chip > -1 ? chip : AY3CHIPS - 1;

    for (byte chip = first; chip <= last; chip++)
        aymidState.adjustNoisePeriod[chip] = NOISE_AY3FILE;
}

/*
 * Restore volume adjustments
 */
void aymidRestoreVolume(int chip, int voice) {

    byte first  = chip > -1 ? chip : 0;
    byte last   = chip > -1 ? chip : AY3CHIPS - 1;

    for (byte chip = first; chip <= last; chip++) {

        byte first  = voice > -1 ? voice : 0;
        byte last   = voice > -1 ? voice : AY3VOICES - 1;

        for (byte voice = first; voice <= last; voice++)
            aymidState.adjustVolume[chip][voice] = VOLUME_AY3FILE;
    }
}

/*
 * Restore pan adjustments
 */
void aymidRestorePan(int voice) {

    byte first  = voice > -1 ? voice : 0;
    byte last   = voice > -1 ? voice : AY3VOICES - 1;

    for (byte voice = first; voice <= last; voice++)
        aymidState.adjustPan[voice] = PAN_NOON;
}

/*
 * Initialize AY3 a b c init states
 */
void aymidInitState(int chip, int voice, InitState init) {

    byte first  = chip > -1 ? chip : 0;
    byte last   = chip > -1 ? chip : AY3CHIPS - 1;

    for (byte chip = first; chip <= last; chip++) {

        byte first  = voice > -1 ? voice : 0;
        byte last   = voice > -1 ? voice : AY3VOICES - 1;

        for (byte voice = first; voice <= last; voice++)
            aymidInitVoice(chip, voice, init);
    }
}

/*
 * Toggle "remix to restore" or vice versa, used when
 * pressing shift (f) button within selected row: affects a b c state
 */

bool aymidToggleState(  int chip, int voice, InitState init, 
                        OverrideState buttonState[][AY3VOICES], 
                        OverrideState buttonStateBuf[][AY3VOICES],
                        bool asXOR) {

    // detect remix state (also as return value for further purposes)
    bool isRemixed      = aymidDetectRemix(chip, voice, buttonState);
    bool isRemixedBuf   = aymidDetectRemix(chip, voice, buttonStateBuf);

    // detect: backup remix and reset
    if ((isRemixed && !asXOR) || (isRemixedBuf && asXOR)) {

        aymidCopyOverride(chip, voice, buttonState, buttonStateBuf);
        aymidInitState(chip, voice, init);

        // XOR: clear the current state, because the buffer was copied to an unremixed state!
        if (asXOR && !isRemixed) aymidOverride(chip, voice, buttonState, TGL_AY3FILE_OFF);

    // detect copy: restore copy back
    } else if (isRemixedBuf) {
        aymidCopyOverride(chip, voice, buttonStateBuf, buttonState);

    // otherwise turn off
    } else aymidOverride(chip, voice, buttonState, TGL_AY3FILE_OFF);

    // return detection
    return isRemixed;
}

/*
 * Overrides (by mode) of voices [AY3FILE, ON, OFF]
 */
void aymidOverride(int chip, int voice, OverrideState buttonState[][AY3VOICES], byte mode) {

    byte first  = voice > -1 ? voice : 0;
    byte last   = voice > -1 ? voice : AY3VOICES - 1;

    for (byte voice = first; voice <= last; voice++)
        aymidOverrideVoice(chip, voice, buttonState, mode);
}

/*
 * Detects a remix state of voices [AY3FILE, ON, OFF]
 */
bool aymidDetectRemix(int chip, int voice, OverrideState buttonState[][AY3VOICES]) {

    byte first  = chip > -1 ? chip : 0;
    byte last   = chip > -1 ? chip : AY3CHIPS - 1;

    for (byte chip = first; chip <= last; chip++) {

        byte first  = voice > -1 ? voice : 0;
        byte last   = voice > -1 ? voice : AY3VOICES - 1;

        for (byte voice = first; voice <= last; voice++)
            if (buttonState[chip][voice] != OverrideState::AY3FILE)
                return true;
    }

    return false;
}

/*
 * Buffers a state [AY3FILE, ON, OFF]
 */
void aymidCopyOverride( int chip, int voice,
                        OverrideState buttonState[][AY3VOICES],
                        OverrideState buttonStateCopy[][AY3VOICES]) {

    byte first  = chip > -1 ? chip : 0;
    byte last   = chip > -1 ? chip : AY3CHIPS - 1;

    for (byte chip = first; chip <= last; chip++) {

        byte first  = voice > -1 ? voice : 0;
        byte last   = voice > -1 ? voice : AY3VOICES - 1;

        for (byte voice = first; voice <= last; voice++)
            buttonStateCopy[chip][voice] = buttonState[chip][voice];
    }
}

/*
 * Toggles a voice value (0 / value)
 */
void aymidToggleVoiceValueGte(int chip, int voice, int voiceValue[][AY3VOICES], byte value) {
    byte first  = chip > -1 ? chip : 0;
    byte last   = chip > -1 ? chip : AY3CHIPS - 1;

    for (byte chip = first; chip <= last; chip++) {

        byte first  = voice > -1 ? voice : 0;
        byte last   = voice > -1 ? voice : AY3VOICES - 1;

        for (byte voice = first; voice <= last; voice++)
            voiceValue[chip][voice] = voiceValue[chip][voice] >= value ? 0 : value;
    }
}

/*
 * Buffers a voice value
 */
void aymidCopyVoiceValue(int chip, int voice, int voiceValue[][AY3VOICES], int voiceValueCopy[][AY3VOICES]) {

    byte first  = chip > -1 ? chip : 0;
    byte last   = chip > -1 ? chip : AY3CHIPS - 1;

    for (byte chip = first; chip <= last; chip++) {

        byte first  = voice > -1 ? voice : 0;
        byte last   = voice > -1 ? voice : AY3VOICES - 1;

        for (byte voice = first; voice <= last; voice++)
            voiceValueCopy[chip][voice] = voiceValue[chip][voice];
    }
}

/*
 * Toggle "remix to restore" and "restore to mute", used when pressing lfo/arp button
 */
void aymidToggleVolume(int chip, int voice) {

    bool isRemixed      = aymidDetectRemixVolume(chip, voice, aymidState.adjustVolume);
    bool isRemixedBuf   = aymidDetectRemixVolume(chip, voice, aymidState.adjustVolumeBuf);

    // detect: backup remix and reset
    if (isRemixed) {
        aymidCopyVoiceValue(chip, voice, aymidState.adjustVolume, aymidState.adjustVolumeBuf);
        aymidInitState(chip, voice, InitState::AMP);

    // detect copy: restore copy back
    } else if (isRemixedBuf) {
        aymidCopyVoiceValue(chip, voice, aymidState.adjustVolumeBuf, aymidState.adjustVolume);

    // otherwise toggle turn off / AY3FILE
    } else aymidToggleVoiceValueGte(chip, voice, aymidState.adjustVolume, VOLUME_AY3FILE);
}

/*
 * Detects a remix volume of voices
 */
bool aymidDetectRemixVolume(int chip, int voice, int voiceValue[][AY3VOICES]) {

    byte first  = chip > -1 ? chip : 0;
    byte last   = chip > -1 ? chip : AY3CHIPS - 1;

    for (byte chip = first; chip <= last; chip++) {

        byte first  = voice > -1 ? voice : 0;
        byte last   = voice > -1 ? voice : AY3VOICES - 1;

        for (byte voice = first; voice <= last; voice++)
            if (voiceValue[chip][voice] < VOLUME_AY3FILE)
                return true;
    }

    return false;
}

/*
 * methods that affects a b c states for tone, noise, env 
 */

/*
 * Restore the most important/seldom changed registers, used when
 * pressing shift (f) button within selected row: affects a b c
 */
void aymidRestoreTones(int chip) {      aymidInitState(     chip, -1, InitState::TONE); }
void aymidRestoreVolumes(int chip) {    aymidRestoreVolume( chip, -1); }
void aymidRestoreNoises(int chip) {     aymidInitState(     chip, -1, InitState::NOISE); }
void aymidRestoreEnvs(int chip) {       aymidInitState(     chip, -1, InitState::ENVELOPE); }

/*
 * Toggle "remix to restore" or vice versa, used when
 * pressing shift (f) button within selected row: affects a b c
 */
void aymidToggleTones(int chip, bool asXOR) {
    aymidToggleState(chip, -1, InitState::TONE, aymidState.overrideTone, aymidState.overrideToneBuf, asXOR); 
}

void aymidToggleVolumes(int chip, bool asXOR) {

    if (asXOR) {
        byte first  = chip > -1 ? chip : 0;
        byte last   = chip > -1 ? chip : AY3CHIPS - 1;

        for (byte chip = first; chip <= last; chip++)
            for (byte voice = 0; voice < AY3VOICES; voice++)
                aymidToggleVolume(chip, voice);

    } else aymidToggleVolume(chip, -1);
}

void aymidToggleNoises(int chip, bool asXOR) {
    aymidToggleState(chip, -1, InitState::NOISE, aymidState.overrideNoise, aymidState.overrideNoiseBuf, asXOR);
}

void aymidToggleEnvs(int chip, bool asXOR) {
    aymidToggleState(chip, -1, InitState::ENVELOPE, aymidState.overrideEnv, aymidState.overrideEnvBuf, asXOR);
}

/*
 * Toggle "remix to restore" and "restore to mute", used when
 * pressing ctrl (seq) button within selected col: affects a, b or c
 */
void aymidToggleVoice(int chip, byte voice, bool asXOR) {
    bool isRemixedTone, isRemixedNoise, isRemixedEnv;

    isRemixedTone   = aymidToggleState(chip, voice, InitState::TONE,    aymidState.overrideTone,    aymidState.overrideToneBuf, false);
    isRemixedNoise  = aymidToggleState(chip, voice, InitState::NOISE,   aymidState.overrideNoise,   aymidState.overrideNoiseBuf,false);
    isRemixedEnv    = aymidToggleState(chip, voice, InitState::ENVELOPE,aymidState.overrideEnv,     aymidState.overrideEnvBuf,  false);        

    // detected any remix state should reset (toggles complete restore/mute)
    if (!asXOR && (isRemixedTone || isRemixedNoise || isRemixedEnv))
        aymidRestoreVoice(chip, voice, InitState::ALLSTATES);
    else updateLastAY3Values(chip, voice, InitState::ALLSTATES);
}

/*
 * Restore envType
 */
void aymidRestoreEnvType(int chip) {

    byte first  = chip > -1 ? chip : 0;
    byte last   = chip > -1 ? chip : AY3CHIPS - 1;

    for (byte chip = first; chip <= last; chip++)
        aymidState.overrideEnvType[chip] = 0;

    updateLastAY3Values(chip, 0, InitState::ENVTYPE);
}

/*
 * Override envType up/down dir
 */
void aymidOverrideEnvType(int chip, int8_t dir, bool prepare) {

    // chip or first of both
    byte chipIdx = chip == -1 ? 0 : chip;

    byte envType = prepare && aymidState.preparedEnvType
                            ? aymidState.preparedEnvType
                            : aymidState.overrideEnvType[chipIdx];

    if (!envType) envType = oldNumber;

    envType += dir;

    if      (envType < 1) envType = 1;
    else if (envType > 8) envType = 8;

    byte first  = chip > -1 ? chip : 0;
    byte last   = chip > -1 ? chip : AY3CHIPS - 1;

    for (byte chip = first; chip <= last; chip++) {

        if (prepare) aymidState.preparedEnvType = envType;
        else {
            aymidState.overrideEnvType[chip] = envType;
            sendImmediateEnvType(chip);
        }
    }
}

/*
 * Send direct EnvType
 */
void sendImmediateEnvType(int chip) {
    if (aymidState.isCleanMode) return;

    if (aymidState.preparedEnvType) {
        aymidState.overrideEnvType[0] = aymidState.preparedEnvType;
        aymidState.overrideEnvType[1] = aymidState.preparedEnvType;
        aymidState.preparedEnvType = 0;
    }

    byte first  = chip > -1 ? chip : 0;
    byte last   = chip > -1 ? chip : AY3CHIPS - 1;

    for (byte chip = first; chip <= last; chip++) {

        // fill data
        byte data;
        runEnvType(chip, 0, &data);

        // send out data
        sendImmediateAY3Value(chip, AY3_ENVTYPE, data);
    }
}

/*
 * Restore all finetunes except noise (TONE, ENV)
 */
void aymidRestoreFinetunes(int chip, int voice) {

    byte first  = chip > -1 ? chip : 0;
    byte last   = chip > -1 ? chip : AY3CHIPS - 1;

    for (byte chip = first; chip <= last; chip++) {

        byte first  = voice > -1 ? voice : 0;
        byte last   = voice > -1 ? voice : AY3VOICES - 1;

        for (byte voice = first; voice <= last; voice++)
            aymidState.adjustFine[chip][voice] = POT_VALUE_TO_AYMID_FINETUNE(POT_NOON);

        aymidState.adjustFineEnv[chip] = POT_VALUE_TO_AYMID_FINETUNE_ENV(POT_NOON);
    }
}

/*
 * Adjust the volume according to remix-parameter
 */
void aymidUpdateVolume(int chip, int voice) {
    if (aymidState.isCleanMode) return;

    byte first  = chip > -1 ? chip : 0;
    byte last   = chip > -1 ? chip : AY3CHIPS - 1;

    for (byte chip = first; chip <= last; chip++) {

        byte first  = voice > -1 ? voice : 0;
        byte last   = voice > -1 ? voice : AY3VOICES - 1;

        for (byte voice = first; voice <= last; voice++) {

            byte reg = AY3_AMPA + voice;

            // Save currently used env enable flag (used by same register)
            byte data = (chip == first ? ay3Reg1Last[reg] : ay3Reg2Last[reg]) & 0x10;

            // Calc new adjusted volume
            int volume = aymidState.adjustVolume[chip][voice];

            // Works in a range of 0..x limit to 16 (NOON)
            volume = volume > VOLUME_AY3FILE ? VOLUME_AY3FILE : volume;
            volume = (aymidState.lastAY3values[reg] & 0x0f) + volume - VOLUME_AY3FILE;

            // Pan
            byte pan = aymidState.adjustPan[voice];
            if (pan < PAN_NOON && chip == 1) volume -= PAN_NOON - pan;
            if (pan > PAN_NOON && chip == 0) volume += PAN_NOON - pan;

            volume = max(0, min(15, volume));
            data |= volume;

            // send out data
            sendImmediateAY3Value(chip, reg, data);
        }
    }
}

void aymidUpdateEnvelopePitch(bool pitchUpdateEnvelope, byte maskBytes[], byte dataBytes[], byte chip) {
    if (pitchUpdateEnvelope) {

        long pitch  = 0;
        bool found  = false;

        // use alternate tuning?
        if (!aymidState.isCleanMode) {

            ///////////////////
            // PITCH SHIFTER
            ///////////////////

            for (byte i = 0; i < AY3VOICES; i++) {

                // found voice with override state ON
                if (aymidState.overrideEnv[chip][i] == OverrideState::ON) {

                    // is envelope not activated?
                    if (!(aymidState.lastAY3values[AY3_AMPA+i] & 0x10)) {

                        // validate tone of same voice
                        byte toneHI = aymidState.lastAY3values[AY3_TONEA_PITCH_HI + 2 * i];
                        byte toneLO = aymidState.lastAY3values[AY3_TONEA_PITCH_LO + 2 * i];
                        byte toneEn = !(aymidState.lastAY3values[AY3_MIXER] & B00000001 << i);

                        // found tone of same voice enabled
                        if (toneEn && (toneHI || toneLO)) {
                            pitch = (toneHI << 8) + toneLO;
                        
                        } else {

                            // search next tone pitches (last resets)
                            for (byte v = i < AY3VOICES-1 ? i : 0; v < AY3VOICES; v++) {
                                toneHI = aymidState.lastAY3values[AY3_TONEA_PITCH_HI + 2 * v];
                                toneLO = aymidState.lastAY3values[AY3_TONEA_PITCH_LO + 2 * v];
                                toneEn = !(aymidState.lastAY3values[AY3_MIXER] & B00000001 << v);

                                // found tone of next voice enabled
                                if (toneEn && (toneHI || toneLO)) {
                                    pitch = (toneHI << 8) + toneLO;
                                    break;
                                }
                            }
                        }
                    }
                }

                if (pitch) {
                    found = true;
                    break;
                }
            }
        }

        // Current original pitch
        if (!pitch) pitch = (   aymidState.lastAY3values[AY3_ENVELOPE_HI] << 8) +
                                aymidState.lastAY3values[AY3_ENVELOPE_LO];

        // calculate finetune
        if (!aymidState.isCleanMode) 
            pitch = calculateFinetune(pitch, aymidState.adjustFineEnv[chip], POT_VALUE_TO_AYMID_FINETUNE_ENV(POT_NOON));

        // Change octave if not a pure noise playing
        if (!aymidState.isCleanMode) {

            // Calculate octave switch (-2, -1, 0, +1, +2) (found alternative? use alternate value)
            int adjustOctaveEnv = found ? aymidState.adjustOctaveEnvAlt[chip] : aymidState.adjustOctaveEnv[chip];

            if      (adjustOctaveEnv > 0)  pitch <<=   adjustOctaveEnv;
            else if (adjustOctaveEnv < 0)  pitch >>= (-adjustOctaveEnv);
        }

        // Scale down to even octave if pitch is higher than possible
        while (pitch > 65535) pitch >>= 1;

        // Store in AYMID structure (2 bytes per voice)
        dataBytes[AY3_ENVELOPE_LO] = pitch & 0xff;
        dataBytes[AY3_ENVELOPE_HI] = (pitch >> 8) & 0xff;

        // Set mask bits accordingly per voice
        maskBytes[1] |= 0x30;
    }
}

void aymidUpdatePitch(bool pitchUpdate[], byte maskBytes[], byte dataBytes[], byte chip) {
    for (byte i = 0; i < AY3VOICES; i++) {
        if (pitchUpdate[i]) {

            long pitch  = 0;

            // use alternate tuning?
            if (!aymidState.isCleanMode) {

                ///////////////////
                // VOICE SNATCHER
                ///////////////////

                // found voice with override state ON
                if (aymidState.overrideTone[chip][i] == OverrideState::ON) {

                    // which voice was selected first?
                    if (!aymidState.copyVoice || (aymidState.copyVoice == i+1)) {

                        // search all override ON tones (linked)
                        for (byte v = 0; v < AY3VOICES; v++) {

                            // ignore identical voice
                            if (v == i) continue;

                            // found voice with override state ON
                            if (aymidState.overrideTone[chip][v] == OverrideState::ON) {

                                byte toneHI = aymidState.lastAY3values[AY3_TONEA_PITCH_HI + 2 * i];
                                byte toneLO = aymidState.lastAY3values[AY3_TONEA_PITCH_LO + 2 * i];

                                // duplicate current pitch to target
                                if (toneHI || toneLO) {
                                    dataBytes[AY3_TONEA_PITCH_LO + 2 * v] = toneLO;
                                    dataBytes[AY3_TONEA_PITCH_HI + 2 * v] = toneHI;

                                    // copy current enable state to target (but twisted!)
                                    if ((aymidState.lastAY3values[AY3_MIXER] & B00000001 << i))
                                        dataBytes[AY3_MIXER] &= ~(B00000001 << v);  // enable
                                    else dataBytes[AY3_MIXER] |=  B00000001 << v;   // disable

                                    // set mask for target
                                    switch (v) {
                                        case 0: maskBytes[0] |= 0x03; break;
                                        case 1: maskBytes[0] |= 0x0c; break;
                                        case 2: maskBytes[0] |= 0x30; break;
                                    }               
                                }    
                            }
                        }
                    }
                }
            }

            // Current original pitch
            if (!pitch) pitch = (   aymidState.lastAY3values[AY3_TONEA_PITCH_HI + 2 * i] << 8) +
                                    aymidState.lastAY3values[AY3_TONEA_PITCH_LO + 2 * i];

            // calculate finetune
            if (!aymidState.isCleanMode) 
                pitch = calculateFinetune(pitch, aymidState.adjustFine[chip][i], POT_VALUE_TO_AYMID_FINETUNE(POT_NOON));

            // Change octave if not a pure noise playing
            if (!aymidState.isCleanMode) {

                // Calculate octave switch (-2, -1, 0, +1, +2)
                if      (aymidState.adjustOctave[chip][i] > 0)  pitch <<=   aymidState.adjustOctave[chip][i];
                else if (aymidState.adjustOctave[chip][i] < 0)  pitch >>= (-aymidState.adjustOctave[chip][i]);
            }

            // Scale down to even octave if pitch is higher than possible
            while (pitch > 4096) pitch >>= 1;

            // Store in AYMID structure (2 bytes per voice)
            dataBytes[AY3_TONEA_PITCH_LO + 2 * i] = pitch & 0xff;
            dataBytes[AY3_TONEA_PITCH_HI + 2 * i] = (pitch >> 8) & 0x0f;

            // Set mask bits accordingly per voice
            switch (i) {
                case 0: maskBytes[0] |= 0x03; break;
                case 1: maskBytes[0] |= 0x0c; break;
                case 2: maskBytes[0] |= 0x30; break;
            }
        }
    }
}

bool runNoise(byte chip, byte voice, byte* data) {

    // Noise (limit to 32)
    byte noise = aymidState.adjustNoisePeriod[chip];
    if (noise < NOISE_AY3FILE) {
        noise = NOISE_AY3FILE - noise;
        noise = max(0, min(31, noise));
        *data = noise;
    }

    return true;
}

bool runMixer(byte chip, byte voice, byte* data) {

    for (voice = 0; voice < AY3VOICES; voice++) {

        // Mute voice
        if (aymidState.muteVoice[chip][voice]) {
            *data |= B00001001 << voice;

        } else {

            // Voice
            if (aymidState.overrideTone[chip][voice] == OverrideState::ON) {
                // ignored, this is used for "voice linker" in pitch routine
                //*data &= ~( B00000001 << voice);
            } else if (aymidState.overrideTone[chip][voice] == OverrideState::OFF) {
                *data |=    B00000001 << voice;
            }

            // Noise
            if (aymidState.overrideNoise[chip][voice] == OverrideState::ON) {
                *data &= ~( B00000001 << (voice+3));
            } else if (aymidState.overrideNoise[chip][voice] == OverrideState::OFF) {
                *data |=    B00000001 << (voice+3);
            }
        }
    }

    return true;
}

bool runAmp(byte chip, byte voice, byte* data) {

    // Volume (limit to 16)
    int volume = aymidState.adjustVolume[chip][voice];

    // Works in a range of 0..x limit to 16 (NOON)
    volume = volume > VOLUME_AY3FILE ? VOLUME_AY3FILE : volume;
    volume = (*data & 0x0f) + volume - VOLUME_AY3FILE;

    // Pan
    byte pan = aymidState.adjustPan[voice];
    if (pan < PAN_NOON && chip == 1)  volume -= PAN_NOON - pan;
    if (pan > PAN_NOON && chip == 0)  volume += PAN_NOON - pan;

    volume = max(0, min(15, volume));
    *data = (*data & 0xf0) | volume;

    // Env
    if (aymidState.overrideEnv[chip][voice] == OverrideState::ON) {
        *data |=    B00010000;
    } else if (aymidState.overrideEnv[chip][voice] == OverrideState::OFF) {
        *data &= ~( B00010000);
    }

    return true;
}

bool runEnvType(byte chip, byte voice, byte* data) {

    // EnvShapes
    switch (aymidState.overrideEnvType[chip]) {
        case 1: *data = B00000000; break;
        case 2: *data = B00000100; break;
        case 3: *data = B00001000; break;
        case 4: *data = B00001010; break;
        case 5: *data = B00001011; break;
        case 6: *data = B00001100; break;
        case 7: *data = B00001101; break;
        case 8: *data = B00001110; break;
        default: break;
    }

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

        {0, NULL, true}, {0, NULL, true},   // Envelope Period

        {0, runEnvType, false},             // Env shape
    };

    byte reg = 0;           // aymid register location in buffer (no mapping is needed)
    byte dataIdx = 2 + 2;   // data location in buffer (masks, msbs first)
    byte data, field, chip;

    bool pitchUpdate[AY3VOICES];
    bool pitchUpdateEnvelope;

    // enable if not running
    if (!aymidState.enabled) {
        copyAndResetDisplay();
        aymidState.enabled = true;
        saveRequest = false;
        loadRequest = false;
        seqSetup = NONE;

        initializeAY3s();
        aymidRestore(-1);
        pressedRow = 1;
    }

    //
    // ANY DISPLAY UPDATE FLAGS HERE
    //

    // toggle incoming indicator
    aymidState.incomingInd = !aymidState.incomingInd;


    // Default - no pitches received
    for (byte i = 0; i < AY3VOICES; i++)
        pitchUpdate[i] = false;

    // Clean out mask bytes to prepare for new values per chip
    for (chip = 0; chip < AY3CHIPS; chip++)
        for (byte z = 0; z < 2; z++)
            newMaskBytes[chip][z] = 0;

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

                        if (reg < AY3_ENVELOPE_LO)  pitchUpdate[regConf->voice] = true;
                        else                        pitchUpdateEnvelope = true;

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
        aymidUpdateEnvelopePitch(pitchUpdateEnvelope, newMaskBytes[chip], newAY3Data[chip], chip);
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
}

/*
 * Visualize everything on LEDs
 *
 * Will only actually show things if CPU load is low,
 * to not drop frames.
 */
void visualizeAY3LEDs() {

    // pix mode
    if (displaycc < MAX_LEDPICCOUNT) return;

    byte chip = 0, data, amp[AY3VOICES], env[AY3VOICES], vol[AY3VOICES];

    // Visualize everything - done after sound is produced to maximize good timing.
    // Initially, only display the data from the first chip (by default), 
    // as both chips are mirrored, with the sole exception being when the 
    // voices for the second chip are swapped or other diffs need to be made visible.

    // When most of the MIDI buffer is empty it is safe to spend time on updating LEDs
    if (Serial.available() < 4) {

        //
        // Get volume adjustment (limiter)
        //

        for (byte voice = 0; voice < AY3VOICES; voice++) {

            // Volume works in a range of 0..x limit to 16 (NOON)
            // Pan works in a range of 0..31 (PAN_NOON = 15)

            if (aymidState.isAltMode) {
                vol[voice] =    aymidState.adjustPan[voice] > PAN_NOON - 1 && 
                                aymidState.adjustPan[voice] < PAN_NOON + 1 ? 0 : 1;
            } else vol[voice] = aymidState.adjustVolume[chip][voice] >= VOLUME_AY3FILE ? 0 : 1;
        }

        //
        // Get the latest used AY3 register data
        //

        // GET AMP & ENV for A, B, C
        data = ay3Reg1Last[AY3_AMPA];
        amp[0] = data & 0x0f;
        env[0] = data & 0x10;

        data = ay3Reg1Last[AY3_AMPB];
        amp[1] = data & 0x0f;
        env[1] = data & 0x10;

        data = ay3Reg1Last[AY3_AMPC];
        amp[2] = data & 0x0f;
        env[2] = data & 0x10;

        // GET MIXER
        data = ay3Reg1Last[AY3_MIXER];

        // CHECK VOICES
        for (byte channel = 1; channel <= AY3VOICES; channel++) {

            byte i = channel-1;                     // channel idx
            byte t = (data & (1 <<  i))     == 0;   // mixer tone flag
            byte n = (data & (1 << (i+3)))  == 0;   // mixer noise flag

            if (amp[i] && t)    setPoint(1, channel);
            else                clrPoint(1, channel);

            if (vol[i])         setPoint(2, channel);
            else                clrPoint(2, channel);

            if (n)              setPoint(3, channel);
            else                clrPoint(3, channel);

            if (env[i])         setPoint(4, channel);
            else                clrPoint(4, channel);
        }

        if (aymidState.preparedEnvType) ledNumber = aymidState.preparedEnvType;
        else {

            // GET ENV SHAPE
            data = ay3Reg1Last[AY3_ENVTYPE];

            if (data == B0000) ledNumber = 1;
            if (data == B0100) ledNumber = 2;
            if (data == B1000) ledNumber = 3;
            if (data == B1010) ledNumber = 4;
            if (data == B1011) ledNumber = 5;
            if (data == B1100) ledNumber = 6;
            if (data == B1101) ledNumber = 7;
            if (data == B1110) ledNumber = 8;
        }

        // cache previous envtype
        if (aymidState.overrideEnvType[chip] == 0)
            oldNumber = ledNumber;
    }
}

void aymidProcessMessage(const byte* buffer, unsigned int size) {

    switch (buffer[2]) {
        case 0x4c:
            // Start play
            if (!aymidState.enabled) {
                copyAndResetDisplay();
                aymidState.enabled = true;
                saveRequest = false;
                loadRequest = false;
                seqSetup = NONE;
                
                initializeAY3s();
                aymidRestore(-1);
            }
            if (!pressedRow) pressedRow = 1;
            break;

        case 0x4d:
            // Stop playback
            aymidState.incomingInd = true; // OFF
            aymidResetAY3Chip(-1);
            break;

        case 0x4f:
            // Display data on LCD
            // Not implemented.
            break;

        case 0x4e:
    
            // Must be at least one 3 header (ID), 2 mask bytes and 2 msb bytes and 1 data byte, end flag [8]
            if (size < 9) break;

            // reject rejection
            encPressed = false;

            // handle with skipped header
            handleAymidFrameUpdate(&buffer[3]);

            // Visualize changes - done after sound is produced to maximize good timing.
            visualizeAY3LEDs();
            break;
    }
}

/*
 * Mute the AY3 chip by clearing all bits
 */
void aymidResetAY3Chip(int chip) {

    // clear bits
    initializeAY3s();

    // clear cache
    updateLastAY3Values(chip, 0, InitState::ALL);

    // clear display
    resetDisplay();
}