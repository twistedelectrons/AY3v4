//
// ENVELOPE
//

void setEnvSpeed(uint16_t value)
{
    // NOTE OFFSET MODE
    if (envMode == 1) return;

    // FULL RANGE MODE

    // limit to a range of 0..1023
    if (value > 1023) value = 1023;

    //
    // SPLIT BEHAVIOR
    //
    //      1/3: logarithmically (up-)scaled
    //      2/3: linear downscaled (/2) to highest (limited to the end)
    //

    // ZERO --> MAP LOGARITHMIC --> 1/3
    if (value < 342) value = rescaleLog(value, 0, 341, 0, 65190);   // logarithmically (up-)scaled

    // 2/3 --> LINEAR STEPS TO MAX --> HIGHEST
    else value = map(value, 342, 1023, 65191, 65533);               // downscaled >> 1 & cutted end


    // reverse interpretor
    value = 65535 - value;

    AY3(11, value);
    AY3(12, value >> 8);

    AY32(11, value);
    AY32(12, value >> 8);
}

void setEnvSpeedLUT(byte speed)
{
    speed = map(speed, 0, 255, 0, 120);

#if ZXTUNING
    // zx (+2 halftones)
    if (clockType == CLOCK_ZX) speed += 2;
#endif

    int value = noteTp[speed] << 4;

#if ZXTUNING
    // zx finetune correction (+9 freq down)
    if (clockType == CLOCK_ZX) value += 9;
#endif

    AY3(11, value);
    AY3(12, value >> 8);

    AY32(11, value);
    AY32(12, value >> 8);
}

void updateEnvSpeed()
{
    if      (envPeriodType == 0) setEnvSpeed(lastEnvSpeed);
    else if (envPeriodType == 1) setEnvSpeedLUT(lastEnvSpeedLUT);
}

void stopEnvSpeed()
{
    AY3( 13, B0100);
    AY32(13, B0100);
}

void triggerEnv()
{
    AY3( 13, envShape[0]);
    AY32(13, envShape[1]);
    ay3Reg1Last[13] = envShape[0] + 1;
    ay3Reg2Last[13] = envShape[1] + 1;
}

void setVolEnvelopes()
{
    // update amplitude channel 1..6 (a, b, c, d, e, f)
    for (byte channel = 1; channel < 7; channel++) {

        // volume/envelope update
        if (bitRead(ledMatrix[4], channel-1) == 1)
            setChannelAmplitude(channel, vol[channel], true);
        else setChannelAmplitude(channel, vol[channel], false);
    }
}


//
// AMPLITUDE
//

void setChannelAmplitude(byte channel, byte amplitude, bool useEnvelope)
{
    // ay3 function pointer
    void  (*AYfunc)(byte, byte);
    byte reg;

    switch (channel) {
        default:
        case 1: AYfunc = AY3;   reg = 8;    break;
        case 2: AYfunc = AY3;   reg = 9;    break;
        case 3: AYfunc = AY3;   reg = 10;   break;
        case 4: AYfunc = AY32;  reg = 8;    break;
        case 5: AYfunc = AY32;  reg = 9;    break;
        case 6: AYfunc = AY32;  reg = 10;   break;
    }

    AYfunc(reg, (amplitude & 0xf) | (useEnvelope != true ? 0 : B00010000));
}


//
// PITCH
//

void setChannelPitch(byte channel, int pitch)
{
    // ay3 function pointer
    void  (*AYfunc)(byte, byte);

    // select chip
    AYfunc = channel > 3 ? AY32 : AY3;

    // tone reg: 0x00, 0x02, 0x04 for channel A B C each chip
    byte reg = ((channel-1) % 3) << 1;

    AYfunc(reg,    pitch & 0xff);          // tone   addr a,b,c (d,e,f): 0x00, 0x02, 0x04
    AYfunc(reg+1, (pitch >> 8) & 0x0f);    // coarse addr a,b,c (d,e,f): 0x01, 0x03, 0x05
}

