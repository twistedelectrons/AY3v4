void AY3  (byte address, byte data) { ay3Reg1[address] = data; }
void AY32 (byte address, byte data) { ay3Reg2[address] = data; }

// time critical methods!

void updateAy3()
{
    // updating 14 sound register
    for (byte i = 0; i < 14; i++) {
        if (ay3Reg1[i] != ay3Reg1Last[i])
        {
            ay3Reg1Last[i] = ay3Reg1[i];
            send1(i, ay3Reg1Last[i]);
        }
    }
}

void updateAy32A()
{
    // updating 14 sound register
    for (byte i = 0; i < 14; i++) {
        if (ay3Reg2[i] != ay3Reg2Last[i])
        {
            ay3Reg2Last[i] = ay3Reg2[i];
            send2A(i, ay3Reg2Last[i]);
        }
    }
}

void updateAy32B()
{
    // updating 14 sound register
    for (byte i = 0; i < 14; i++) {
        if (ay3Reg2[i] != ay3Reg2Last[i])
        {
            ay3Reg2Last[i] = ay3Reg2[i];
            send2B(i, ay3Reg2Last[i]);
        }
    }
}

void mode_latch1()      { PORTD |= _BV(4);  PORTD |= _BV(5); }  // digitalWrite (12, HIGH); digitalWrite (13, HIGH);
void mode_write1()      { PORTD &= ~_BV(4); PORTD |= _BV(5); }  // digitalWrite (12, LOW);  digitalWrite (13, HIGH);
void mode_inactive1()   { PORTD &= ~_BV(4); PORTD &= ~_BV(5); } // digitalWrite (12, LOW);  digitalWrite (13, LOW);

void mode_latch2A()     { PORTD |= _BV(4);  PORTD |= _BV(6); }  // digitalWrite (12, HIGH); digitalWrite (14, HIGH);
void mode_write2A()     { PORTD &= ~_BV(4); PORTD |= _BV(6); }  // digitalWrite (12, LOW);  digitalWrite (14, HIGH);
void mode_inactive2A()  { PORTD &= ~_BV(4); PORTD &= ~_BV(6); } // digitalWrite (12, LOW);  digitalWrite (14, LOW);

void mode_latch2B()     { PORTD |= _BV(4);  PORTD |= _BV(7); }  // digitalWrite (12, HIGH); digitalWrite (15, HIGH);
void mode_write2B()     { PORTD &= ~_BV(4); PORTD |= _BV(7); }  // digitalWrite (12, LOW);  digitalWrite (15, HIGH);
void mode_inactive2B()  { PORTD &= ~_BV(4); PORTD &= ~_BV(7); } // digitalWrite (12, LOW);  digitalWrite (15, LOW);


// interrupts are disabled during the write pulse to the chip. This is to ensure that
// the write signal time (tDW) is within the 10 us maximum spec. On a 16 MHz ATmega 328,
// interrupts are disabled for about 4.5 us during each register write. (AY3891x lib)
//
// Disabling interrupts has another background: since it depends on an exact synchronization 
// of the clock frequency (time-critical) in order not to cause clicks, all interrupts are 
// stopped before the write process and thus in sync exactly at the time based on the specified values.
//
// time-critical notes:
// PWM hardware timers with different prescalers and modes are difficult to synchronize
// and this is the attempt to synchronize it manually. It should also be noted that 
// access to ports also takes place with different response times as a result.
// Different conditions were avoided in all functions, as these are time-critical actions for 
// updateAy32, send2 or all mode_x2 functions. That is why there are exists an 2A and 2B variant 
// that address other pins, but have the same structure as for variant 1.


void send1(unsigned char address, unsigned char data)
{
    // optimized send code
    // --------------------
    // ignore: initial inactive
    // PORTD &= ~_BV(4); // digitalWrite (12, LOW);

    PORTC = address;

    // latch
    PORTD |= _BV(4);    // digitalWrite (12, HIGH);
    PORTD |= _BV(5);    // digitalWrite (13, HIGH);

    // inactive
    PORTD &= ~_BV(4);   // digitalWrite (12, LOW);
    PORTD &= ~_BV(5);   // digitalWrite (13, LOW);

    PORTC = data;

    // noInterrupts
    cli();

    // write
    PORTD |= _BV(5);    // digitalWrite (13, HIGH);

    // inactive
    PORTD &= ~_BV(5);   // digitalWrite (13, LOW);

    // interrupts
    sei();
}

