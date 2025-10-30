void save()
{
    // normal mode
    if (!writeConfig) {

        memPointer = (preset * 60) + ((bank * 60) * 8);

        for (byte i = 0; i < 16; i++) writey(seqNote[i]);
        for (byte i = 1; i < 7; i++) {
            writey(ledMatrix[i]);
            writey(tune[i]);
            writey(vol[i]);
        }

        writey(arpSpeed);
        writey(lfoShape);
        writey(lfoSpeed);
        writey(lfoDepth);
        writey(envNumber[0]);
        writey(lastEnvSpeedLUT);
        writey(seqSpeed);
        writey(detune);
        writey(glide);
        writey(noiseFreq[0]);

        for (byte i = 0; i < 8; i++) {
            bitWrite(seqNoise1, i, seqNoise[i]);
            bitWrite(seqNoise2, i, seqNoise[i + 8]);
            bitWrite(seqVoice1, i, seqVoice[i]);
            bitWrite(seqVoice2, i, seqVoice[i + 8]);
        }

        writey(seqNoise1);
        writey(seqNoise2);
        writey(seqVoice1);
        writey(seqVoice2);

        writey(envNumber[1]);
        writey(noiseFreq[1]);

        writey(lastEnvSpeed & 0xff);
        writey(lastEnvSpeed >> 8);
        writey(envMode);
        writey(noiseMode);

        writey(seqMax);
    }
}

void writey(byte data)
{
    EEPROM.write(memPointer, data);
    memPointer++;
}

byte readey()
{
    memPointer++;
    return EEPROM.read(memPointer - 1);
}

void load()
{
    // reset chip selection
    selectedChip = -1;

    memPointer = (preset * 60) + ((bank * 60) * 8);

    for (byte i = 0; i < 16; i++) seqNote[i] = readey();
    for (byte i = 1; i < 7; i++) {
        ledMatrix[i] = readey();
        bitWrite(ledMatrix[i], 6, 0);
        bitWrite(ledMatrix[i], 7, 0);

        presetTune[i] = readey();
        vol[i] = readey();
    } // 37

    arpSpeed = readey();
    lfoShape = readey();
    lfoSpeed = readey();
    lfoDepth = readey();

    envNumber[0] = readey();
    if      (envNumber[0] == 1) envShape[0] = B0000;
    else if (envNumber[0] == 2) envShape[0] = B0100;
    else if (envNumber[0] == 3) envShape[0] = B1000;
    else if (envNumber[0] == 4) envShape[0] = B1010;
    else if (envNumber[0] == 5) envShape[0] = B1011;
    else if (envNumber[0] == 6) envShape[0] = B1100;
    else if (envNumber[0] == 7) envShape[0] = B1101;
    else if (envNumber[0] == 8) envShape[0] = B1110;

    // envelope (old LUT)
    lastEnvSpeedLUT = readey();

    // seq speed
    seqSpeed = readey();

    // detune
    detune = readey();
    doDetune(detune);

    // glide
    glide = readey();

    // noise freq 1
    noiseFreq[0] = readey();

    // update noise 1
    AY3(6, noiseFreq[0]);

    data7ALast = 0;
    data7BLast = 0;

    // update vol/env a..f
    setVolEnvelopes();

    // prepare pitch a..f
    preparePitches();

    arpRange = map(lfoDepth, 0, 255, 0, 6);

    seqNoise1 = readey();
    seqNoise2 = readey();
    seqVoice1 = readey();
    seqVoice2 = readey();

    for (byte i = 0; i < 8; i++) {
        seqNoise[i]     = bitRead(seqNoise1, i);
        seqNoise[i + 8] = bitRead(seqNoise2, i);
        seqVoice[i]     = bitRead(seqVoice1, i);
        seqVoice[i + 8] = bitRead(seqVoice2, i);
    }

    envNumber[1] = readey();

    // take over chip 1
    if (envNumber[1] > 8) 
        envNumber[1] = envNumber[0];

    if      (envNumber[1] == 1) envShape[1] = B0000;
    else if (envNumber[1] == 2) envShape[1] = B0100;
    else if (envNumber[1] == 3) envShape[1] = B1000;
    else if (envNumber[1] == 4) envShape[1] = B1010;
    else if (envNumber[1] == 5) envShape[1] = B1011;
    else if (envNumber[1] == 6) envShape[1] = B1100;
    else if (envNumber[1] == 7) envShape[1] = B1101;
    else if (envNumber[1] == 8) envShape[1] = B1110;

    // update env shape
    triggerEnv();

    // noise freq 2
    noiseFreq[1] = readey();

    // take over chip 1
    if (noiseFreq[1] > 15) 
        noiseFreq[1] = noiseFreq[0];

    // update noise 2
    AY32(6, noiseFreq[1]);

    // envelope (default)
    lastEnvSpeed = readey();
    lastEnvSpeed |= readey() << 8;
    envMode = readey();

    // noise mode
    noiseMode = readey();

    // seq length
    seqMax = readey();
    if (!seqMax) seqMax = 15;

    // stop envelope freq
    stopEnvSpeed();
}