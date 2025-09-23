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

    // pressed save - initiate rocket start (countdown!)
    if (savePressed) {
        savecc++;

        if (savecc > 1000) {
            savecc = 0;
            countDown--;
            ledNumber = countDown;

            // release & save preset
            if (!countDown) {
                savePressed = false;
                save();
            }
        }
    } else savecc = 0;

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