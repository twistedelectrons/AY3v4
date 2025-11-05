void doButtons()
{
    // two conditions ensure that neither the register update 
    // nor the selection of the selected row is disrupted, 
    // as both use some of the same data lines. 
    // selectRow ensures delayed and protected selection, 
    // PORTB checking refers to LED SUPPRESSION, which is 
    // initiated by timer before each register update.

#if LEDSUPPRESSION
    if (selectRow || PORTB == B11111111) return;
#else
    if (selectRow) return;
#endif

    // PORTB reset (LED)
    PORTB = B11111111;

    // Butt Matrix

    switch (btntickcc) {

        case 0: // Phase 1
                PORTA &= ~_BV(1); // digitalWrite (A1-25, LOW);
                PORTA |= _BV(2);  // digitalWrite (A2, HIGH);
                PORTA |= _BV(3);  // digitalWrite (A3, HIGH);
                PORTC |= _BV(0);  // digitalWrite (16, HIGH);

                butt[1] = digitalRead(5); // ENCODER SWITCH
                butt[2] = digitalRead(6); // CHANNEL: b
                butt[3] = digitalRead(7); // CHANNEL: f

                if (butt[1] != buttLast[1]) buttPressed(1, butt[1]);
                if (butt[2] != buttLast[2]) buttPressed(2, butt[2]);
                if (butt[3] != buttLast[3]) buttPressed(3, butt[3]);
                break;

        case 1: // Phase 2
                PORTA |= _BV(1);  // digitalWrite (A1-25, HIGH);
                PORTA &= ~_BV(2); // digitalWrite (A2-26, LOW);
                PORTA |= _BV(3);  // digitalWrite (A3-27, HIGH);
                PORTC |= _BV(0);  // digitalWrite (16, HIGH);

                butt[4] = digitalRead(5); // ROW 5: SEQ
                butt[5] = digitalRead(6); // ROW 2: LFO/ARP
                butt[6] = digitalRead(7); // CHANNEL: c

                if (butt[4] != buttLast[4]) buttPressed(4, butt[4]);
                if (butt[5] != buttLast[5]) buttPressed(5, butt[5]);
                if (butt[6] != buttLast[6]) buttPressed(6, butt[6]);
                break;

        case 2: // Phase 3
                PORTA |= _BV(1);  // digitalWrite (A1-25, HIGH);
                PORTA |= _BV(2);  // digitalWrite (A2-26, HIGH);
                PORTA &= ~_BV(3); // digitalWrite (A3-27, LOW);
                PORTC |= _BV(0);  // digitalWrite (16, HIGH);

                butt[7] = digitalRead(5); // ROW 4: ENV
                butt[8] = digitalRead(6); // ROW 1: VOICE
                butt[9] = digitalRead(7); // CHANNEL: d

                if (butt[7] != buttLast[7]) buttPressed(7, butt[7]);
                if (butt[8] != buttLast[8]) buttPressed(8, butt[8]);
                if (butt[9] != buttLast[9]) buttPressed(9, butt[9]);
                break;

        case 3: // Phase 4
                PORTA |= _BV(1);  // digitalWrite (A1-25, HIGH);
                PORTA |= _BV(2);  // digitalWrite (A2-26, HIGH);
                PORTA |= _BV(3);  // digitalWrite (A3-27, HIGH);
                PORTC &= ~_BV(0); // digitalWrite (16, LOW);

                butt[10] = digitalRead(5); // ROW 3: NOISE
                butt[11] = digitalRead(6); // CHANNEL: a
                butt[12] = digitalRead(7); // CHANNEL: e

                if (butt[10] != buttLast[10]) buttPressed(10, butt[10]);
                if (butt[11] != buttLast[11]) buttPressed(11, butt[11]);
                if (butt[12] != buttLast[12]) buttPressed(12, butt[12]);
                break;

    } // End buttMatrix

    btntickcc++;
    if (btntickcc > 3) btntickcc = 0;
}

byte indexOfPin(int pin) {
    switch (pin) {
        default:

        // COLS
        case 11:    return 0;
        case 2:     return 1;
        case 6:     return 2;
        case 9:     return 3;
        case 12:    return 4;
        case 3:     return 5;

        // ROWS
        case 8:     return 0;
        case 5:     return 1;
        case 10:    return 2;
        case 7:     return 3;
        case 4:     return 4;
    }
}

