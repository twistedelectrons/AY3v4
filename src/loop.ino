void tick()
{
    switch (tickcc)
    {
        case 0: inputToggle ? doButtons() : doPots();           // reciprocal scanning
                inputToggle = !inputToggle;             break;  // relaxed

        case 1: doEncoder();                            break;  // fast
        case 2: doLedMatrix();                          break;  // fast
    }

    tickStateMachine();

    tickcc++;
    if (tickcc > 2) tickcc = 0; // time critical (3tick loop)
}

void tickStateMachine()
{
    //
    // ASSIGN pressedRow (delayed / safety)
    //

    if (selectedcc == SELECT_DELAY) {
        selectedcc = 0;
        pressedRow = selectRow;
        selectRow = 0;
    }

    if (selectRow) selectedcc++;


    //
    // Held buttons
    //

    // pressed voice - reset tuning check
    if (voicePressed && millis() > resetcc) {
        voicePressed = false;

        for (byte i = 1; i < 7; i++) tune[i] = presetTune[i] = 100;
    }


    // pressed encoder - initiate rocket start (countdown!)
    if (encPressed) {
        encodercc++;

        if (encodercc > 1000) {
            encodercc = 0;
            countDown--;

            if (countDown < 9) ledNumber = aymidState.enabled ? 9-countDown : countDown;

            // release & save preset
            if (!countDown) {
                encPressed = false;

                if (aymidState.enabled) {
                    aymidState.enabled = false;

                    // reload preset
                    mode = 1;
                    loadRequest = true;
                }
                else save();
            }
        }
    } else encodercc = 0;


    // load preset
    if (loadRequest && displaycc >= 20000) {
        loadRequest = false;

        if (lastPreset != preset) {
            lastPreset = preset;
            EEPROM.write(3800, preset); 
        }

        if (lastBank != bank) {
            lastBank = bank;
            EEPROM.write(3801, bank);
        }

        oldNumber = ledNumber = (mode == 1) ? preset + 1 : bank + 1;
        load();
    }


    // pressed seq - sequencer setup mode
    if (seqPressed == 1) {
        seqsetupcc++;

        if (seqsetupcc > 2000) {
            seqsetupcc = 0;
            countDown--;
            ledNumber = countDown;

            if (!countDown) {
                seqSetup = !seqSetup;

                // silence voices, entering setup
                if (seqSetup == 0) {
                    base[1] = base[2] = base[3] = base[4] = base[5] = base[6] = 0;

                    bitWrite(data7A, 0, 1);
                    bitWrite(data7A, 1, 1);
                    bitWrite(data7A, 2, 1);
                    bitWrite(data7B, 0, 1);
                    bitWrite(data7B, 1, 1);
                    bitWrite(data7B, 2, 1);

                    pressedRow = 0;
                }

                ledNumber = oldNumber;
                encoderMoved(0);
            }
        }
    } else seqsetupcc = 0;


    //
    // Display counter
    //

    if (displaycc < 20000)
    {
        // prevent conflicts
        if (!writeConfig) displaycc++;


    // Counter to wipe numbers on led matrix and revert back to stored led matrix
    } else restoreMatrix();
}