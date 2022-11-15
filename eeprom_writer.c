#include <avr/eeprom.h>
#include <util/delay.h>
//#include <avr/pgmspace.h>
#include <stdio.h>
#include "display.h"
#include "eeprom_writer.h"
#include "avrlibtypes.h"

char txt[40];

void TS_detect() {
	unsigned char apin, dpin;
	unsigned char found = 0;
	unsigned char *port, *ddr;
	unsigned char apins[2], dpins[2], *pinregs[2];
	unsigned short val, vals[2];

	//A/D-converter enable, no irq, prescaler 128 -> 125 KHz
	ADCSRA = 0x87;

	DDRC &= ~0x0f;	//input
	PORTC |= 0x0f;	//enable pullup

	for (apin = PC0; apin < PC4; apin++) {
		ADMUX = (0x40 | apin);	//select analog pin, reference 5 V,
					//right aligned
		//first check PB0 and PB1
		port = (unsigned char *) _SFR_MEM_ADDR(PORTB);
		ddr = (unsigned char *) _SFR_MEM_ADDR(DDRB);

		for (dpin = 0; dpin < 8; dpin++) {
			if (dpin == 2) {	//change from PORTB to PORTD
				port = (unsigned char *) _SFR_MEM_ADDR(PORTD);
				ddr = (unsigned char *) _SFR_MEM_ADDR(DDRD);
			}
			*ddr |= (1<<dpin);	//output
			*port &= ~(1<<dpin);	//low
			_delay_ms(1);		//wait a little bit,
						//otherwise we messure
						//the falling edge!
			ADCSRA |= (1<<ADSC);	//A/D-converter start
			//wait until ready
			while (ADCSRA&(1<<ADSC));
			val = ADC;
			*ddr &= ~(1<<dpin);	//input
			*port |= (1<<dpin);	//pullup

			if (val < 100 && found < 2) {
				vals[found] = val;
				apins[found] = apin;
				dpins[found] = dpin;
				pinregs[found] = port - 2;
				found++;
			}
		}
	}
	TFT_GPIO_init();	//reset Pins for display
	sprintf(txt, "Values: %i, %i", vals[0], vals[1]);
	print_str(10,210,1,Grey,Black,txt);
	if (found == 2) {
		unsigned char i;
		//the lower value is the X direction
		i = (vals[0] > vals[1]) ? 1 : 0;

		eeprom_update_byte(E_XP, apins[i]);
		eeprom_update_byte(E_XM, dpins[i]);
		eeprom_update_byte(E_YP, dpins[!i]);
		eeprom_update_byte(E_YM, apins[!i]);
		eeprom_update_word((u16 *)E_XM_PIN, (u16) pinregs[i]);
		eeprom_update_word((u16 *)E_YP_PIN, (u16) pinregs[!i]);

		sprintf(txt, "XP: A%i, XM: %i, YP: %i, YM: A%i", apins[i], dpins[i], dpins[!i], apins[!i]);
		print_str(10,220,1,Green,Black,txt);
	}
	else
		print_str(10,220,1,Red,Black,"Not detected!");

}

int main () {
	unsigned int i;
	//unsigned char *p = 0;
	TFT_init();
	TFT_set_rotation(PORTRAIT_2);
	TFT_fill(Black);
	TFT_on();

	print_str(10,22,1,White,Black,"Writing EEPROM...");
	for(i=0; i < SDrive_eep_bin_len; i++) {
		Draw_H_Bar(10,229,50,10,(i/(SDrive_eep_bin_len/219.0)),Yellow,Blue,Grey,1);
		//eeprom_write_byte((unsigned char*)i, pgm_read_byte(&SDrive_eep_bin[i]));
		eeprom_update_byte((unsigned char*)i, SDrive_eep_bin[i]);
		//_delay_ms(10);
	}
	print_str(10,122,1,White,Black,"Verifying EEPROM...");
	for(i=0; i < SDrive_eep_bin_len; i++) {
		Draw_H_Bar(10,229,150,10,(i/(SDrive_eep_bin_len/219.0)),Yellow,Blue,Grey,1);
		if (eeprom_read_byte((unsigned char*)i) != SDrive_eep_bin[i]) {
			sprintf(txt, "Verify error at 0x%03x: %02x != %02x", i,
				eeprom_read_byte((unsigned char*)i), SDrive_eep_bin[i]);
			print_str(10,180,1,Red,Black,txt);
			return(1);
		}
		//_delay_ms(10);
	}

	print_str(10,180,1,Green,Black,"Done!");
	print_str(10,200,1,White,Black,"Detecting touchscreen...");
	TS_detect();
	print_str(10,240,1,White,Black,"Now flash SDrive.hex!");
	return(0);
}
