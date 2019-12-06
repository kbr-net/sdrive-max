// X-	LCD_D0	 8	14	PB0	PCINT0	Hi-Z	L	Z
// Y-	LCD_CS	A3	26	PC3	ADC3	L	Z+ADC	L
// X+	LCD_RS	A2	25	PC2	ADC2	Z	H	Z+ADC
// Y+	LCD_D1	~9	15	PB1	PCINT1	L	Z	H
// Ungünstig erscheint hier, dass CS beteiligt ist.
// Die Ausrichtung des LCDs ist stets in „natürlicher“ Form:
// Hochkant mit den Flachkabeln unten. Für Handys eben.

struct TSPoint {
	unsigned int x;
	unsigned int y;
};

unsigned char TS_init();
void restorePorts();
void waitTouch();
char isTouching();
//uint16_t readTouch(uint8_t b);
struct TSPoint getPoint();
struct TSPoint getRawPoint();
