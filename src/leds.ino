#define DIMLEVEL    5
#define FLASHSPEED  1

// internals
int matrixFlasher;
bool flippedS = false;
bool flippedN = false;

void ledTickAymid()
{
    byte maskB = B11111111;
    byte maskC = B00000000;

    switch (ledtickcc)
    {
        case 0: // LEDNUMBER
                if (ledNumber == 1) { maskB = B11111110; maskC = B01000000; }
                if (ledNumber == 2) { maskB = B11111101; maskC = B01000000; }
                if (ledNumber == 3) { maskB = B11111011; maskC = B01000000; }
                if (ledNumber == 4) { maskB = B11110111; maskC = B01000000; }
                if (ledNumber == 5) { maskB = B11101111; maskC = B01000000; }
                if (ledNumber == 7) { maskB = B11111101; maskC = B10000000; }
                if (ledNumber == 6) { maskB = B11111110; maskC = B10000000; }
                if (ledNumber == 8) { maskB = B11111011; maskC = B10000000; }

                if (!flippedN && aymidState.preparedEnvType) maskB = B11111111;

                PORTB = maskB;
                PORTC = maskC;
                miniDelay();

                maskB = aymidState.incomingInd ? B11111111 : (mode == 1 ? B11101111 : B11110111); 
                maskC = B10000000;

                PORTB = maskB;
                PORTC = maskC;
                miniDelay();
                break;


        case 1: // LEDMATRIX

                //
                // ROW, COL HANDLING
                //

                for (byte i = 1; i < 6; i++) {
                    byte line = ledMatrix[i];
                    byte add2 = aymidState.isAltMode;

                    if (flippedN && pressedRow == i) { // +condition
                        if ((line & 0x20) != 0) maskC = line & B00000111;
                        else if (add2)          maskC = line | B00110000;
                        else                    maskC = line | B00100000;
                    } else                      maskC = line;

                    if (displaycc < MAX_LEDPICCOUNT) maskC = ledMatrixPic[i];

                    PORTB = ~(B00000001 << (i-1)); // set pin
                    PORTC = maskC;
                    miniDelay();
                }
                break;

        default: // switch off

                PORTB = maskB;
                miniDelay();
    }

    ledtickcc++;
    if (ledtickcc > 1 + DIMLEVEL) ledtickcc = 0;
}