void send2A(unsigned char address, unsigned char data)
{
    // optimized send code
    // --------------------
    // ignore: initial inactive
    // PORTD &= ~_BV(4); // digitalWrite (12, LOW);

    PORTC = address;

    // latch
    PORTD |= _BV(4);    // digitalWrite (12, HIGH);
    PORTD |= _BV(6);    // digitalWrite (14, HIGH);

    // inactive
    PORTD &= ~_BV(4);   // digitalWrite (12, LOW);
    PORTD &= ~_BV(6);   // digitalWrite (14, HIGH);

    PORTC = data;

    // noInterrupts
    cli();

    // write
    PORTD |= _BV(6);    // digitalWrite (14, HIGH);

    // inactive
    PORTD &= ~_BV(6);   // digitalWrite (14, HIGH);

    // interrupts
    sei();
}

void send2B(unsigned char address, unsigned char data)
{
    // compressed send code
    // --------------------
    // ignore: initial inactive
    // PORTD &= ~_BV(4); // digitalWrite (12, LOW);

    PORTC = address;

    // latch
    PORTD |= _BV(4);    // digitalWrite (12, HIGH);
    PORTD |= _BV(7);    // digitalWrite (15, HIGH);

    // inactive
    PORTD &= ~_BV(4);   // digitalWrite (12, LOW);
    PORTD &= ~_BV(7);   // digitalWrite (15, HIGH);

    PORTC = data;

    // noInterrupts
    cli();

    // write
    PORTD |= _BV(7);    // digitalWrite (15, HIGH);

    // inactive
    PORTD &= ~_BV(7);   // digitalWrite (15, HIGH);

    // interrupts
    sei();
}

//
//   A SHORT STORY ABOUT PWM-HARDWARE-TIMER AND REGISTER-UPDATE AND THE WHOLE CRACKLING STORY
//
// - 2Mhz clock timer is much more reliable in clear sound than the 1.777Mhz clock timer
// - the clarity of the sound depends on the 16-bit timer, how they transmits the data 
//   in sync with the (PWM) clock; unsynced timers lead to crackling when feeding the ay3
// - for this purpose 2x 16 bit timers are started in parallel (most reliable) 
//   and they are adjusted to get in sync with the clock
// - starting the timers and executing the ISR routines is always the same after the start, 
//   but totally dependent of the amount of setup routine code
// - therefore the values have to be adapted exactly for this and it is not possible to 
//   adjust the timer again in the same way at runtime
// - the clock with the ISR then remains stable throughout but does not match every 
//   register change (especially in the treble range)
// - for this reason, each setting (revision > A and clock type) is handled individually,
//   as they can be configured individually in detail to achieve the best sync.
//

// I. 2x16bit timer using 2Mhz clock [PRESCALE = 256]:
// TIMER 2MHz Match (1249 -> 50, 624):
    /*
        cli();
        TCCR1A = 0;
        TCCR1B = _BV(WGM12) | _BV(CS12);            // CTC (OCR2A = TOP), PRESCALE = 256
        TIMSK1 = _BV(OCIE1A);
        TCNT1  = 624;                               // off: 1249/2 = 624,5 (-0.5) trunc = 624
        OCR1A  = 1249;                              // max: 1249 (50Hz)

        TCCR3A = 0;
        TCCR3B = _BV(WGM32) | _BV(CS32);            // CTC (OCR2A = TOP), PRESCALE = 256
        TIMSK3 = _BV(OCIE3A);
        TCNT3  = 0;                                 // off: 0
        OCR3A  = 1249;                              // max: 1249 (50Hz)
        sei();
    */

// II. 2x16bit timer using 1,777 clock [PRESCALE = 256]:
// TIMER 1,777MHz Match (1238 -> 50.44391, 619):
    /*
        cli();
        TCCR1A = 0;
        TCCR1B = _BV(WGM12) | _BV(CS12);            // CTC (OCR2A = TOP), PRESCALE = 256
        TIMSK1 = _BV(OCIE1A);
        TCNT1  = 619;                               // off: 1238/2 = 619
        OCR1A  = 1238;                              // max: 1238 (50.44391Hz)

        TCCR3A = 0;
        TCCR3B = _BV(WGM32) | _BV(CS32);            // CTC (OCR2A = TOP), PRESCALE = 256
        TIMSK3 = _BV(OCIE3A);
        TCNT3  = 0;                                 // off: 0
        OCR3A  = 1238;                              // max: 1238 (50.44391Hz)
        sei();
    */

    // seek values without crackles (near?, hit!)
    // 1405? 1246? 1244! 1240? 1238! 1232! 1228! 1226? 1208?
    // PRESCALE 256:1238 -> ARDUINO WEB TIMERS (Hz): 50.44391
    // switch in more fine-grained PRESCALE = 64 area (50.44391 Hz -> max 4955):
    // PRESCALE 64: 1238 x4 = 4952, fixed search by 50.44391 Hz hit again -> 4955
    // PRESCALE 64: value: 4955, offset: 2478!