void buttPressedAymid(int pin, int state)
{
    byte index  = indexOfPin(pin);
    int chip    = -1; // default all
    int voice   = -1; // default all

    // find out what chips are selected
    if (aymidState.selectedAY3s.selection) {
        if      (aymidState.selectedAY3s.b.AY3 && !aymidState.selectedAY3s.b.AY32)  chip = 0;
        else if (aymidState.selectedAY3s.b.AY32 && !aymidState.selectedAY3s.b.AY3)  chip = 1;
    }

    // pressed
    if (state == 0) {

        switch (pin) {

            //
            // ENCODER SWITCH
            //

            case 1:     if (aymidState.preparedEnvType) sendImmediateEnvType(chip);
                        else                            aymidRestoreEnvType(chip);

                        encPressed = true;
                        countDown = 20;
                        break;

            //
            // MATRIX ROWS
            //

            case 8:     // ROW 1: VOICE
            case 5:     // ROW 2: LFO/ARP
            case 10:    // ROW 3: NOISE
            case 7:     // ROW 4: ENV

                        selectRow = index+1;

                        if (aymidState.isShiftMode || pressedRow == selectRow) {
                            switch (selectRow) {
                                case 1: aymidToggleTones(chip,  aymidState.isShiftMode); break;
                                case 2: aymidToggleVolumes(chip,aymidState.isShiftMode); break; 
                                case 3: aymidToggleNoises(chip, aymidState.isShiftMode); break;
                                case 4: aymidToggleEnvs(chip,   aymidState.isShiftMode); break;
                            }
                        }
                        break;

            case 4:     // ROW 5: SEQ
                        aymidState.isCtrlMode = true;
                        break;


            //
            // MATRIX COLS
            //

            case 11:    // CHANNEL: a
            case 2:     // CHANNEL: b
            case 6:     // CHANNEL: c

                        if (aymidState.isCtrlMode) aymidToggleVoice(chip, index, true);
                        else {
                            switch (pressedRow) {
                                case 1: aymidOverrideVoice(chip, index, aymidState.overrideTone, aymidState.isAltMode ? TGL_AY3FILE_ON : TGL_AY3FILE_OFF);  break;
                                case 2: aymidToggleVolume(chip, index);                                                                                     break;
                                case 3: aymidOverrideVoice(chip, index, aymidState.overrideNoise,aymidState.isAltMode ? TGL_AY3FILE_ON : TGL_AY3FILE_OFF);  break;
                                case 4: aymidOverrideVoice(chip, index, aymidState.overrideEnv,  aymidState.isAltMode ? TGL_AY3FILE_ON : TGL_AY3FILE_OFF);  break;
                            }

                            if (pressedRow == 1) aymidFindInitToneOverrideOn(chip);
                        }
                        break;

            case 9:     // CHANNEL: d
                        if (aymidState.isCtrlMode) {
                            aymidState.isCleanMode = !aymidState.isCleanMode;

                            // special case: envshape
                            if (aymidState.isCleanMode)
                                updateLastAY3Values(chip, 0, InitState::ENVTYPE);
                            else aymidOverrideEnvType(-1, 0, false);

                        } else {
                            aymidRestoreVoice(chip, voice, InitState::ALL);
                            aymidState.isCleanMode = false;
                        }
                        break;

            case 12:    // CHANNEL: e
                        break;

            case 3:     // CHANNEL: f
                        aymidState.isShiftMode = true;
                        break;
        }

    // released
    } else {

        switch (pin) {

            //
            // ENCODER SWITCH
            //

            case 1:     encPressed = false;
                        break;

            //
            // MATRIX ROWS
            //

            case 4:     // ROW 5: SEQ
                        aymidState.isCtrlMode = false;

                        // RESTORE FINETUNES
                        aymidRestoreFinetunes(chip, voice);
                        break;

            //
            // MATRIX COLS
            //

            case 12:    // CHANNEL: e
                        aymidState.isAltMode = !aymidState.isAltMode;
                        break;

            case 3:     // CHANNEL: f
                        aymidState.isShiftMode = false;
                        break;
        }
    }
}