void doLedMatrix()
{
    // decide flasher's speed (2x or 1x - default)
    byte speed = (pressedRow == 3 || pressedRow == 4) && selectedChip > -1 && !aymidState.enabled ? FLASHSPEED << 1 : FLASHSPEED;

    matrixFlasher += speed;
    if (matrixFlasher > 4000)
    {
        matrixFlasher = 0;
        maincc++;
    }

    if (displaycc >= MAX_LEDPICCOUNT)
    {
        if (matrixFlasher > 1000)   flippedN = true;
        else                        flippedN = false; // Normal

        if (maincc % 2 == 0)        flippedS = true;
        else                        flippedS = false; // Slow

    } else flippedS = flippedN = false;

    // AYMID routine
    if (aymidState.enabled) return ledTickAymid();

    // 1 = 1,6
    // 2 = 2,6
    // 3 = 3,6
    // 4 = 4,6
    // 5 = 5,6
    // 6 = 1,7
    // 7 = 2,7
    // 8 = 3,7
    // bank = 4,7
    // preset = 5,7

    byte maskB = B11111111;
    byte maskC = B00000000;

    switch (ledtickcc)
    {
        case 0:  // LEDNUMBER
                if (ledNumber == 1) { maskB = B11111110; maskC = B01000000; }
                if (ledNumber == 2) { maskB = B11111101; maskC = B01000000; }
                if (ledNumber == 3) { maskB = B11111011; maskC = B01000000; }
                if (ledNumber == 4) { maskB = B11110111; maskC = B01000000; }
                if (ledNumber == 5) { maskB = B11101111; maskC = B01000000; }
                if (ledNumber == 7) { maskB = B11111101; maskC = B10000000; }
                if (ledNumber == 6) { maskB = B11111110; maskC = B10000000; }
                if (ledNumber == 8) { maskB = B11111011; maskC = B10000000; }

                PORTB = maskB;
                PORTC = maskC;
                miniDelay();

                if      (mode == 1) { maskB = B11101111; maskC = B10000000; }
                else if (mode == 2) { maskB = B11110111; maskC = B10000000; }

                PORTB = maskB;
                PORTC = maskC;
                miniDelay();
                break;


        case 1: // LEDMATRIX


                //
                // LFO / ARP
                //

                if (flippedN && pressedRow == 2) {
                    if (ledMatrix[2] != 0)  maskC = 0;
                    else                    maskC = B00111111;
                } else                      maskC = ledMatrix[2];

                if (displaycc < MAX_LEDPICCOUNT) maskC = ledMatrixPic[2];

                PORTB = B11111101;
                PORTC = maskC;
                miniDelay();


                //
                // VOICE
                //

                // voice states (bit 0..5)
                maskC = ledMatrix[1];

                if (flippedN && pressedRow == 1) {

                    // has actives
                    if (maskC) {

                        // selected voice
                        if (pressedCol < 7) {

                            // slow 3x
                            if (flippedS)   maskC =  B00000000; // clr all

                            // normal 1x             B00XXXXXn clr voice n = 0, <<, cleanup (1: B00111110 ..)           
                            else            maskC &= B00111111 & ~(1 << (pressedCol - 1));

                            // no selection
                        } else              maskC =  B00000000; // clr all

                        // none actives
                    } else                  maskC =  B00111111; // none --> set all
                }

                if (seqSetup == 0 && !writeConfig && displaycc >= MAX_LEDPICCOUNT) {
                    if (seqVoice[selectedStep] == 1)    maskC = B00111111;
                    else                                maskC = 0;
                }

                if (displaycc < MAX_LEDPICCOUNT) maskC = ledMatrixPic[1];

                PORTB = B11111110;
                PORTC = maskC;
                miniDelay();


                //
                // NOISE & ENV
                //

                for (byte i = 3; i <= 4; i++) {

                    // noise with 3 states (0, 1, 2 selection)
                    maskC = ledMatrix[i];

                    if (flippedN && pressedRow == i) {

                        // 1st chip noise selection
                        if (selectedChip == 0 && pressedCol < 4) {

                            // slow 3x
                            if (flippedS) {
                                if (maskC)  maskC  = B00000000; // clr
                                else        maskC |= B00000111; // set 1st bitblock

                            // normal 1x
                            } else {
                                if (maskC)  maskC &= B00111000; // get 1st bitblock
                                else        maskC  = B00111111; // set all
                            }

                        // 2nd chip noise selection
                        } else if (selectedChip == 1 && pressedCol > 3 && pressedCol < 7) {

                            // slow 3x
                            if (flippedS) {
                                if (maskC)  maskC  = B00000000; // clr
                                else        maskC |= B00111000; // set 2nd bitblock

                            // normal 1x
                            } else {
                                if (maskC)  maskC &= B00000111; // get 2nd bitblock
                                else        maskC  = B00111111; // set all
                            }

                        // no or both chip selection
                        } else {
                            if (maskC)      maskC =  B00000000; // all ---> clr
                            else            maskC =  B00111111; // none --> set all
                        }
                    }

                    // noise seq assignment
                    if (i == 3 && seqSetup == 0 && !writeConfig && displaycc >= MAX_LEDPICCOUNT) {
                        if (seqNoise[selectedStep] == 1)    maskC = B00111111;
                        else                                maskC = 0;
                    }

                    if (displaycc < MAX_LEDPICCOUNT) maskC = ledMatrixPic[i];

                    PORTB = (i == 3) ? B11111011 : B11110111;
                    PORTC = maskC;
                    miniDelay();
                }


                //
                // SEQUENCER
                //

                if (flippedN && pressedRow == 5) {
                    if (ledMatrix[5] != 0)  maskC = 0;
                    else                    maskC = B00111111;
                } else                      maskC = ledMatrix[5];

                if (displaycc < MAX_LEDPICCOUNT) maskC = ledMatrixPic[5];

                PORTB = B11101111;
                PORTC = maskC;
                miniDelay();
                break;

        default: // switch off

                PORTB = maskB;
                //PORTC = maskC;    // optimized without PORTC
                miniDelay();
    }

    ledtickcc++;
    if (ledtickcc > 1 + DIMLEVEL) ledtickcc = 0;
}