void setupTimerZX()
{
    GTCCR = 1 << TSM | 1 << PSRASY | 1 << PSRSYNC;  // Halt all timers

    ASSR    = 0;                // Reset Async status register, TIMER2 clk = CPU clk
    TCNT2   = COUNT_DELAY_ZX;   // Reset Clock Counter (between 0..4) or adjust with repeating: asm volatile ("nop");

    // 1,777MHz [PRESCALE = 64]

    cli();
    TCCR1A = 0;
    TCCR1B = _BV(WGM12) | _BV(CS11) | _BV(CS10);    // CTC (OCR2A = TOP), PRESCALE = 64
    TIMSK1 = _BV(OCIE1A);
    TCNT1  = 2460;                                  // off: 4955/2 = 2477,5 (+0.5) ceil = 2478, [alt: 2400, 2451, 2454, 2457, 2460, 2466, 2469, 2472, 2475]
    OCR1A  = 4955;                                  // max: 1238*4 = 4952 (+3 by 50.44391) = 4955 

    TCCR3A = 0;
    TCCR3B = _BV(WGM32) | _BV(CS31) | _BV(CS30);    // CTC (OCR2A = TOP), PRESCALE = 64
    TIMSK3 = _BV(OCIE3A);
    TCNT3  = 90;                                    // off: 0, [alt: 6, 90, 100]
    OCR3A  = 4955;                                  // max: 1238*4 = 4952 (+3 by 50.44391) = 4955

#if LEDSUPPRESSION
    // DUTY CYCLE MATCH
    TIMSK1 |= _BV(OCIE1B);
    TIMSK3 |= _BV(OCIE3B);
    OCR1B  = 4953;
    OCR3B  = 4953;
#endif
    sei();

    GTCCR = 0; // Release timers
}

void setupTimer()
{
    GTCCR = 1 << TSM | 1 << PSRASY | 1 << PSRSYNC;  // Halt all timers

    ASSR    = 0; // Reset Async status register, TIMER2 clk = CPU clk
    TCNT2   = 0; // Reset Clock Counter (between 0..4) or adjust with repeating: asm volatile ("nop");

    // 500Khz [PRESCALE = 64]

    cli();
    TCCR1A = 0;
    TCCR1B = _BV(WGM12) | _BV(CS11) | _BV(CS10);    // CTC (OCR2A = TOP), PRESCALE = 64
    TIMSK1 = _BV(OCIE1A);
    TCNT1  = 2499;                                  // off: 4999/2 = 2499,5 (-0.5) trunc = 2499
    OCR1A  = 4999;                                  // max: 4999 (50Hz)

    TCCR3A = 0;
    TCCR3B = _BV(WGM32) | _BV(CS31) | _BV(CS30);    // CTC (OCR2A = TOP), PRESCALE = 64
    TIMSK3 = _BV(OCIE3A);
    TCNT3  = 0;                                     // off: 0
    OCR3A  = 4999;                                  // max: 4999 (50Hz)

#if LEDSUPPRESSION
    // DUTY CYCLE MATCH
    TIMSK1 |= _BV(OCIE1B);
    TIMSK3 |= _BV(OCIE3B);
    OCR1B  = 4997;
    OCR3B  = 4997;
#endif
    sei();

    GTCCR = 0; // Release timers
}