void updatePitches()
{
    // update tone channel 1..6 (a, b, c, d, e, f)
    for (byte channel = 1; channel < 7; channel++) {

        // pitch update
        if (pitchLast[channel] != pitch[channel]) {
            pitchLast[channel] = pitch[channel];
            setChannelPitch(channel, pitch[channel]);
        }
    }
}

void assignChannelPitch(byte channel)
{
    if (base[channel]) pitch[channel] = destiPitch[channel];
}

int calculatePitch(byte channel, PitchType ptype)
{
    bool useEnvelope    = ptype == PitchType::ENVELOPE;
    bool useNoise       = ptype == PitchType::NOISE;

    int temp;

    // note / env-note (no tune, no offset)
    if (useEnvelope || useNoise)    temp = base[channel];
    else                            temp = base[channel] + (100 - tune[channel]) + offsetNote[channel];

    // lower the noise (1 oct), 
    // substract current (max 31) noisefreq steps for each chip, and -3 halftones correction!
    if (useNoise) temp -= 12 + (channel < 4 ? noiseFreq[0] : noiseFreq[1]) -3;

    // clock frequency correction
    if (clockType != CLOCK_LOW) temp--; // -= HIGH CLOCK ADAPTIONS =-
                                        // -1, because it shifts down a halftone for ~2MHz vs 500Khz
#if CLOCK_LOW_EMU
    else temp -= useEnvelope ? 48 : 24; // -= LOW CLOCK ADAPTIONS =-
                                        // -24 corresponding      to version 3.4 (notes)
#else                                   // -48 corresponding apx. to version 3.4 (env-notes)
    else temp += 24;                    // +24 LOW clock note adaptation (500Khz tuned to 2Mhz)
#endif


#if ZXTUNING
    // zx (+2 halftones)
    if (clockType == CLOCK_ZX || clockType == CLOCK_ZXEXT) temp += 2;
#endif


    // env note offset (-/+) center = 0
    if (useEnvelope) temp += (lastEnvSpeed >> 3) - 64;


    // in range
    if      (temp > 127)    temp = 127;
    else if (temp < 0)      temp = 0;


#if CLOCK_LOW_EMU                       // -72 corresponding apx. nearest octave to version 3.4 (env-notes of envTp)
    int result = (useEnvelope && clockType == CLOCK_LOW) ? envTp[temp] : noteTp[temp];
#else
    int result = noteTp[temp];
#endif


#if ZXTUNING
    // zx finetune correction (-1 freq for lower notes than D6 0x56)
    if ((clockType == CLOCK_ZX || clockType == CLOCK_ZXEXT) && temp <= 76) result -= 1;
#endif

    return result;
}

void preparePitches()
{
    // update pitch channel 1..6 (a, b, c, d, e, f)
    for (byte channel = 1; channel < 7; channel++) {

        // sequencer offset
        if (bitRead(ledMatrix[5], channel-1) && held[MASTER] > 0)
            base[channel] = seq + (key - 60);

        if (base[channel] != 0) {

            destiPitch[channel] = calculatePitch(channel, PitchType::TONE);

            // envMode 1 and first channels of chip 1 & 2
            if (envPeriodType == 0 && envMode == 1 && (channel == 1 || channel == 4)) {
                int envValue = calculatePitch(channel, PitchType::ENVELOPE);

                // channel a
                if (channel == 1) {
                    AY3(11, envValue);
                    AY3(12, envValue >> 8);

                // channel d
                } else if (channel == 4) {
                    AY32(11, envValue);
                    AY32(12, envValue >> 8);
                }
            }

            // noiseMode 1 and first channels of chip 1 & 2
            if (noiseMode == 1 && (channel == 1 || channel == 4)) {
                byte noiseValue = calculatePitch(channel, PitchType::NOISE) >> 7; // downscaled to 5bit

                // channel a
                if (channel == 1)
                    AY3(6, noiseValue);

                // channel d
                else if (channel == 4)
                    AY32(6, noiseValue);
            }

            if (bitRead(ledMatrix[2], channel-1))
                destiPitch[channel] += (int)lfo;

            destiPitch[channel] += offsetFreq[channel];

        } else pitch[channel] = 0;
    }
}

