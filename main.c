/* Matra Murena Ignition Timer
 *
 * Copyright (c) 2024 Anders Dinsen anders@dinsen.net - All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* Target board configuration: */
#undef CRYSTAL22
#define FLIPPEDDISPLAYS

#include <at89x52.h>

/* 89S51 registers not in the at89x51.h */
__sfr __at 0x8E AUXR;
__sfr __at 0xA2 AUXR1;
__sfr __at 0xA6 WDTRST;

#include <stdlib.h>

#ifdef CRYSTAL22
#define MINCOUNT 5532; 
#define	IGNITION_FACTOR 55312384L 
#else
#define MINCOUNT 2766;
#define	IGNITION_FACTOR 27656192L 
#endif

#ifdef FLIPPEDDISPLAYS
static const unsigned char rpm_display[] = {3,4,5,6};
static const unsigned char deg_display[] = {0,1,2};
#else
static const unsigned char rpm_display[] = {6,5,4,3};
static const unsigned char deg_display[] = {2,1,0};
#endif

/* The physical offset of the flywheel notch in degrees from TDC is 28 degrees: */
#define FW_OFFSET 28 

#define LATCH_FW P3_7
#define LATCH_IGN P3_6

/* Global flags */
volatile __bit f_ignition_interrupt;
volatile __bit f_crank_interrupt;

/* Other global variables */

volatile union _byte_addressable_long { 
	unsigned long l;
	struct {
		unsigned int w0;
		unsigned int w1;
	} w;
	struct {
		unsigned char b0;
		unsigned char b1;
		unsigned char b2;
		unsigned char b3;
	} b;
} t_ignition;

volatile union _byte_addressable_long t_crank;

volatile unsigned char timer0_extension;
volatile unsigned char digits[7];
volatile unsigned char display_digit;
volatile unsigned char mask_digit;
volatile int timer_tick;
volatile char user_command;
volatile unsigned char ms_counter;
volatile unsigned char rpm[4];
volatile unsigned char adv[3];

#ifdef FLIPPEDDISPLAYS
/* LED displays are flipped to the rear of the PCB so the bits are permutated:
 * bit 0 => F
 * bit 1 => G
 * bit 2 => D
 * bit 3 => C
 * bit 4 => DP
 * bit 5 => A
 * bit 6 => B
 * bit 7 => E */
static const unsigned char s7[] = {
	0b11101101, /* 0 ABCDEF  */
	0b01001000, /* 1  BC     */
	0b11100110, /* 2 AB DE G */
	0b01101110, /* 3 ABCD  G */
	0b01001011, /* 4  BC  FG */
	0b00101111, /* 5 A CD FG */
	0b10101111, /* 6 A CDEFG */
	0b01101000, /* 7 ABC     */
	0b11101111, /* 8 ABCDEFG */
	0b01101111, /* 9 ABCD FG */ 
	0b11101011, /* A ABC EFG*/ 
	0b10001111, /* b   CDEFG */ 
	0b10000110, /* c    DE G */ 
	0b11001110, /* d  BCDE G */ 
	0b10100111, /* E A  DEFG */ 
	0b10100011  /* F A   EFG */ 
};
#define LED_DASH	(2)
#define LED_DP		(16)
#else
/* LED displays are fitted on the front of the PCB as per the original design:
 * bit 0 => A
 * bit 1 => B
 * bit 2 => C
 * bit 3 => D
 * bit 4 => E
 * bit 5 => F
 * bit 6 => G
 * bit 7 => DP */
static const unsigned char s7[] = {
	0b00111111, /* 0 */
	0b00000110, /* 1 */
	0b01011011, /* 2 */
	0b01001111, /* 3 */
	0b01100110, /* 4 */
	0b01101101, /* 5 */
	0b01111101, /* 6 */
	0b00000111, /* 7 */
	0b01111111, /* 8 */
	0b01101111, /* 9 */ 
	0b01110111, /* A */ 
	0b01111100, /* b */ 
	0b01011000, /* c */ 
	0b01011110, /* d */ 
	0b01111001, /* E */ 
	0b01110001  /* F */ 
};
#define LED_DASH	(64)
#define LED_DP		(128)
#endif

/* Interrupt routines
 * INT0 = Ignition trigger
 * INT1 = Timer 0 (16 bit counting) 
 * INT2 = Flywheel trigger
 * INT3 = Timer 1 (unused)
 * INT4 = UART
 * INT5 = Timer 2 (display)
 */

/* ignition interrupt routine, main job is to save the timer0 for later calculation and then restart from 0 */

void ignition_isr(void) __interrupt (0) {
	t_ignition.b.b0 = TL0;
	t_ignition.b.b1 = TH0;
	t_ignition.b.b2 = timer0_extension;
	TL0 = TH0 = timer0_extension = 0;
	LATCH_FW = 0; /* unlatch flywheel trigger circuit */
	TF0 = 0;
	f_ignition_interrupt = 1;
	P3_5 = 0;
}