//
// shows number in range 0..256 for any purpose
//

void showMatrixedNumber(byte value)
{

//
// lookup number by last col in row:
//
//  page    1.  2.  3.  4.  5.  6.  7.  8.  0.
//  row 1   6   36  66  96  126 156 186 216 246  <-- last col in row
//  row 2   12  42  72  102 132 162 192 222 252
//  row 3   18  48  78  108 138 168 198 228 258
//  row 4   24  54  84  114 144 174 204 234
//  row 5   30  60  90  120 150 180 210 240
//
//  value = last - col (of row)
//

    if (displaycc >= MAX_LEDPICCOUNT) copyDisplay();

    displaycc = 0;

    //
    // maxNumber 240
    //

    // matrix 8x [1..30]
    byte col = B00000000;
    if (value) col = 1 << (value - 1) % 6;

    // page 1..8
    for (ledNumber = 1; ledNumber < 8; ledNumber++)
        if (value <= 30 * ledNumber) break;

    // overflow correction (switch off page)
    if (value > 240) ledNumber = 0;

    // page value 1..30 (+fixed case 0)
    byte pageval = value % 30;
    if (!pageval && value) pageval = 30;

    // set col in row
    for (byte row = 1; row < 6; row++)
        ledMatrixPic[row] = (pageval > 6 * (row-1) && pageval <= (6 * row)) ? col : B00000000;

    /* range detection, the idea behind:
    ledMatrixPic[1] = ((value % 30) > 0  && (value % 30) <= 6)  ? col : B00000000;
    ledMatrixPic[2] = ((value % 30) > 6  && (value % 30) <= 12) ? col : B00000000;
    ledMatrixPic[3] = ((value % 30) > 12 && (value % 30) <= 18) ? col : B00000000;
    ledMatrixPic[4] = ((value % 30) > 18 && (value % 30) <= 24) ? col : B00000000;
    ledMatrixPic[5] = ((value % 30) > 24 && (value % 30) <= 30) ? col : B00000000;
    */
}

void restoreMatrix()
{
    if (displaycc != 20000) {
        displaycc = 20000;
        ledMatrix[1] = oldMatrix[1];
        ledMatrix[2] = oldMatrix[2];
        ledMatrix[3] = oldMatrix[3];
        ledMatrix[4] = oldMatrix[4];
        ledMatrix[5] = oldMatrix[5];
    }
}

void copyDisplay()
{
    oldNumber = ledNumber;

    oldMatrix[1] = ledMatrix[1];
    oldMatrix[2] = ledMatrix[2];
    oldMatrix[3] = ledMatrix[3];
    oldMatrix[4] = ledMatrix[4];
    oldMatrix[5] = ledMatrix[5];
}

void resetDisplay()
{
    ledNumber = 0;

    ledMatrix[1] = 0;
    ledMatrix[2] = 0;
    ledMatrix[3] = 0;
    ledMatrix[4] = 0;
    ledMatrix[5] = 0;
}

void copyAndResetDisplay()
{
    copyDisplay();
    resetDisplay();
}

void restoreDisplay()
{
    ledNumber = oldNumber;

    displaycc = 0;
    restoreMatrix();
}

void setPoint(byte row, byte col)
{
    if (row && row < 6 && col && col < 7)
        ledMatrix[row] |= (B00000001 << (col-1));
}

void clrPoint(byte row, byte col)
{
    if (row && row < 6 && col && col < 7)
        ledMatrix[row] &= ~ (B00000001 << (col-1));
}

void tglPoint(byte row, byte col)
{
    if (row && row < 6 && col && col < 7)
        ledMatrix[row] ^= (B00000001 << (col-1));
}