//
// PITCH BEND
//

void prepareBend(int value)
{
    bender = value;

    // update bend for channel 1..6 (a, b, c, d, e, f)
    for (byte channel = 1; channel < 7; channel++) {

        // reset
        bend[channel] = 0;
        int8_t off = 0;

        // note
        int temp = base[channel] + (100 - tune[channel]) + offsetNote[channel];

#if ZXTUNING
        // zx (+2 halftones)
        if (clockType == CLOCK_ZX) temp += 2;

        // zx finetune correction (-1 freq for lower notes than D6 0x56)
        if (clockType == CLOCK_ZX && temp <= 76) off = -1;
#endif

        // high
        if (value > 64)
            bend[channel] = map(    value, 
                                    65, 
                                    127, 
                                    0, 
                                    (noteTp[temp + 2] + off) - (noteTp[temp] + off)
                                );

        // low
        if (value < 64)
            bend[channel] = -map(   value, 
                                    0, 
                                    63, 
                                    (noteTp[temp] + off) - (noteTp[temp - 2] + off),
                                    0);
    }
}


//
// ARPEGGIATOR
//

void prepareArpeggiator(byte mode, byte select)
{
    byte s = select; // MASTER, 1, 2, 3, 4, 5, 6
    bool octUp = false;
    byte value;

    // restart
    lfo = 0;

    // set range
    byte min = (s == MASTER) ? 1 : s;
    byte max = (s == MASTER) ? 6 : s;

    // mode predefines
    if (mode == ARP_UP) {
        arpStep[s]++;
        octUp = arpStep[s] > held[s];
        value = 1;

    } else if (mode == ARP_DOWN) {
        arpStep[s]--;
        octUp = arpStep[s] < 1;
        value = held[s];

    } else if (mode == ARP_UPSCALE) {
        arpStep[s]++;
        octUp = arpStep[s] > (held[s] * 2) - 2;
        value = 1;
    }

    //
    // next arp octave validation
    //

    if (octUp) {

        // next step
        arpStep[s] = value;

        // next octave
        arpOct[s]++;

        // reset octave
        if (arpOct[s] > arpRange)
            arpOct[s] = 0;
    }

    //
    // collect every channel by action
    //

    for (byte channel = min; channel <= max; channel++) {
        if (bitRead(ledMatrix[2], channel-1) == 1) {

            // random
            if (mode == ARP_RANDOM) 
                base[channel] = noteMem[s][random(held[s])] + (random(arpRange) * 12);

            // upscale (+special condition)
            else if (mode == ARP_UPSCALE && arpStep[s] > held[s]) 
                base[channel] = noteMem[s][(held[s] * 2) - arpStep[s] - 1] + (arpOct[s] * 12);

            // default
            else if (mode == ARP_UP || mode == ARP_DOWN || mode == ARP_UPSCALE)
                base[channel] = noteMem[s][arpStep[s] - 1] + (arpOct[s] * 12);
        }
    }
}


//
// LFO
//

