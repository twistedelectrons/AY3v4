// Arduino MIDI Library - callbacks:
// https://fortyseveneffects.github.io/arduino_midi_library/a00033.html
// https://github.com/FortySevenEffects/arduino_midi_library/wiki/Using-Callbacks

void readMidi()
{
    while (MIDI.read()) {

        // ignore other midi messages in aymid mode
        if (aymidState.enabled && MIDI.getType() != SystemExclusive) break;

        switch (MIDI.getType()) {

            case SystemExclusive:   handleSystemExclusive(MIDI.getSysExArray(), MIDI.getSysExArrayLength());    break;
            case ProgramChange:     handleProgramChange(MIDI.getChannel(), MIDI.getData1(), MIDI.getData2());   break;
            case ControlChange:     handleControlChange(MIDI.getChannel(), MIDI.getData1(), MIDI.getData2());   break;
            case NoteOn:            handleNoteOn(MIDI.getChannel(), MIDI.getData1(), MIDI.getData2());          break;
            case NoteOff:           handleNoteOff(MIDI.getChannel(), MIDI.getData1(), MIDI.getData2());         break;
            case PitchBend:         handleBend(MIDI.getChannel(), MIDI.getData2());                             break;
            case Start:             handleStart();                                                              break;
            case Continue:          handleContinue();                                                           break;
            case Stop:              handleStop();                                                               break;
            case Clock:             handleClock();                                                              break;
            default:                break;
        }
    }
}

// Handles a completed sysex buffer
void handleSystemExclusive(const byte* buffer, unsigned int size)
{
    if (size > 1) {

        // Manufacturer ID used for AYMID
        if (buffer[1] == 0x2E) {
            aymidProcessMessage(buffer, size);
        }
    }
}

void handleProgramChange(int channel, int number1, int number2)
{
    if (channel == masterChannel) {

        if (number1 < 65) {
            bank = preset = 0;

            while (number1 > 8) {
                number1 -= 8;
                bank++;
            }

            preset = number1;
            encoderMoved(0);

            // explicite load in none-default state
            // otherwise it's already handled by encoderMoved
            if (pressedRow || seqSetup) loadRequest = true;
        }
    }
}

void handleControlChange(byte channel, byte number, byte value)
{
    if (channel == masterChannel) receivedCC(number, value);
}

void handleNoteOn(byte channel, byte note, byte velocity)
{
    receivedNote(channel, note, velocity ? 127 : 0);
}

void handleNoteOff(byte channel, byte note, byte velocity)
{
    receivedNote(channel, note, 0);
}

void handleBend(byte channel, int value)
{
    prepareBend(value);
}

void handleStart()
{
    clockSynced = true;
    seqcc       = 0;
    lfoPhase    = 0;
}

void handleContinue()
{
    clockSynced = true;
}

void handleStop()
{
    clockSynced = false;
}

void handleClock()
{
    if (clockSynced) clockcc++;
}

void receivedCC(int number, int value)
{
    byte channel;
    int analogTemp;

    switch (number) {

        case 2:     // lfo/arp speed
                    analogTemp = value << 3;
                    lfoSpeed = value << 1;
                    arpSpeed = (1023 - analogTemp) >> 2;
                    break;

        case 3:     // lfo/arp depth
                    lfoDepth = value << 1;
                    arpRange = map(lfoDepth, 0, 255, 0, 6);
                    break;

        case 6:     // detune amount
                    detune = value << 1;
                    doDetune(detune);
                    if (chord > 0) ledNumber = chord;
                    break;

        case 7:     // glide amount
                    glide = value << 1;
                    break;

        case 8:     // sequencer & envelope speed
                    seqSpeed = (value >> 4) + 1; // 1...16

                    // default (10bit log/lin mix)
                    if (envPeriodType == 0) {
                        lastEnvSpeed = ((value + 1) << 3) - 1;  // 1023 (max 10bit)
                        setEnvSpeed(lastEnvSpeed);
                    } else if (envPeriodType == 1) {
                        lastEnvSpeedLUT = ((value + 1) << 1) - 1; // 255 (max 8bit)
                        setEnvSpeedLUT(lastEnvSpeedLUT);
                    }
                    break;

        case 9:     // noise frequency chip 1 & 2
                    setNoiseFreq(-1, value >> 2); // todo: original value * 8???
                    break;

        case 10:    // noise frequency chip 1
                    setNoiseFreq(0, value >> 2); // todo: original value * 8???
                    break;

        case 11:    // noise frequency chip 2
                    setNoiseFreq(1, value >> 2); // todo: original value * 8???
                    break;

        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        case 17:    // tune voice 1..6
                    channel = number-11;
                    tune[channel] = map(value, 0, 127, 124, 76);
                    break;

        case 18:
        case 19:
        case 20:
        case 21:
        case 22:
        case 23:    // volume voice 1..6
                    channel = number-17;
                    vol[channel] = map(value, 0, 127, 0, 15);

                    // set with envelope true/false
                    if (bitRead(ledMatrix[4], channel-1) == 1)
                        setChannelAmplitude(channel, vol[channel], true);
                    else setChannelAmplitude(channel, vol[channel], false);
                    break;

        case 24:
        case 25:
        case 26:
        case 27:
        case 28:
        case 29:    // enable/disable voice 1..6
                    channel = number-23;
                    bitWrite(ledMatrix[1], channel-1, value >> 6);
                    break;

        case 30:
        case 31:
        case 32:
        case 33:
        case 34:
        case 35:    // enable/disable noise 1..6
                    channel = number-29;
                    bitWrite(ledMatrix[3], channel-1, value >> 6);
                    break;

        case 36:
        case 37:
        case 38:
        case 39:
        case 40:
        case 41:    // enable/disable lfo/arp 1..6
                    channel = number-35;
                    bitWrite(ledMatrix[2], channel-1, value >> 6);
                    break;

        case 42:
        case 43:
        case 44:
        case 45:
        case 46:
        case 47:    // enable/disable envelope 1..6
                    channel = number-41;
                    bitWrite(ledMatrix[4], channel-1, value >> 6);
                    setVolEnvelopes();
                    break;

        case 48:
        case 49:
        case 50:
        case 51:
        case 52:
        case 53:    // enable/disable sequencer 1..6
                    channel = number-47;
                    bitWrite(ledMatrix[5], channel-1, value >> 6);
                    break;
    }
}

