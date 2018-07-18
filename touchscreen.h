// X-	LCD_D0	 8	14	PB0	PCINT0	Hi-Z	L	Z
// Y-	LCD_CS	A3	26	PC3	ADC3	L	Z+ADC	L
// X+	LCD_RS	A2	25	PC2	ADC2	Z	H	Z+ADC
// Y+	LCD_D1	~9	15	PB1	PCINT1	L	Z	H
// Ungünstig erscheint hier, dass CS beteiligt ist.
// Die Ausrichtung des LCDs ist stets in „natürlicher“ Form:
// Hochkant mit den Flachkabeln unten. Für Handys eben.

//#pragma once
//#include <stdint.h>

#if ! defined(ILI9329) && ! defined(HX8347G) && ! defined(ILI9340)
//Touch For New ILI9341 TP
/* no more needed, will be calibrated now
#define TS_MINX 120
#define TS_MAXX 900

#define TS_MINY 75
#define TS_MAXY 880
*/

#define XP	PC2		//must be an analog port
#define XP_PORT	PORTC
#define XP_DDR	DDRC

#define XM	PB0
#define XM_PORT	PORTB
#define XM_DDR	DDRB
#define XM_PIN	PINB		//for isTouching()

#define YP	PB1
#define YP_PORT	PORTB
#define YP_DDR	DDRB

#define YM	PC3		//must be an analog port
#define YM_PORT	PORTC
#define YM_DDR	DDRC

#else
//Touch For ILI9329 or HX8347G
/*
#ifdef HX8347G
#define TS_MINX 100
#define TS_MAXX 850

#define TS_MINY 110
#define TS_MAXY 850
#else
#define TS_MINX 120
#define TS_MAXX 920

#define TS_MINY 110
#define TS_MAXY 940
#endif
*/

#define XP	PC1		//must be an analog port
#define XP_PORT	PORTC
#define XP_DDR	DDRC

#define XM	PD7
#define XM_PORT	PORTD
#define XM_DDR	DDRD
#define XM_PIN	PIND		//for isTouching()

#define YP	PD6
#define YP_PORT	PORTD
#define YP_DDR	DDRD

#define YM	PC2		//must be an analog port
#define YM_PORT	PORTC
#define YM_DDR	DDRC

#endif

struct TSPoint {
	unsigned int x;
	unsigned int y;
};

char isTouching();
//uint16_t readTouch(uint8_t b);
struct TSPoint getPoint();
struct TSPoint getRawPoint();
