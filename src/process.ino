void doArp(byte select)
{
    switch (lfoShape) {
        case 6: prepareArpeggiator(ARP_UP,      select); break;
        case 5: prepareArpeggiator(ARP_DOWN,    select); break;
        case 7: prepareArpeggiator(ARP_UPSCALE, select); break;
        case 8: prepareArpeggiator(ARP_RANDOM,  select); break;
    }
}

void doLfoClock()
{
    lfoPhase += lfoIncrements[lfoSpeed >> 5];
    if (lfoPhase > 1)
        lfoPhase -= 1;
}

void doArpClock()
{
    if (held[MASTER] > 0) {

        // count
        arpcc++;

        // process
        if (arpcc >= clockSpeeds[map(lfoSpeed, 0, 255, 15, 0)]) {
            arpcc = 0;

            doArp(MASTER);
        }
    }
}

void doProcess()
{
    // ignore aymid
    if (aymidState.enabled) return;

    // clock handler (relaxed)
    if (clockSynced) {

        // don't miss a clock tick
        while (clockcc) {
            --clockcc;

            doLfoClock();
            doArpClock();
            doSequencerClock();
        }
    }

    // lfo
    if (lfoShape >= LFO_SHAPE1 && 
        lfoShape <= LFO_SHAPE4) prepareLfo(lfoShape);

    // arp
    if (!clockSynced) {

        arpcc++;
        if (arpcc > arpSpeed * 4) {
            arpcc = 0;

            for (byte i = MASTER; i < 7; i++)
                if (held[i] > 0) doArp(i);
        }
    }

    // pitches
    preparePitches();
}

void retriggerMixerAndSteps()
{
    // reset step
    seqStep = 0;

    // process
    processMixerAndSteps();
}

void processMixerAndSteps()
{
    // reset count
    seqcc = 0;

    // assign note
    seq = seqNote[seqStep];

    // mixer
    processMixer(true);

    ///////

    if (pressedRow == 5 && !seqPressed) {
        if (seqStep <= 7)   ledNumber = seqStep + 1;
        else                ledNumber = seqStep - 7;
    }

    // next
    seqStep++;
    if (seqStep > seqMax) seqStep = 0;
}

void processMixer(bool updateStep)
{
    if (seqSetup == NONE && displaycc >= 3000) setMixer(updateStep);
}