void receivedNote(byte channel, byte note, byte vel)
{
    note++;

    if (channel == masterChannel && seqSetup == 0)
        seqNote[selectedStep] = note; // in setup mode, do the note thing

    if ((masterChannel == 1 && channel == 1) ||
        (masterChannel != 1 && channel == masterChannel)) {

        if (chord == 8) {

            // polyphonic
            channel = 17;
            held[MASTER] = 0;

            // note on
            if (vel) {

                // Look for free voice
                for (byte i = 1; i < 7; i++) {
                    if (slots[i] == 0) {
                        slots[i] = note;
                        base[staggered[i]] = note;
                        triggerEnv();
                        break;
                    }
                }

            // note off
            } else {

                // look for note in slot
                for (byte i = 1; i < 7; i++) {
                    if (slots[i] == note) {
                        slots[i] = 0;
                        base[staggered[i]] = 0;
                    }
                }
            }
        }

        if (channel < 17) {

            // omni channel
            if (!vel) {

                if (held[MASTER] > 0)
                    held[MASTER]--;

                // remove note from noteMem
                for (byte i = 0; i <= held[MASTER] + 1; i++)
                    if (noteMem[MASTER][i] == note)
                        noteMem[MASTER][i] = 00;

                isort(noteMem[MASTER], held[MASTER] + 1);

                // update mixer
                processMixer(false);

                if (held[MASTER] == 0) {

                    base[1] = base[2] = base[3] = base[4] = base[5] = base[6] = 0;

                    AY3(11, 255);
                    AY3(12, 255);

                    AY32(11, 255);
                    AY32(12, 255);

                    bitWrite(data7A, 0, 1);
                    bitWrite(data7A, 1, 1);
                    bitWrite(data7A, 2, 1);
                    bitWrite(data7B, 0, 1);
                    bitWrite(data7B, 1, 1);
                    bitWrite(data7B, 2, 1);
                }

            // play
            } else {

                key = note;

                held[MASTER]++;
                triggerEnv();
                updateEnvSpeed();

                // add note to mem
                if (held[MASTER] > 19)
                    held[MASTER] = 19;

                noteMem[MASTER][held[MASTER] - 1] = note;
                isort(noteMem[MASTER], held[MASTER]);

                // retrigger mixer and restart sequencer
                retriggerMixerAndSteps();

                base[1] = base[2] = base[3] = base[4] = base[5] = base[6] = note;
                handleBend(1, bender);

                if (held[MASTER] == 1) {
                    preparePitches();

                    // update pitch channel 1..6 (a, b, c, d, e, f)
                    for (byte channel = 1; channel < 7; channel++)
                        assignChannelPitch(channel);

                    // exceed arp counter (reset)
                    arpcc = 0x0400;
                }
            }
        }
    }

    if (masterChannel == 1) {

        // voice on/off for midichannel 2..7
        if (channel >= 2 && channel <= 7) {

            // map midichannel to channels: 1..6
            byte s = channel-1; 

            // note on
            if (vel) {

                // assign
                held[s]++;

                triggerEnv();
                updateEnvSpeed();

                // limit
                if (held[s] > 19)
                    held[s] = 19;

                // add note to mem
                noteMem[s][held[s] - 1] = note;

                // sort
                isort(noteMem[s], held[s]);

                // set note & bender
                base[s] = note;
                handleBend(1, bender);

                // update pitch
                assignChannelPitch(s);

                // exceed arp counter (reset)
                if (held[s] == 1) arpcc = 0x0400;

            // note off
            } else {

                // release
                if (held[s] > 0) held[s]--;

                // remove note from noteMem
                for (byte i = 0; i <= held[s] + 1; i++)
                    if (noteMem[s][i] == note)
                        noteMem[s][i] = 00;

                // sort
                isort(noteMem[s], held[s] + 1);

                // free
                if (held[s] == 0)
                    base[s] = 0;
            }
        }

        // polyphonic, bitches!
        if (channel == 8) {

            held[MASTER] = 0;

            // note on
            if (vel) {

                // Look for free voice
                for (byte i = 1; i < 7; i++) {
                    if (slots[i] == 0) {
                        slots[i] = note;
                        base[staggered[i]] = note;
                        handleBend(1, bender);
                        triggerEnv();
                        break;
                    }
                }

            // note off
            } else {

                // look for note in slot
                for (byte i = 1; i < 7; i++) {
                    if (slots[i] == note) {
                        slots[i] = 0;
                        base[staggered[i]] = 0;
                    }
                }
            }
        }

        // noise on/off - midichannel 9..14 to channels-1
        if (channel >= 9 && channel <= 14) {

            byte i = channel-9; // channel-9: 0, 1, 2, 3, 4, 5

            if (vel)    bitWrite(ledMatrix[3], i, 1);
            else        bitWrite(ledMatrix[3], i, 0);
        }
    }
}