void buttPressed(int pin, int state)
{
    buttLast[pin] = state;

    // AYMID routine
    if (aymidState.enabled) return buttPressedAymid(pin, state);

    // ignore buttons in config mode
    if (writeConfig) {

        // visuals by release
        if (state) encoderMoved(0);
        return;
    }

    // pressed
    if (state == 0) {

        // local counter
        byte counter = 0;

        // restore current matrix values
        if (displaycc < 20000) restoreMatrix(false);

        // edit sequence mode? ignore input for row 6
        if (seqSetup == EDIT && pressedRow == 6 && (
                pin == 11   || 
                pin == 2    || 
                pin == 6    || 
                pin == 9    || 
                pin == 12   || 
                pin == 3)) return; 

        switch (pin) {

            //
            // ENCODER SWITCH
            //

            case 1:     // envelope should toggle envelop speed mode
                        if (pressedRow == 3 || (pressedRow == 4 && envPeriodType == 0)) {

                            // no sequence?
                            if (seqSetup == NONE) {

                                if (displaycc >= MAX_LEDPICCOUNT) copyMatrix();

                                byte m = pressedRow == 3 ? noiseMode : envMode;

                                // toggle envelope mode [F O]
                                m = m ? 0 : 1;

                                displaycc = 0;

                                // F - Full Range Mode (default)
                                if (m == 0) {
                                    
                                    ledMatrixPic[1] = B110011;
                                    ledMatrixPic[2] = B110011;
                                    ledMatrixPic[3] = B011110;
                                    ledMatrixPic[4] = B110011;
                                    ledMatrixPic[5] = B110011;
                                }

                                // O - Note Offset Mode
                                if (m == 1) {

                                    ledMatrixPic[1] = B000110;
                                    ledMatrixPic[2] = B001100;
                                    ledMatrixPic[3] = B011000;
                                    ledMatrixPic[4] = B001100;
                                    ledMatrixPic[5] = B000110;
                                }

                                if (pressedRow == 3) {
                                    noiseMode = m;

                                    // restore last value
                                    if (m == 0) updateNoiseFreq();

                                } else {
                                    envMode = m;

                                    // restore last value
                                    if (m == 0) updateEnvSpeed();
                                }
                            }

                        // seq length
                        } else if (seqSetup == EDIT) {
                            seqMax = selectedStep;
                            if (!seqMax) seqMax = 15;
                            maincc = 2;
                        
                        } else {

                            // initialize preset
                            if (voicePressed) {

                                initPreset();

                                if (displaycc >= MAX_LEDPICCOUNT) copyMatrix();

                                displaycc = 0;

                                ledMatrixPic[1] = B111111;
                                ledMatrixPic[2] = B100001;
                                ledMatrixPic[3] = B100001;
                                ledMatrixPic[4] = B100001;
                                ledMatrixPic[5] = B111111;

                            // toggle preset / bank
                            } else if (!pressedRow) {                    

                                encPressed = true;
                                countDown = 20;
                                oldNumber = ledNumber;
                            }
                        }
                        break;

            //
            // MATRIX ROWS
            //

            case 8:     // ROW 1: VOICE
                        pressedRow = 1;

                        // initiate voicePressed state (checked after 4 seconds)
                        resetcc = millis() + 4000;
                        voicePressed = true;

                        // sequence?
                        if (seqSetup == EDIT) seqVoice[selectedStep] = !seqVoice[selectedStep];
                        else {

                            if (displaycc >= MAX_LEDPICCOUNT) copyMatrix();

                            // roll voice mode [E V T]
                            voiceMode++;
                            if (voiceMode > VOICE_TUNING) {
                                pressedRow = 0;
                                ledNumber = oldNumber;

                            } else {

                                displaycc = 0;

                                if (voiceMode == VOICE_ENABLE) {
                                    ledMatrixPic[1] = B011110;
                                    ledMatrixPic[2] = B000011;
                                    ledMatrixPic[3] = B001111;
                                    ledMatrixPic[4] = B000011;
                                    ledMatrixPic[5] = B011110;
                                }

                                if (voiceMode == VOICE_VOLUME) {
                                    ledMatrixPic[1] = B110011;
                                    ledMatrixPic[2] = B110011;
                                    ledMatrixPic[3] = B110011;
                                    ledMatrixPic[4] = B011110;
                                    ledMatrixPic[5] = B001100;
                                }

                                if (voiceMode == VOICE_TUNING) {
                                    ledMatrixPic[1] = B111111;
                                    ledMatrixPic[2] = B111111;
                                    ledMatrixPic[3] = B001100;
                                    ledMatrixPic[4] = B001100;
                                    ledMatrixPic[5] = B001100;
                                }
                            }
                        }
                        break;

            case 5:     // ROW 2: LFO/ARP

                        // no sequence? -> lfo pitch
                        if (seqSetup == NONE) pressedRow = pressedRow == 2 ? 0 : 2;
                        break;

            case 10:    // ROW 3: NOISE
            case 7:     // ROW 4: ENV

                        // RELEASE or DISPLAY CHIP
                        if ((pressedRow == 3 && pin == 10) || 
                            (pressedRow == 4 && pin == 7)) {

                            // no sequence
                            if (seqSetup == NONE) {

                                // select chip 1
                                if (selectedChip == -1) {
                                    selectedChip = 0;
                                    pressedCol = 1;

                                // select chip 2
                                } else if (selectedChip == 0) {
                                    selectedChip = 1;
                                    pressedCol = 4;

                                // release (select both chips)
                                } else {
                                    selectedChip = -1;
                                    pressedCol = 0;

                                    // goto preset
                                    pressedRow = 0;
                                    ledNumber = oldNumber;

                                    // store last single chip
                                    lastSingleChip = 0;
                                    break;
                                }
                            }

                        } else {

                            // no sequence
                            if (seqSetup == NONE) {

                                // select both chips
                                selectedChip = -1;
                                
                                // select column
                                pressedCol = lastSingleChip;
                            }
                        }

                        // NOISE
                        if (pin == 10) {
                            pressedRow = 3;

                            // sequence?
                            if (seqSetup == EDIT) seqNoise[selectedStep] = !seqNoise[selectedStep];

                        // ENVELOPE
                        } else {

                            // no sequence? -> env
                            if (seqSetup == NONE) pressedRow = 4;
                        }
                        break;

            case 4:     // ROW 5: SEQ

                        // initiate entering seq mode
                        if (seqSetup == NONE) {
                            seqPressed = true;
                            countDown = 3;
                        }

                        if (pressedRow == 5 || seqSetup == EDIT) {

                            // exit mode
                            seqSetup = NONE;
                            seqPressed = false;

                            // goto preset
                            pressedRow = 0;
                            ledNumber = oldNumber;

                        } else pressedRow = 5;
                        break;

            //
            // MATRIX COLS
            //

            case 6:     counter++;  // CHANNEL: c
            case 2:     counter++;  // CHANNEL: b
            case 11:    counter++;  // CHANNEL: a

                        if (pressedRow) {
                            pressedCol = counter;
                            selectedChip = 0;
                        }
                        break;

            case 3:     counter++;  // CHANNEL: f
            case 12:    counter++;  // CHANNEL: e
            case 9:     counter++;  // CHANNEL: d

                        if (pressedRow) {
                            pressedCol = counter+3;
                            selectedChip = 1;
                        }
                        break;
        }

        // store last single chip
        if (selectedChip > -1) lastSingleChip = selectedChip;

        // reset "voice mode: E"    
        if (pressedRow != 1) voiceMode = 0;

        //
        // CALCULATE & SHOW LED STATES
        //

        // ignore voice tuning
        if (voiceMode != VOICE_TUNING) {

            // bit toggling (except voice volume)
            if (pressedRow != 1 || (pressedRow == 1 && voiceMode == VOICE_ENABLE)) {

                // MATRIX COLS
                if (pin == 11 || pin == 2 || pin == 6 || pin == 9 || pin == 12 || pin == 3)
                    bitWrite(ledMatrix[pressedRow], pressedCol - 1, !bitRead(ledMatrix[pressedRow], pressedCol - 1));
            }

            // PRESET, ARP, VOICE, NOISE, ENV handling
            if (pressedRow == 1 ||
                pressedRow == 2 ||
                pressedRow == 3 ||
                pressedRow == 4)
                encoderMoved(0);

        } else ledNumber = 0;

    // released
    } else {

        switch (pin) {

            //
            // ENCODER SWITCH
            //

            case 1:     // handle except noise & envelope selection
                        if (pressedRow != 3 && seqSetup != EDIT && !(pressedRow == 4 && envPeriodType == 0)) {

                            // toggle: PRESET / BANK
                            if (countDown > 9) mode = (mode == 1) ? 2 : 1;

                            encPressed = false;
                            ledNumber = oldNumber;
                            pressedRow = 0;
                            encoderMoved(0);
                        }
                        break;

            //
            // MATRIX COLS
            //

            case 4:     seqPressed = false;
                        break;

            //
            // MATRIX ROWS
            //

            case 8:     // ROW 1: VOICE
                        voicePressed = false;
                        break;
        }
    }
}