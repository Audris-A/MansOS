/*
 * Copyright (c) 2008-2012 the MansOS team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *  * Redistributions of source code must retain the above copyright notice,
 *    this list of  conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _ATMEGA_TIMERS_H_
#define _ATMEGA_TIMERS_H_

#include <defines.h>
#include <avr/wdt.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/power.h>

#define atmegaWatchdogStop() wdt_disable()

// these must be defined as macros
#define JIFFY_CLOCK_DIVIDER 64
//! Atmel has straight decimal MHz
#define SLEEP_CLOCK_DIVIDER 1000

// TODO: test this
#define ACLK_SPEED          (CPU_HZ / JIFFY_CLOCK_DIVIDER)
#define SLEEP_CLOCK_SPEED   (CPU_HZ / SLEEP_CLOCK_DIVIDER)

#define PLATFORM_HAS_TIMERB 1

enum {
    JIFFY_TIMER_MS = 1000 / TIMER_INTERRUPT_HZ,

    // how many ticks per jiffy
    PLATFORM_ALARM_TIMER_PERIOD = JIFFY_TIMER_MS *
           (CPU_HZ / TIMER_INTERRUPT_HZ / JIFFY_CLOCK_DIVIDER),
    // in 1 jiffy = 1 ms case, 1 jiffy = 16000 / 64 = 250 clock cycles

    PLATFORM_MIN_SLEEP_MS = 1, // min sleep amount = 1ms
    PLATFORM_MAX_SLEEP_MS = 0xffff / (CPU_HZ / 1000 / SLEEP_CLOCK_DIVIDER + 1),

    // clock cycles in every sleep ms: significant digits and decimal part
    SLEEP_CYCLES = (CPU_HZ / 1000 / SLEEP_CLOCK_DIVIDER),
    SLEEP_CYCLES_DEC = (CPU_HZ / SLEEP_CLOCK_DIVIDER) % 1000,

    ALARM_CYCLES = (ACLK_SPEED / 1000 / JIFFY_CLOCK_DIVIDER),
    ALARM_CYCLES_DEC = (ACLK_SPEED / JIFFY_CLOCK_DIVIDER) % 1000,

    TICKS_IN_MS = CPU_HZ / 1000 / JIFFY_CLOCK_DIVIDER,
};

// bits for clock divider setup
#define TIMER0_DIV_1 (1 << CS00)
#define TIMER0_DIV_8 (1 << CS01)
#define TIMER0_DIV_64 (1 << CS01) | (1 << CS00)
#define TIMER0_DIV_256 (1 << CS02)
#define TIMER0_DIV_1024 (1 << CS02) | (1 << CS00)

#define TIMER1_DIV_1 (1 << CS10)
#define TIMER1_DIV_8 (1 << CS11)
#define TIMER1_DIV_64 (1 << CS11) | (1 << CS10)
#define TIMER1_DIV_256 (1 << CS12)
#define TIMER1_DIV_1024 (1 << CS12) | (1 << CS10)

#if JIFFY_CLOCK_DIVIDER == 1
#define TIMER0_DIVIDER_BITS TIMER0_DIV_1
#elif JIFFY_CLOCK_DIVIDER == 8
#define TIMER0_DIVIDER_BITS TIMER0_DIV_8
#elif JIFFY_CLOCK_DIVIDER == 64
#define TIMER0_DIVIDER_BITS TIMER0_DIV_64
#elif JIFFY_CLOCK_DIVIDER == 256
#define TIMER0_DIVIDER_BITS TIMER0_DIV_256
#elif JIFFY_CLOCK_DIVIDER == 1024
#define TIMER0_DIVIDER_BITS TIMER0_DIV_1024
#endif

#if SLEEP_CLOCK_DIVIDER == 1
#define TIMER1_DIVIDER_BITS TIMER1_DIV_1
#elif SLEEP_CLOCK_DIVIDER == 8
#define TIMER1_DIVIDER_BITS TIMER1_DIV_8
#elif SLEEP_CLOCK_DIVIDER == 64
#define TIMER1_DIVIDER_BITS TIMER1_DIV_64
#elif SLEEP_CLOCK_DIVIDER == 256
#define TIMER1_DIVIDER_BITS TIMER1_DIV_256
#elif SLEEP_CLOCK_DIVIDER == 1024
#define TIMER1_DIVIDER_BITS TIMER1_DIV_1024
#endif

#ifdef USE_HARDWARE_TIMERS
//=========================

#define atmegaStartTimer0() TCCR0B |= TIMER0_DIVIDER_BITS
#define atmegaStopTimer0() TCCR0B &= ~(TIMER0_DIVIDER_BITS)

#define atmegaTimersInit() \
    atmegaInitTimer0(); \
    atmegaInitTimer1();

#if MCU_model==attiny88
#define atmegaInitTimer0() \
    /* disable timer power saving */ \
    power_timer0_enable(); \
    /* CTC mode, compare with OCRxA */ \
    /* no clock source at the moment - timer not running */ \
    //TCCR0A = (1 << CTC0); \
    /* set time slice to jiffy (1ms) */ \
    OCR0A = PLATFORM_ALARM_TIMER_PERIOD - 1; \
    /* Enable Compare-A interrupt */ \
    TIMSK0 = (1 << OCIE0A); \
    /* reset counter */ \
    TCNT0 = 0;