/* timer0 overflow interrupt routine extends timer0 to 24 bits */

void timer0_isr(void) __interrupt (1) {
	timer0_extension++;
	P3_5 = 1;
}

/* crank interrupt routine saves the timer0 including 8 bit extension in the crank time variable for later calculations */

void crank_isr(void) __interrupt (2) {
	f_crank_interrupt = 1;
	t_crank.b.b0 = TL0;
	t_crank.b.b1 = TH0;
	t_crank.b.b2 = timer0_extension;
	LATCH_FW = 1; /* latch flywheel trigger circuit to prevent it from picking up noise until next ignition */
}

/* uart interrupt reads watever user command character is received 
 * TODO implement interrupt and buffered output, emit a character from an output buffer */

void uart_isr(void) __interrupt (4) {
	if ( RI ) {
		user_command = SBUF;
		RI = 0;
	}
}

/* timer 2 interrupt runs the refresh of the display and a miliseconds counter for precise main loop timing */

void digits_isr(void) __interrupt (5) {
	P1_4 = 0; /* test output to measure interrupt time */

	EXF2 = 0;
	TF2 = 0;

	/* next display digit, calculate mask */
	if ( display_digit ) {
		display_digit--;
		mask_digit |= 128;
		mask_digit >>= 1;
	}
	else {
		display_digit = 6;
		mask_digit = (unsigned char)0b10111111;
	}

	/* display digit */
	P2 = 0;
	P0 = mask_digit;
	P2 = digits[display_digit];

	ms_counter++;

	P1_4 = 1; /* reset test output */
}

/* this function should be called to set a start point in time from which delay is measured */

void inline delay_start(void) {
	ms_counter = 0;
}

/* this function sleeps until d ms has passed since delay_start() was called */

void inline delay_ms(unsigned char d) {
	while ( ms_counter < d ) {
		WDTRST = 0x1e;
		WDTRST = 0xe1; 
	}
}

/* UART utility routines
 * TODO rewrite to use a small output buffer and output characters by interrupts */

void inline emit_char(char c) {
	SBUF=c;
	while ( !TI ) {
		WDTRST = 0x1e;
		WDTRST = 0xe1;
	}
	TI=0; 
}

void emit_string(__code char *s) {
	while( *s ) 
		emit_char(*s++);
}


/* main loop where all processing occurs and state is maintained. */

