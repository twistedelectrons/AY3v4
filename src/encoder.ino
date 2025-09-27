void doEncoder()
{
    if (encTimer == 0) {

        // Read the current state of CLK
        int currentStateCLK = digitalRead(ENCPINA);

        // If last and current state of CLK are different, then pulse occurred
        // React to only 1 state change to avoid double count
        if (currentStateCLK != lastStateCLK && currentStateCLK == 1 && lastStateCLK != -1)
        {

            // If the DT state is different than the CLK state then
            if (digitalRead(ENCPINB) != currentStateCLK) {

                // the encoder is rotating CCW so decrement
                encTimer = ENC_TIMER;
                encoderMoved(-1);

            } else {

                // Encoder is rotating CW so increment
                encTimer = ENC_TIMER;
                encoderMoved(1);
            }
        }

        // Remember last CLK state
        lastStateCLK = currentStateCLK;

    } else encTimer--;
}

void encoderMovedAymid(int8_t dir)
{
    // ENCODER SHAPE

    if (dir) aymidOverrideEnvType(-1, dir, true);
}

void encoderMoved(int8_t dir)
{
    // AYMID routine
    if (aymidState.enabled) return encoderMovedAymid(dir);

    if (seqSetup == 0) {

        // set step
        selectedStep += dir;

        // max position
        byte max = MAX_SEQSTEP;

        if (writeConfig == CFG_REVISION)    max = MAX_REVISION;
        if (writeConfig == CFG_CLOCKTYPE)   max = MAX_CLOCKTYPE;
        if (writeConfig == CFG_ENVPDTYPE)   max = MAX_ENVPDTYPE;

        if      (selectedStep > max)    selectedStep = max;
        else if (selectedStep < 0)      selectedStep = 0;

        // step position
        if (!writeConfig) ledNumber = (selectedStep % 8) + 1;

        // reset counter
        displaycc = 0;

        // show numbers
        switch (selectedStep) {

            case 0:     if (writeConfig == CFG_REVISION) {
                            ledMatrixPic[1] = B011110;
                            ledMatrixPic[2] = B110011;
                            ledMatrixPic[3] = B111111;
                            ledMatrixPic[4] = B110011;
                            ledMatrixPic[5] = B110011;
                            break;
                        }

                        ledMatrixPic[1] = B010000;
                        ledMatrixPic[2] = B010000;
                        ledMatrixPic[3] = B010000;
                        ledMatrixPic[4] = B010000;
                        ledMatrixPic[5] = B010000;                            
                        break;

            case 1:     if (writeConfig == CFG_REVISION) {
                            ledMatrixPic[1] = B011111;
                            ledMatrixPic[2] = B110011;
                            ledMatrixPic[3] = B011111;
                            ledMatrixPic[4] = B110011;
                            ledMatrixPic[5] = B011111;
                            break;
                        }

                        ledMatrixPic[1] = B111000;
                        ledMatrixPic[2] = B100000;
                        ledMatrixPic[3] = B111000;
                        ledMatrixPic[4] = B001000;
                        ledMatrixPic[5] = B111000;
                        break;

            case 2:     ledMatrixPic[1] = B111000;
                        ledMatrixPic[2] = B100000;
                        ledMatrixPic[3] = B111000;
                        ledMatrixPic[4] = B100000;
                        ledMatrixPic[5] = B111000;
                        break;

            case 3:     ledMatrixPic[1] = B101000;
                        ledMatrixPic[2] = B101000;
                        ledMatrixPic[3] = B111000;
                        ledMatrixPic[4] = B100000;
                        ledMatrixPic[5] = B100000;
                        break;

            case 4:     ledMatrixPic[1] = B111000;
                        ledMatrixPic[2] = B001000;
                        ledMatrixPic[3] = B111000;
                        ledMatrixPic[4] = B100000;
                        ledMatrixPic[5] = B111000;
                        break;

            case 5:     ledMatrixPic[1] = B111000;
                        ledMatrixPic[2] = B001000;
                        ledMatrixPic[3] = B111000;
                        ledMatrixPic[4] = B101000;
                        ledMatrixPic[5] = B111000;
                        break;

            case 6:     ledMatrixPic[1] = B111000;
                        ledMatrixPic[2] = B100000;
                        ledMatrixPic[3] = B100000;
                        ledMatrixPic[4] = B100000;
                        ledMatrixPic[5] = B100000;
                        break;

            case 7:     ledMatrixPic[1] = B111000;
                        ledMatrixPic[2] = B101000;
                        ledMatrixPic[3] = B111000;
                        ledMatrixPic[4] = B101000;
                        ledMatrixPic[5] = B111000;
                        break;

            case 8:     ledMatrixPic[1] = B111000;
                        ledMatrixPic[2] = B101000;
                        ledMatrixPic[3] = B111000;
                        ledMatrixPic[4] = B100000;
                        ledMatrixPic[5] = B111000;
                        break;

            case 9:     ledMatrixPic[1] = B111001;
                        ledMatrixPic[2] = B101001;
                        ledMatrixPic[3] = B101001;
                        ledMatrixPic[4] = B101001;
                        ledMatrixPic[5] = B111001;
                        break;

            case 10:    ledMatrixPic[1] = B010001;
                        ledMatrixPic[2] = B010001;
                        ledMatrixPic[3] = B010001;
                        ledMatrixPic[4] = B010001;
                        ledMatrixPic[5] = B010001;
                        break;

            case 11:    ledMatrixPic[1] = B111001;
                        ledMatrixPic[2] = B100001;
                        ledMatrixPic[3] = B111001;
                        ledMatrixPic[4] = B001001;
                        ledMatrixPic[5] = B111001;
                        break;

            case 12:    ledMatrixPic[1] = B111001;
                        ledMatrixPic[2] = B100001;
                        ledMatrixPic[3] = B111001;
                        ledMatrixPic[4] = B100001;
                        ledMatrixPic[5] = B111001;
                        break;

            case 13:    ledMatrixPic[1] = B101001;
                        ledMatrixPic[2] = B101001;
                        ledMatrixPic[3] = B111001;
                        ledMatrixPic[4] = B100001;
                        ledMatrixPic[5] = B100001;
                        break;

            case 14:    ledMatrixPic[1] = B111001;
                        ledMatrixPic[2] = B001001;
                        ledMatrixPic[3] = B111001;
                        ledMatrixPic[4] = B100001;
                        ledMatrixPic[5] = B111001;
                        break;

            case 15:    ledMatrixPic[1] = B111001;
                        ledMatrixPic[2] = B001001;
                        ledMatrixPic[3] = B111001;
                        ledMatrixPic[4] = B101001;
                        ledMatrixPic[5] = B111001;
                        break;
        }

        if      (writeConfig == CFG_MIDICHANNEL)    EEPROM.write(3802, selectedStep + 1);
        else if (writeConfig == CFG_REVISION)       EEPROM.write(3803, selectedStep);
        else if (writeConfig == CFG_CLOCKTYPE)      EEPROM.write(3804, selectedStep);
        else if (writeConfig == CFG_ENVPDTYPE)      EEPROM.write(3805, selectedStep);

    } else {

         // PRESET / BANK SELECTION
        if (pressedRow == 0) {

            int8_t value = (mode == 1) ? preset : bank;

            value += dir;

            if      (value > 7) value = 7;
            else if (value < 0) value = 0;

            oldNumber = ledNumber = value + 1;

            if (mode == 1)  preset  = value;
            else            bank    = value;

            // load preset
            load();
        }

        // AMPLITUDE 0..15
        if (voiceMode < VOICE_TUNING) {

            if (pressedRow == 1 && 
                pressedCol >= 1 && 
                pressedCol <= 6) {

                int8_t value = vol[pressedCol];

                value += dir;

                if      (value > 15)  value = 15;
                else if (value < 0)   value = 0;

                vol[pressedCol] = value;

                // set with envelope true/false
                if (bitRead(ledMatrix[4], pressedCol-1) == 1)
                    setChannelAmplitude(pressedCol, vol[pressedCol], true);
                else setChannelAmplitude(pressedCol, vol[pressedCol], false);

                ledNumber = 1 + (vol[pressedCol] / 2);
            }

        // TRANSPOSE 76..[100]..124     +/-24
        } else if (voiceMode == VOICE_TUNING) {

            if (pressedRow == 1 && 
                pressedCol >= 1 && 
                pressedCol <= 6) {

                byte currentTune = presetTune[pressedCol];

                currentTune -= dir;

                if (currentTune > 124)   currentTune = 124;
                if (currentTune < 76)    currentTune = 76;

                // visuals <--- negative [page 4] | [page 5] positive --->
                if      (currentTune > 100) showMatrixedNumber(191-currentTune); // page 5 (+reverse)!
                else if (currentTune < 100) showMatrixedNumber(190-currentTune); // page 4 (-reverse)!
                else                        displaycc = MAX_LEDPICCOUNT; // reset

                presetTune[pressedCol] = currentTune;

                if (!chord) tune[pressedCol] = currentTune;
            }
        }

        // LFO SHAPE 1..8
        if (pressedRow == 2) 
            ledNumber = lfoShape = ((byte)((lfoShape-1) + dir) % 8) + 1;

        // NOISE FREQ 0..31
        if (pressedRow == 3) {

            // zapping (dir 0) inside row should not 
            // change values by dual chip selection
            if (selectedChip == -1 && !dir) return;

            int8_t value = 0;

            byte first  = selectedChip < 0 ? 0 : selectedChip;
            byte last   = selectedChip < 0 ? 1 : selectedChip;

            for (byte chip = first; chip <= last; chip++) {

                // last single mode value is the initial value in dual mode
                if (chip == lastSingleChip || first == last) {

                    value = noiseFreq[chip] - dir;

                    if      (value > 31)    value = 31;
                    else if (value < 0)     value = 0;

                    ledNumber = map(value, 0, 31, 8, 1);
                    break;
                }
            }

            for (byte chip = first; chip <= last; chip++) {

                // update noise params
                noiseFreq[chip] = value;

                // update chips
                if (chip)   AY32(6, noiseFreq[1]);
                else        AY3( 6, noiseFreq[0]);
            }
        }

        // ENVELOPE SHAPE 1..8
        if (pressedRow == 4) {

            // zapping (dir 0) inside row should not 
            // change values by dual chip selection
            if (selectedChip == -1 && !dir) return;

            byte value = 0;
            byte shape = 0;

            byte first  = selectedChip < 0 ? 0 : selectedChip;
            byte last   = selectedChip < 0 ? 1 : selectedChip;

            for (byte chip = first; chip <= last; chip++) {

                // last single mode value is the initial value in dual mode
                if (chip == lastSingleChip || first == last) {

                    value = envNumber[chip] + dir;

                    if      (value > 8) value = 8;
                    else if (value < 1) value = 1;

                    ledNumber = value;

                    if      (value == 1) shape = B0000;
                    else if (value == 2) shape = B0100;
                    else if (value == 3) shape = B1000;
                    else if (value == 4) shape = B1010;
                    else if (value == 5) shape = B1011;
                    else if (value == 6) shape = B1100;
                    else if (value == 7) shape = B1101;
                    else if (value == 8) shape = B1110;
                    break;
                }
            }

            for (byte chip = first; chip <= last; chip++) {

                envNumber[chip] = value;
                envShape[chip]  = shape;
            }

            // set vol/env a..f
            setVolEnvelopes();

            // trigger envelope by held (master)
            if (held[MASTER] > 0) triggerEnv();
        }
    }
}