void setupTimerAtari()
{
    GTCCR = 1 << TSM | 1 << PSRASY | 1 << PSRSYNC;  // Halt all timers

    ASSR    = 0; // Reset Async status register, TIMER2 clk = CPU clk
    TCNT2   = 0; // Reset Clock Counter (between 0..4) or adjust with repeating: asm volatile ("nop");

    // 2MHz [PRESCALE = 64]

    cli();
    TCCR1A = 0;
    TCCR1B = _BV(WGM12) | _BV(CS11) | _BV(CS10);    // CTC (OCR2A = TOP), PRESCALE = 64
    TIMSK1 = _BV(OCIE1A);
    TCNT1  = 2499;                                  // off: 4999/2 = 2499,5 (-0.5) trunc = 2499
    OCR1A  = 4999;                                  // max: 4999 (50Hz)

    TCCR3A = 0;
    TCCR3B = _BV(WGM32) | _BV(CS31) | _BV(CS30);    // CTC (OCR2A = TOP), PRESCALE = 64
    TIMSK3 = _BV(OCIE3A);
    TCNT3  = 0;                                     // off: 0
    OCR3A  = 4999;                                  // max: 4999 (50Hz)

#if LEDSUPPRESSION
    // DUTY CYCLE MATCH
    TIMSK1 |= _BV(OCIE1B);
    TIMSK3 |= _BV(OCIE3B);
    OCR1B  = 4997;
    OCR3B  = 4997;
#endif
    sei();

    GTCCR = 0; // Release timers
}

void initAY3Clock()
{
    GTCCR = 1 << TSM | 1 << PSRASY | 1 << PSRSYNC;  // halt timers

    // denied EXTERNAL
    if (clockType != CLOCK_ZXEXT) {

        switch (clockType) {
            
            case CLOCK_LOW:     // PWM 500Khz (17 cycles)
                                TCCR2A = (boardRevision == BOARD_REVA) 
                                ?
                                    1 << COM2A0 |   // TOGGLE
                                    1 << WGM21      // CTC (OCR2A = TOP)
                                :
                                    1 << COM2B0 |   // TOGGLE
                                    1 << WGM21;     // CTC (OCR2A = TOP)
                                TCCR2B = 
                                    1 << CS20;      // NO PRESCALE = 1
                                TIMSK2 = 0;         // NO ISRs
                                OCR2A = 16;         // 17 cycles
                                break;

            case CLOCK_ZX:      // PWM 1.777Mhz for rev.B, otherwise use default
                                if (boardRevision > BOARD_REVA) {

                                    // non-inverting FPWM 1.778Mhz on OC2B
                                    TCCR2A = 
                                        1 << COM2B1 |   // SET +COM2B0 / CLR -COM2B0
                                        //1 << COM2B0 | // TGL
                                        1 << WGM20 |    // FAST PWM (flag1)
                                        1 << WGM21;     // OCR2A = TOP
                                    TCCR2B = 
                                        1 << WGM22 |    // FAST PWM (flag2)
                                        1 << CS20;      // PRESCALE 1
                                    TIMSK2 = 0;         // NO ISRs
                                    OCR2A = 8;          // 9 CPU cycles     ~ 8889Khz       // TGL MODE: OCR2A 3 ~ 2Mhz
                                    OCR2B = 3;          // 9/2 CPU cycles   ~ 1.778Mhz
                                    break;
                                }

                                // continue!

            case CLOCK_ATARI:
            default:            // PWM 2Mhz (4 cycles)
                                TCCR2A = (boardRevision == BOARD_REVA)
                                ?
                                    1 << COM2A0 |   // TOGGLE
                                    1 << WGM21      // CTC (OCR2A = TOP)
                                :
                                    1 << COM2B0 |   // TOGGLE
                                    1 << WGM21;     // CTC (OCR2A = TOP)
                                TCCR2B = 
                                    1 << CS20;      // NO PRESCALE = 1
                                TIMSK2 = 0;         // NO ISRs
                                OCR2A = 3;          // 4 cycles
        }
    }

    GTCCR = 0; // Release timers
}

void resetAY()
{
    digitalWrite(9, LOW);   // RIO: set to logic 0 (RESET)
    digitalWrite(9, HIGH);  // RIO: set to logic 1 (NO RESET)
}

// INTERRUPT SERVICE ROUTINES
ISR(TIMER1_COMPA_vect) { updateAy3(); }     // updating CHIP 1 - Left
ISR(TIMER3_COMPA_vect) { updateAy32(); }    // updating CHIP 2 - Right

// LED off impulse for PORTB (optimized without PORTC)
ISR(TIMER1_COMPB_vect) { 
    asm volatile ("nop"); 
    asm volatile ("nop");
    PORTB = B11111111;
}

ISR(TIMER3_COMPB_vect) { 
    asm volatile ("nop");
    asm volatile ("nop");
    PORTB = B11111111;
}