#else
#define atmegaInitTimer0() \
    /* disable timer power saving */ \
    power_timer0_enable(); \
    /* CTC mode, compare with OCRxA */ \
    /* no clock source at the moment - timer not running */ \
    TCCR0A = (1 << WGM01); \
    /* set time slice to jiffy (1ms) */ \
    OCR0A = PLATFORM_ALARM_TIMER_PERIOD - 1; \
    /* Enable Compare-A interrupt */ \
    TIMSK0 = (1 << OCIE0A); \
    /* reset counter */ \
    TCNT0 = 0;
#endif

#define atmegaInitTimer1() \
    /* disable timer power saving */ \
    power_timer1_enable(); \
    /* CTC mode, compare with OCRxA */ \
    /* no clock source at the moment - timer not running */ \
    TCCR1A = 0; \
    TCCR1B = (1 << WGM12); \
    /* do not set ocr yet */ \
    /* Enable Compare-A interrupt */ \
    TIMSK1 = (1 << OCIE1A); \
    /* reset counter */ \
    TCNT1 = 0;

#define ALARM_TIMER_INTERRUPT() ISR(TIMER0_COMPA_vect)

#define ALARM_TIMER_START() atmegaStartTimer0()
#define ALARM_TIMER_STOP() atmegaStopTimer0()
#define ALARM_TIMER_WAIT_TICKS(ticks) { \
        uint16_t end = TCNT0 + ticks;   \
        while (TCNT0 != end);           \
    }

#define ALARM_TIMER_REGISTER OCR0A

//=========================
#endif   // USE_HARDWARE_TIMERS


// no need for time correction
#define CORRECTION_TIMER_REGISTER 0

#define ALARM_TIMER_READ() ALARM_TIMER_REGISTER

#define SLEEP_TIMER_WRAPAROUND() false
#define SLEEP_TIMER_RESET_WRAPAROUND()

// perform sleep/idle
// FIXME: why is power-save mode not working?
#define ENTER_SLEEP_MODE()                      \
    set_sleep_mode(SLEEP_MODE_IDLE);            \
    sleep_enable();                             \
    sei();                                      \
    sleep_mode();                               \
    sleep_disable();

// TODO: enable/disable all pull-ups?

// no action needed to exit idle/sleep mode
#define EXIT_SLEEP_MODE()

// in order to wake from sleep mode the timer must be async
// don't know, what that means, it was in Mantis :)
// but we use different timer setting, so it should do without these routines
#define WAIT_FOR_ASYNC_UPDATE() \
      /* while ((ASSR & (1 << OCR0UB)) || (ASSR & (1 << TCN0UB))); */


#define TIMER_INTERRUPT_VECTOR 0

#undef PLATFORM_HAS_CORRECTION_TIMER


// no DCO recalibration
#define hplInitClocks() 


#endif