void prepareLfo(byte mode)
{
    //
    // phase calculation (resp. clock)
    //

    // use of predefined values instead of divisions (reduce consumption).
    // uint16_t factor = (clockType == CLOCK_LOW) ? 1023 : 255;
    // float phaseTemp = lfoDepth / (float)factor * LFOMAX; // LFOMAX (2000)

    float factor;

    if (clockType == CLOCK_LOW) factor = 1.953125;  // LFOMAX (2000) / factor 1024!  (prev: 1023);
    else                        factor = 7.8125;    // LFOMAX (2000) / factor 256!   (prev: 255);

    float phaseTemp = lfoDepth * factor;


    //
    // lfo phase additions (allowed by unsync)
    //

    if (!clockSynced) {
        if (mode == LFO_SHAPE1) {

            // falling
            lfoPhase += (0.0001 + lfoSpeed * 0.00005);  // div (float)20000
            if (lfoPhase > 1) lfoPhase -= 1;

        } else if (mode == LFO_SHAPE2) {

            // raising
            lfoPhase -= (0.0001 + lfoSpeed * 0.00005);  // div (float)20000
            if (lfoPhase < 0) lfoPhase += 1;

        } else if (mode == LFO_SHAPE3) {

            // 1.5x falling
            lfoPhase += (0.0001 + lfoSpeed * 0.0001); // 10000, div (float)13333.3333
            if (lfoPhase > 1) lfoPhase -= 1;

        } else if (mode == LFO_SHAPE4) {

            // falling
            lfoPhase += (0.0001 + lfoSpeed * 0.00005);  // div (float)20000
            if (lfoPhase > 1) lfoPhase -= 1;
        }
    }


    //
    // lfo phase calculation
    //

    if (mode == LFO_SHAPE4) {
        if      (lfoPhase <= 0.2)   lfo = phaseTemp * lfoPhase * 5;
        else if (lfoPhase <= 0.4)   lfo = phaseTemp * (1 - (lfoPhase - 0.2)) * 5;
        else if (lfoPhase <= 0.6)   lfo = phaseTemp * (lfoPhase - 0.4) * 5;
        else if (lfoPhase <= 0.8)   lfo = phaseTemp * (1 - (lfoPhase - 0.6)) * 5;
        else                        lfo = phaseTemp * (lfoPhase - 0.8) * 5;

        lfo = (lfo - 0.5) * 2;

    } else lfo = (lfoPhase - 0.5) * 2 * phaseTemp;
}


//
// NOISE
//

void setNoiseFreq(int chip, byte value)
{
    // ay3 function pointer
    void  (*AYfunc)(byte, byte);

    byte first  = chip > -1 ? chip : 0;
    byte last   = chip > -1 ? chip : 1;

    for (byte chip = first; chip <= last; chip++) {

        noiseFreq[chip] = value;

        // select chip
        AYfunc = chip ? AY32 : AY3;
        AYfunc(6, value);
    }
}

void updateNoiseFreq()
{
    AY3( 6, noiseFreq[0]);
    AY32(6, noiseFreq[1]);
}


//
// MIXER
//

void setMixer(bool updateStep)
{
    for (byte chip = 0; chip <= 1; chip++) {

        byte data = B11111111;

        // channel idx
        byte i = chip ? 3 : 0;

        // TONE
        bitWrite(data, 0, !bitRead(ledMatrix[1], i+1 - 1)); // A
        bitWrite(data, 1, !bitRead(ledMatrix[1], i+2 - 1)); // B
        bitWrite(data, 2, !bitRead(ledMatrix[1], i+3 - 1)); // C

        // NOISE (ENABLED BY HELD NOTE)
        bitWrite(data, 3, held[MASTER] ? !bitRead(ledMatrix[3], i+1 - 1) : 1); // A
        bitWrite(data, 4, held[MASTER] ? !bitRead(ledMatrix[3], i+2 - 1) : 1); // B
        bitWrite(data, 5, held[MASTER] ? !bitRead(ledMatrix[3], i+3 - 1) : 1); // C

        if (updateStep) {

            // sequencer params
            if (bitRead(ledMatrix[5], i) == 1 && base[i+1] != 0) {
                bitWrite(data, 0, !seqVoice[seqStep]);
                bitWrite(data, 3, !seqNoise[seqStep]);
            }

            if (bitRead(ledMatrix[5], i+1) == 1 && base[i+2] != 0) {
                bitWrite(data, 1, !seqVoice[seqStep]);
                bitWrite(data, 4, !seqNoise[seqStep]);
            }

            if (bitRead(ledMatrix[5], i+2) == 1 && base[i+3] != 0) {
                bitWrite(data, 2, !seqVoice[seqStep]);
                bitWrite(data, 5, !seqNoise[seqStep]);
            }
        }

        if (chip) {

            // assign
            data7B = data;

            // update
            if (data7BLast != data7B) {
                data7BLast = data7B;
                AY32(7, data7B);
            }

        } else {

            // assign
            data7A = data;

            // update
            if (data7ALast != data7A) {
                data7ALast = data7A;
                AY3(7, data7A);
            }
        }
    }
}