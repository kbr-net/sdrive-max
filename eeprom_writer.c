#include <avr/eeprom.h>
#include <util/delay.h>
//#include <avr/pgmspace.h>
#include "display.h"
#include "eeprom_writer.h"

int main () {
	unsigned int i;
	//unsigned char *p = 0;
	TFT_init();
	TFT_set_rotation(PORTRAIT_2);
	TFT_fill(Black);
	TFT_on();
	print_str(10,22,1,White,Black,"Writing EEPROM...");
	for(i=0; i < SDrive_eep_bin_len; i++) {
		Draw_H_Bar(10,229,50,10,(int) (i/(SDrive_eep_bin_len/219.0)),Yellow,Blue,Grey,1);
		//eeprom_write_byte((unsigned char*)i, pgm_read_byte(&SDrive_eep_bin[i]));
		eeprom_update_byte((unsigned char*)i, SDrive_eep_bin[i]);
		//_delay_ms(10);
	}
	print_str(10,122,1,White,Black,"Verifying EEPROM...");
	for(i=0; i < SDrive_eep_bin_len; i++) {
		Draw_H_Bar(10,229,150,10,(int) (i/(SDrive_eep_bin_len/219.0)),Yellow,Blue,Grey,1);
		if (eeprom_read_byte((unsigned char*)i) != SDrive_eep_bin[i]) {
			print_str(10,180,1,Red,Black,"Verify Error!");
			return(1);
		}
		//_delay_ms(10);
	}
	print_str(10,180,1,Green,Black,"Done!");
	return(0);
}
