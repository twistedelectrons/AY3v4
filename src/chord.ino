void doDetune(byte data)
{
    // lower half: bend
    if (data < 127) {

        chord = 0;

        // high clock adaption
        uint8_t shift = (clockType != CLOCK_LOW) ? 2 : 0;

        offsetFreq[1] = 0;
        offsetFreq[2] = -data / (20 >> shift);
        offsetFreq[3] = +data / (10 >> shift);

        offsetFreq[4] = +data / (30 >> shift);
        offsetFreq[5] = -data / (10 >> shift);
        offsetFreq[6] = +data / (20 >> shift);

        for (byte i = 1; i < 7; i++) {
            offsetNote[i] = 0;
            tune[i] = presetTune[i];
        }

    // upper half: chords
    } else {

        // reset tuning
        for (byte i = 1; i < 7; i++) tune[i] = 100;

        // no bend
        offsetFreq[1] = offsetFreq[2] = offsetFreq[3] = offsetFreq[4] = offsetFreq[5] = offsetFreq[6] = 0;

        // chords
        chord = map(data, 130, 250, 1, 8);

        switch (chord) {

            case 8: held[MASTER] = 0;
                    for (byte i = 1; i < 7; i++) base[i] = offsetNote[i] = slots[i] = 0;
                    break;

            case 1: offsetNote[1] = 1;
                    offsetNote[2] = 13;
                    offsetNote[3] = -11;
                    offsetNote[4] = 37;
                    offsetNote[5] = 25;
                    offsetNote[6] = -23;
                    break;

            case 2: offsetNote[1] = 1;
                    offsetNote[2] = 8;
                    offsetNote[3] = 13;
                    offsetNote[4] = 25;
                    offsetNote[5] = 8 + 12;
                    offsetNote[6] = 37;
                    break;

            case 3: offsetNote[1] = 1;
                    offsetNote[2] = -15;
                    offsetNote[3] = 4;
                    offsetNote[4] = 19;
                    offsetNote[5] = -5;
                    offsetNote[6] = -8;
                    break;

            case 4: offsetNote[1] = 1;
                    offsetNote[2] = -1;
                    offsetNote[3] = 6;
                    offsetNote[4] = -5;
                    offsetNote[5] = 13;
                    offsetNote[6] = 25;
                    break;

            case 5: offsetNote[1] = 1;
                    offsetNote[2] = 6;
                    offsetNote[3] = 10;
                    offsetNote[4] = -6;
                    offsetNote[5] = -11;
                    offsetNote[6] = 13;
                    break;

            case 6: offsetNote[1] = 1;
                    offsetNote[2] = 4;
                    offsetNote[3] = 8;
                    offsetNote[4] = 12;
                    offsetNote[5] = 16;
                    offsetNote[6] = 1;
                    break;

            case 7: offsetNote[1] = 1;
                    offsetNote[2] = 5;
                    offsetNote[3] = 8;
                    offsetNote[4] = 10;
                    offsetNote[5] = 13;
                    offsetNote[6] = -11;
                    break;
        }

        // fix tuning issue
        if (chord < 8)
            for (byte i = 1; i < 7; i++)
                offsetNote[i]--;
    }
}