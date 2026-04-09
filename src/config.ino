void configProcessMessage(const byte* buffer, unsigned int size) {

    switch (buffer[5]) {

        //
        // CONFIG
        //
        
        case 0x01:  handleConfigReset(&buffer[6]);              break;  // reset to revision (incl. calib)    
        case 0x02:  handleConfiguration(&buffer[6], size-6);    break;  // configuration

#if USECALIBRATION
        //
        // CLOCK / TIMER CALIBRATION
        //

        case 0x03:  handleCalibReset(&buffer[6]);               break;  // reset calibration (all = 7F)
        case 0x04:  handleCalibration(&buffer[6], size-6);      break;  // clocktype calib
#endif
    }
}


void handleConfigReset(const byte* buffer) {

    switch (buffer[0]) {

        case BOARD_REVA:    //rev A (incl. ATARI calib)
                            EEPROM.write(3840, VERSION);
                            EEPROM.write(3843, 1);
                            EEPROM.write(3844, BOARD_REVA);
                            EEPROM.write(3845, CLOCK_ATARI);
                            EEPROM.write(3846, ENV_PDTYPE_MIX);
                            EEPROM.write(3850, CALIBRATION_OFF);
                            EEPROM.write(3851, CALIBRATION_OFF);
                            EEPROM.write(3857, CALIBRATION_OFF);
                            EEPROM.write(3858, CALIBRATION_OFF);

                            // reboot
                            wdt_enable(WDTO_500MS);
                            break;

        case BOARD_REVB:    // rev B (incl. ZX calib)
                            EEPROM.write(3840, VERSION);
                            EEPROM.write(3843, 1);
                            EEPROM.write(3844, BOARD_REVB);
                            EEPROM.write(3845, CLOCK_ZX);
                            EEPROM.write(3846, ENV_PDTYPE_MIX);
                            EEPROM.write(3850, CALIBRATION_OFF);
                            EEPROM.write(3851, CALIBRATION_OFF);
                            EEPROM.write(3857, CALIBRATION_OFF);
                            EEPROM.write(3858, CALIBRATION_OFF);

                            // reboot
                            wdt_enable(WDTO_500MS);
                            break;
    }
}

void handleConfiguration(const byte* buffer, unsigned int size) {

    if (size != 5) return;

    byte cChn = masterChannel;
    byte cRev = boardRevision;
    byte cClk = clockType;
    byte cEnT = envPeriodType;

    byte nChn = buffer[0];
    byte nRev = buffer[1];
    byte nClk = buffer[2];
    byte nEnT = buffer[3];

    if (!nChn && nChn < 16 && nChn != cChn)
        EEPROM.write(3843, nChn);

    if (nRev <= BOARD_REVB && nRev != cRev)
        EEPROM.write(3844, nRev);

    if (nClk <= CLOCK_LOW && nClk != cClk)
        EEPROM.write(3845, nClk);

    if (nEnT <= ENV_PDTYPE_LUT && nEnT != cEnT)
        EEPROM.write(3846, nEnT);

    // reboot
    wdt_enable(WDTO_500MS);
}

void handleCalibReset(const byte* buffer) {

    bool all = buffer[0] == 0x7F ? true : false;
    byte clk = all ? CLOCK_ATARI : buffer[0];

    switch (clk) {

        case CLOCK_ATARI:   EEPROM.write(3850, CALIBRATION_OFF);
                            EEPROM.write(3851, CALIBRATION_OFF);
                            if (!all) {

                                // reboot
                                wdt_enable(WDTO_500MS);
                                break;
                            }

        case CLOCK_ZX:      EEPROM.write(3857, CALIBRATION_OFF);
                            EEPROM.write(3858, CALIBRATION_OFF);

                            // reboot
                            wdt_enable(WDTO_500MS);
                            break;
    }
}

void handleCalibration(const byte* buffer, unsigned int size) {

    if (size != 7) return;

    byte cLoL, cHiL, cLoR, cHiR, cCnt;

    byte nLoL = buffer[1];
    byte nHiL = buffer[2];
    byte nLoR = buffer[3];
    byte nHiR = buffer[4];
    byte nCnt = buffer[5];

    switch (buffer[0]) {

        case CLOCK_ATARI:   if (calibratedAtari) {

                                cLoL = offsetL_Atari & 0x7f;
                                cHiL = (offsetL_Atari >> 7) & 0x7f;
                                cLoR = offsetR_Atari & 0x7f;
                                cHiR = (offsetR_Atari >> 7) & 0x7f;
                                cCnt = cntDelayAtari;

                                if (((nHiL << 7) | nLoL) < 2500) {
                                    if (nLoL != cLoL) EEPROM.write(3852, nLoL);
                                    if (nHiL != cHiL) EEPROM.write(3853, nHiL);
                                }

                                if (((nHiR << 7) | nLoR) < 2500) {
                                    if (nLoR != cLoR) EEPROM.write(3854, nLoR);
                                    if (nHiR != cHiR) EEPROM.write(3855, nHiR);
                                }

                                if (nCnt < 4 && nCnt != cCnt)
                                    EEPROM.write(3856, nCnt);

                            } else {

                                EEPROM.write(3850, CALIBRATION_ST1);
                                EEPROM.write(3851, CALIBRATION_ST2);

                                EEPROM.write(3852, nLoL);   // CHIP1 LO (LEFT)  (0...2499): 2478
                                EEPROM.write(3853, nHiL);   // CHIP1 HI
                                EEPROM.write(3854, nLoR);   // CHIP2 LO (RIGHT) (0...2499): 42
                                EEPROM.write(3855, nHiR);   // CHIP2 HI
                                EEPROM.write(3856, nCnt);   // PWM OFFSET       (0...3):    1                                
                            }

                            // reboot
                            wdt_enable(WDTO_500MS);
                            break;

        case CLOCK_ZX:      if (calibratedZX) {

                                cLoL = offsetL_ZX & 0x7f;
                                cHiL = (offsetL_ZX >> 7) & 0x7f;
                                cLoR = offsetR_ZX & 0x7f;
                                cHiR = (offsetR_ZX >> 7) & 0x7f;
                                cCnt = cntDelayZX;

                                if (((nHiL << 7) | nLoL) < 2479) {
                                    if (nLoL != cLoL) EEPROM.write(3859, nLoL);
                                    if (nHiL != cHiL) EEPROM.write(3860, nHiL);
                                }

                                if (((nHiR << 7) | nLoR) < 2479) {
                                    if (nLoR != cLoR) EEPROM.write(3861, nLoR);
                                    if (nHiR != cHiR) EEPROM.write(3862, nHiR);
                                }

                                if (nCnt < 9 && nCnt != cCnt)
                                    EEPROM.write(3863, nCnt);

                            } else {
    
                                EEPROM.write(3857, CALIBRATION_ZX1);
                                EEPROM.write(3858, CALIBRATION_ZX2);

                                EEPROM.write(3859, nLoL);   // CHIP1 LO (LEFT)  (0...2478): 2460
                                EEPROM.write(3860, nHiL);   // CHIP1 HI
                                EEPROM.write(3861, nLoR);   // CHIP2 LO (RIGHT) (0...2478): 90
                                EEPROM.write(3862, nHiR);   // CHIP2 HI
                                EEPROM.write(3863, nCnt);   // PWM OFFSET       (0...8):    7
                            }

                            // reboot
                            wdt_enable(WDTO_500MS);
                            break;
    }
}