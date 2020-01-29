#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include "avrlibdefs.h"         // global AVRLIB defines
#include "avrlibtypes.h"        // global AVRLIB types definitions
#include "touchscreen.h"
#include "tft.h"
#include "display.h"

u16 MINX EEMEM = 0xffff;
u16 MINY EEMEM = 0xffff;
u16 MAXX EEMEM = 0xffff;
u16 MAXY EEMEM = 0xffff;

#define TS_MINX eeprom_read_word(&MINX)
#define TS_MINY eeprom_read_word(&MINY)
#define TS_MAXX eeprom_read_word(&MAXX)
#define TS_MAXY eeprom_read_word(&MAXY)

extern unsigned int MAX_X;
extern unsigned int MAX_Y;
extern struct display tft;

unsigned char E_XP EEMEM = 0xff;
unsigned char XP;			//must be an analog port(PORTC)
#define XP_DDR DDRC			//fixed
#define XP_PORT PORTC			//fixed

unsigned char E_XM EEMEM = 0xff;
unsigned char XM;			//PB0, PD7
u16 E_XM_PIN EEMEM = 0xffff;
unsigned char *XM_PIN;			//for isTouching()
#define XM_DDR *(XM_PIN+1)
#define XM_PORT *(XM_PIN+2)

unsigned char E_YP EEMEM = 0xff;
unsigned char YP;			//PB1, PD6
u16 E_YP_PIN EEMEM = 0xffff;
unsigned char *YP_PIN;
#define YP_DDR *(YP_PIN+1)
#define YP_PORT *(YP_PIN+2)

unsigned char E_YM EEMEM = 0xff;
unsigned char YM;			//must be an analog port(PORTC)
#define YM_DDR DDRC			//fixed
#define YM_PORT PORTC			//fixed

int map(long x, long in_min, long in_max, long out_min, long out_max) {
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

unsigned char TS_init() {
	//A/D-converter enable, no irq, prescaler 128 -> 125 KHz
	ADCSRA = 0x87;

	if (eeprom_read_byte(&E_XP) == 0xff) {	//not detected?
		return(0);
	}

	XP = eeprom_read_byte(&E_XP);
	XM = eeprom_read_byte(&E_XM);
	YP = eeprom_read_byte(&E_YP);
	YM = eeprom_read_byte(&E_YM);
	XM_PIN = (unsigned char *) eeprom_read_word(&E_XM_PIN);
	YP_PIN = (unsigned char *) eeprom_read_word(&E_YP_PIN);

	return(1);
}

// prepare ports for touchscreen lines
static void setIdling() {
	XM_DDR &= ~(1<<XM); XM_PORT |= (1<<XM);		// X- = Hi-Z
	YM_PORT &= ~(1<<YM);				// Y- = L	analog
	XP_DDR &= ~(1<<XP); XP_PORT &= ~(1<<XP);	// X+ = Z	analog
	YP_PORT &= ~(1<<YP);				// Y+ = L
	//_delay_us(0.5);
	_delay_us(10);	//need more time, otherwise one bad touch occurs
}

// restore standard port conditions(for display to work)
void restorePorts() {
	XP_PORT |= (1<<XP);	// RS high
	//YM_PORT &=~(1<<YM);	// CS high or low??
	YM_PORT |= (1<<YM);	// CS high
	XM_DDR |= (1<<XM);	// X- (D0): output
	YM_DDR |= (1<<YM);	// Y- (CS): output
	XP_DDR |= (1<<XP);	// X+ (RS): output
	YP_DDR |= (1<<YP);	// Y+ (D1): output
}

// not used yet
//EMPTY_INTERRUPT(PCINT0_vect);

void waitTouch() {
	setIdling();
#if defined(HX8347G) || defined(ILI9329)
	PCMSK2 |= (1<<XM);		//select interrupt pin
	PCICR |= (1<<PCIE2);		//interrupt enable
#else
	PCMSK0 |= (1<<XM);		//select interrupt pin
	PCICR |= (1<<PCIE0);		//interrupt enable
#endif
/* too expensive, we make it by self, see below
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	sleep_mode();
*/
	//SMCR = (1<<SM1) | (1<<SE);	//power down mode, sleep enable
	SMCR = (1<<SM1) | (1<<SM0) | (1<<SE);	//power save mode, sleep enable
	sleep_cpu();
	SMCR = 0;			//disable
#if defined(HX8347G) || defined(ILI9329)
	PCICR &= ~(1<<PCIE2);		//interrupt disable
#else
	PCICR &= ~(1<<PCIE0);		//interrupt disable
#endif
	restorePorts();
}

char isTouching() {
	cli();		// no interrupts during change of port directions!
	setIdling();
	char ret=!(*XM_PIN & (1<<XM));	// read press condition(TRUE when LOW)
	restorePorts();
	sei();
	return ret;
}

uint16_t readTouch(uint8_t b) {
	if (b) {					// Y messure
		XM_DDR &=~(1<<XM); XM_PORT &=~(1<<XM);	// X- = Z
		YM_DDR |= (1<<YM); YM_PORT &=~(1<<YM);	// Y- = L
		XP_DDR &=~(1<<XP); XP_PORT &=~(1<<XP);	// X+ = Z
		YP_DDR |= (1<<YP); YP_PORT |= (1<<YP);	// Y+ = H
		ADMUX = (0x40 | XP);	// ADC2, reference 5 V, right aligned
	}
	else {						// X messure
		XM_DDR |= (1<<XM); XM_PORT &=~(1<<XM);	// X- = L
		YM_DDR &=~(1<<YM); YM_PORT &=~(1<<YM);	// Y- = Z
		XP_DDR |= (1<<XP); XP_PORT |= (1<<XP);	// X+ = H
		YP_DDR &=~(1<<YP); YP_PORT &=~(1<<YP);	// Y+ = Z
		ADMUX = (0x40 | YM);	// ADC3, reference 5 V, right aligned
	}
	_delay_us(0.5);
	ADCSRA |= (1<<ADSC);		// A/D-converter start
	while (ADCSRA&(1<<ADSC));	// wait until ready
	uint16_t ret=ADC;
	return ret;
}

struct TSPoint getPoint () {
	struct TSPoint p;

	cli();			//disable interrupts

	p.x = map(readTouch(0), TS_MINX, TS_MAXX, 0, MAX_X);
	p.y = map(readTouch(1), TS_MINY, TS_MAXY, 0, MAX_Y);

	restorePorts();
	sei();
	return(p);
}

struct TSPoint getRawPoint () {
	struct TSPoint p;

	cli();

	p.x = readTouch(0);
	p.y = readTouch(1);

	restorePorts();
	sei();
	return(p);
}
