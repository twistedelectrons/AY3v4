const uint8_t apins[] = {A4, A5, A0, A7, A6};

int8_t getOctave(int value) {

    // -2, -1, 0, +1, +2: even spaced
    if      (value < 5)             return 2;
    else if (value < 1 * 256 / 3)   return 1;
    else if (value < 2 * 256 / 3)   return 0;
    else if (value < 250)           return -1;
    else                            return -2;
}

void potTickAymid()
{
    int analogTemp;

    byte pot = pottickcc;

    // prepare 3 pots in order A, B, C!!
    byte voice = 4-pot;

    switch (pot) {

        case 0:     // 1 POT - [ ENVELOPE ] - (LFO/ARP SPEED)
                    analogTemp = analogRead(A4);
                    if ((analogTemp >> 2) > (potLast[0] + 1) || 
                        (analogTemp >> 2) < (potLast[0] - 1)) {

                        // FULL SCALED

                        // scale down to 8bit
                        analogTemp >>= 2;

                        potLast[0] = analogTemp;

                        // CRUSHED SIZE

                        int8_t octave = getOctave(analogTemp);

                        for (byte chip = 0; chip < AY3CHIPS; chip++)
                            aymidState.adjustOctaveEnv[chip] = octave;

                    }
                    break;

        case 1:     // 2 POT - [ NOISE ] - (LFO/ARP DEPTH)
                    analogTemp = analogRead(A5) >> 2;
                    if (analogTemp > (potLast[1] + 1) || 
                        analogTemp < (potLast[1] - 1)) {
                        potLast[1] = analogTemp;

                        

                    }
                    break;


        case 2:     // 6 POT - [ TONE C ] - (ENV/SEQ SPEED)
        case 3:     // 5 POT - [ TONE B ] - (GLIDE)
        case 4:     // 4 POT - [ TONE A ] - (DETUNE)

                    analogTemp = analogRead(apins[pot]) >> 2;
                    if (analogTemp > (potLast[pot] + 1) || 
                        analogTemp < (potLast[pot] - 1)) {
                        potLast[pot] = analogTemp;

                        int8_t octave = getOctave(analogTemp);

                        for (byte chip = 0; chip < AY3CHIPS; chip++)
                            aymidState.adjustOctave[chip][voice] = octave;
                    }
                    break;
    }

    pottickcc++;
    if (pottickcc > 4) pottickcc = 0;
}

void doPots()
{
    analogcc++;
    if (analogcc <= ASYNC_DELAY) return; // time-critical (insync with loop)

    analogcc = 0;

    // AYMID routine
    if (aymidState.enabled) return potTickAymid();

    int analogTemp;

    switch (pottickcc) {
        case 0:     analogTemp = analogRead(A4) >> 2;
                    if (analogTemp > (potLast[0] + 1) || 
                        analogTemp < (potLast[0] - 1)) {
                        potLast[0]  = analogTemp;
                        lfoSpeed = analogTemp;
                        arpSpeed = 255 - analogTemp;
                    }
                    break;

        case 1:     analogTemp = analogRead(A5) >> 2;
                    if (analogTemp > (potLast[1] + 1) || 
                        analogTemp < (potLast[1] - 1)) {
                        potLast[1] = analogTemp;
                        lfoDepth = analogTemp;
                        arpRange = lfoDepth >> 6;
                    }
                    break;

        case 2:     analogTemp = analogRead(A7) >> 2;
                    if (analogTemp > (potLast[3] + 1) || 
                        analogTemp < (potLast[3] - 1)) {
                        potLast[3] = analogTemp;
                        glide = analogTemp;
                    }
                    break;

        case 3:     analogTemp = analogRead(A6) >> 2;
                    if (analogTemp > (potLast[4] + 1) || 
                        analogTemp < (potLast[4] - 1)) {
                        potLast[4] = analogTemp;

                        detune = analogTemp;
                        doDetune(detune);

                        if (potLast[4] > -1) {
                            if (detune < 127)   ledNumber = oldNumber;
                            else                ledNumber = chord;
                        }
                    }
                    break;

        case 4:     analogTemp = analogRead(A0);
                    if ((analogTemp >> 2) > (potLast[2] + 1) || 
                        (analogTemp >> 2) < (potLast[2] - 1)) {

                        // default (10bit log/lin mix)
                        if (envPeriodType == 0) {
                            lastEnvSpeed = analogTemp;
                            setEnvSpeed(lastEnvSpeed);
                        }

                        // scale down to 8bit
                        analogTemp >>= 2;

                        potLast[2] = analogTemp;
                        seqSpeed = 16 - (analogTemp >> 4); // 1...16

                        // alternate (old 8bit LUT)
                        if (envPeriodType == 1) {
                            lastEnvSpeedLUT = analogTemp;
                            setEnvSpeedLUT(analogTemp);
                        }
                    }
                    break;
    }

    pottickcc++;
    if (pottickcc > 4) pottickcc = 0;
}