void main(void) {
	unsigned char i, j; 
	unsigned char n = 0;
	long l;
	__bit f_ign_overflow;

	/* ports init */
	P0 = 255; /* All displays off */
	P1 = 255; /* P1 are inputs */
	P2 = 0; /* All segments off */
	P3 = 63; /* Latch off detectors, other pins are serial I/O, interrupt inputs and debug outputs */

	/* global variables initialization */
	timer_tick = 0;
	user_command = 0;
	display_digit = 0;
	ms_counter = 0;
	mask_digit = (unsigned char)0b10111111;
	timer0_extension = 0;

	/* Timer and UART init */
	TMOD	= 0x21;	/* Timer 0 in 16 bit (mode 1), Timer 1 in 8 bit autoreload (mode 2) */

	/* PCON 	|= 0x80; Double baud rate */
	TH1	= 0xfa; /* 4800 Baud rate at 11.0592 MHz, 9600 Baud rate at 22.1184 MHz */
	TL1	= 0;
	SCON	= 0x50; /* Asynchronous mode 8-bit data, 1-stop bit */
	TR1	= 1;	/* Start baud rate generator */

	/* Timer 0 is used as a free-running 16 bit counter */
	TL0 = TH0 = 0;
	TF0	= 0;	/* clear timer 0 overflow flag */
	TR0	= 1;	/* Start timer 0 */

	/* Timer 2 is used for the display refresh at 1 kHz */
	T2CON	= 0b00000100; /* automatic relad timer 2, start timer 2 */
	T2MOD	= 0;
	TL2	= 0;
	TH2	= 0;
#ifdef CRYSTAL22
	RCAP2H	= 0xF8;
	RCAP2L	= 0xCD;
#else	
	RCAP2H	= 0xFC;
	RCAP2L	= 0x67;
#endif

	IT1 = IT0 = 1;  /* INT0 and INT1 are both falling edge triggered */

	/* Other inits */
	AUXR = 0b00011000; /* WDIDLE = 1, watch dog timer continues counting
			      in idle mode 
	                      DISRTO = 1, reset pin is input only
			      DISALE = 0, ALE is emitted */

	IE = 0b10110111; /* EA = 1, enable all interrupts
			    Unused bit = 0
			    ET2 = 1, enable timer 2 interrupt
			    ES = 1, enable serial port interrupt
			    ET1 = 0, disable timer 1 interrupt
			    EX1 = 1, enable external interrupt 1
			    ET0 = 1, disable timer 0 interrupt
			    EX0 = 1, enable external interrupt 0 */
	
	IP = 0b00000101; /* the two external interrupts takes interrupt priority over the timers and serial port */

	LATCH_FW = LATCH_IGN = 0;

	l = 0;
	delay_start();
	while (1) {
		WDTRST = 0x1e;
		WDTRST = 0xe1;

		P3_4 = 0; /* debug output to time calculations */

		if ( P1_2 && P1_3 /* dip switches 3 & 4 both off */ ) {
			/* TEST MODE: serial output test */
			emit_string("SERIAL OUTPUT TEST MODE\r\n");
		}
		else if ( P1_2 && !P1_3 /* dip switches 3 off & 4 on */ ) {
			/* Display test mode */
			for (i=6; i != 255; i--) 
				digits[i] = s7[(n & 31) >> 1] | (i==(n&7)? LED_DP : 0);
			n++;
		}
		else if ( f_ignition_interrupt ) {
			if ( P1_1 /* dip switch 2 off */ ) {
				/* TEST MODE: display timer values directly in hex */
				digits[deg_display[0]] = f_crank_interrupt? s7[t_crank.b.b0 & 15] : LED_DASH;
				digits[deg_display[1]] = f_crank_interrupt? s7[t_crank.b.b0 >> 4] : LED_DASH;
				digits[deg_display[2]] = f_crank_interrupt? s7[t_crank.b.b1 & 15] : LED_DASH;

				if ( t_crank.b.b1 & 16 ) 
					digits[deg_display[0]] |= LED_DP;
				if ( t_crank.b.b1 & 32 ) 
					digits[deg_display[1]] |= LED_DP;
				if ( t_crank.b.b1 & 64 ) 
					digits[deg_display[2]] |= LED_DP;

				digits[rpm_display[0]] = s7[t_ignition.b.b0 & 15];
				digits[rpm_display[1]] = s7[t_ignition.b.b0 >> 4];
				digits[rpm_display[2]] = s7[t_ignition.b.b1 & 15];
				digits[rpm_display[3]] = s7[t_ignition.b.b1 >> 4];

				if ( t_ignition.b.b2 & 1 ) 
					digits[rpm_display[0]] |= LED_DP;
				if ( t_ignition.b.b2 & 2 ) 
					digits[rpm_display[1]] |= LED_DP;
				if ( t_ignition.b.b2 & 4 ) 
					digits[rpm_display[2]] |= LED_DP;
				if ( t_ignition.b.b2 & 8 ) 
					digits[rpm_display[3]] |= LED_DP;
				f_crank_interrupt = 0;
			}
			else {
				/* calcuate and display RPM and crank angle */
				f_ign_overflow = t_ignition.l < MINCOUNT; 
				l = IGNITION_FACTOR / t_ignition.l;

				for (i=0, j=3; i<4; i++, j--) {
					if ( f_ign_overflow ) 
						n = 9;
					else
						n = (unsigned char)(l % 10);
					digits[rpm_display[i]] = s7[n];
					rpm[j] = n;
					l /= 10;
				}

				if ( P1_3 /* dip switch 1 off */ ) {
					for (j=0; j<4; j++) emit_char(rpm[j]+'0');
					emit_char(' ');
				}

				if ( f_crank_interrupt ) {
					/* crank signal detected, calculate advance */
					l = ( 180 * t_crank.l ) / t_ignition.l - FW_OFFSET; 
					for (i=0; i<3; i++) {
						n = (unsigned char)(l % 10);
						digits[deg_display[i]] = s7[n];
						adv[i] = n;
						l /= 10;
					}
					if ( P1_3 ) {
						for (j=0; j<3; j++) emit_char(adv[j]+'0');
					}

					f_crank_interrupt = 0;
				}
				else {
					/* no crank signal detected, dash out advance digits */
					for (i=0; i<3; i++) 
						digits[i] = LED_DASH;
				}

				if ( P1_3 ) {
					emit_char('\r');
					emit_char('\n');
				}
			}
		}
		else {
			/* no ignition detected since last round of the main loop, dash out all digits */
			for (i=0; i<7; i++)
				digits[i] = LED_DASH;
			f_crank_interrupt = 0;
		}

		f_ignition_interrupt = 0;

		if ( user_command ) {
			/* for now, just echo whatever is typed on the serial port */
			emit_char(user_command);
			if ( user_command == '\r' )
				emit_char('\n');
			user_command = '\0';
		}

		P3_4 = 1; /* debug for measuring the time it takes to run the main loop */

		delay_ms(250); /* update display four times a second */
		delay_start();
	}; 
}

