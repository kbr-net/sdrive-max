#include <stdio.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include "avrlibdefs.h"         // global AVRLIB defines
#include "avrlibtypes.h"        // global AVRLIB types definitions
#include "tape.h"
#include "display.h"
#include "fat.h"
#include "usart.h"

extern unsigned char atari_sector_buffer[];
extern struct FileInfoStruct FileInfo;
extern void Clear_atari_sector_buffer();

unsigned short block;
unsigned short baud;

void set_tape_baud () {
	//UBRR = (F_CPU/16/BAUD)-1 +U2X
	USART_Init(F_CPU/16/(baud/2)-1);
}
unsigned int send_tape_block (unsigned int offset) {
	unsigned char *p = atari_sector_buffer+BLOCK_LEN-1;
	unsigned char i,r;

        if (offset < FileInfo.vDisk->size) {	//data record
		sprintf_P((char*)atari_sector_buffer,PSTR("Block %u / %u "),offset/BLOCK_LEN+1,(FileInfo.vDisk->size-1)/BLOCK_LEN+1);
		print_str(35,132,2,Yellow,window_bg, (char*) atari_sector_buffer);
		//read block
                r = faccess_offset(FILE_ACCESS_READ,offset,BLOCK_LEN);
		//shift buffer 3 bytes right
		for(i = 0; i < BLOCK_LEN; i++) {
			*(p+3) = *p;
			p--;
		}
		if(r < BLOCK_LEN) {	//no full record?
			atari_sector_buffer[2] = 0xfa;	//mark partial record
			atari_sector_buffer[130] = r;	//set size in last byte
		}
		else
			atari_sector_buffer[2] = 0xfc;	//mark full record

		offset += r;
        }
	else {	//this is the last/end record
		print_str_P(35,132,2,Yellow,window_bg, PSTR("End  "));
		Clear_atari_sector_buffer(BLOCK_LEN+3);
		atari_sector_buffer[2] = 0xfe;	//mark end record
		offset = 0;
	}
	atari_sector_buffer[0] = 0x55;	//sync marker
	atari_sector_buffer[1] = 0x55;
	USART_Send_Buffer(atari_sector_buffer,BLOCK_LEN+3);
	USART_Transmit_Byte(get_checksum(atari_sector_buffer,BLOCK_LEN+3));
	_delay_ms(300);	//PRG(0-N) + PRWT(0.25s) delay
	return(offset);
}

void check_for_FUJI_file () {
	struct tape_FUJI_hdr *hdr = (struct tape_FUJI_hdr *)atari_sector_buffer;
	char *p = hdr->chunk_type;

	faccess_offset(FILE_ACCESS_READ,0,sizeof(struct tape_FUJI_hdr));
	if (    p[0] == 'F' &&          //search for FUJI header
		p[1] == 'U' &&
		p[2] == 'J' &&
		p[3] == 'I' )
	{
		tape_flags.FUJI = 1;
	}
	else {
		tape_flags.FUJI = 0;
	}

	if(tape_flags.turbo)	//set fix to
		baud = 1000; //1000 baud
	else
		baud = 600;
	set_tape_baud();

	block = 0;
	return;
}

unsigned int send_FUJI_tape_block (unsigned int offset) {
	unsigned short r;
	unsigned short gap, len;
	unsigned short buflen = 256;
	unsigned char first = 1;
	struct tape_FUJI_hdr *hdr = (struct tape_FUJI_hdr *)atari_sector_buffer;
	char *p = hdr->chunk_type;

        while (offset < FileInfo.vDisk->size) {
		//read header
		faccess_offset(FILE_ACCESS_READ,offset,
			sizeof(struct tape_FUJI_hdr));
		len = hdr->chunk_length;

		if (    p[0] == 'd' &&          //is a data header?
			p[1] == 'a' &&
			p[2] == 't' &&
			p[3] == 'a' )
		{
			block++;
			break;
		}
		else if(p[0] == 'b' &&          //is a baud header?
			p[1] == 'a' &&
			p[2] == 'u' &&
			p[3] == 'd' )
		{
			if(!tape_flags.turbo) {	//ignore baud hdr on turbo mode
				baud = hdr->irg_length;
				set_tape_baud();
			}
		}
		offset += sizeof(struct tape_FUJI_hdr) + len;
	}

	gap = hdr->irg_length;	//save GAP
	len = hdr->chunk_length;
	sprintf_P((char*)atari_sector_buffer,
		PSTR("Baud: %u Length: %u Gap: %u    "),baud, len, gap);
	print_str(15,153,1,Green,window_bg, (char*) atari_sector_buffer);

	while(gap--)
		_delay_ms(1);	//wait GAP

        if (offset < FileInfo.vDisk->size) {	//data record
		sprintf_P((char*)atari_sector_buffer,
			PSTR("Block %u     "),block);
		print_str(35,132,2,Yellow,window_bg,
			(char*) atari_sector_buffer);

		//read block
		offset += sizeof(struct tape_FUJI_hdr);	//skip chunk hdr
		while(len) {
			if(len > 256)
				len -= 256;
			else {
				buflen = len;
				len = 0;
			}
			r = faccess_offset(FILE_ACCESS_READ,offset,buflen);
			offset += r;
			USART_Send_Buffer(atari_sector_buffer,buflen);
			if(first && atari_sector_buffer[2] == 0xfe) {
				//most multi stage loaders starting over by self
				// so do not stop here!
				//tape_flags.run = 0;
				block = 0;
			}
			first = 0;
		}
		if(block == 0)
			_delay_ms(200);	//add an end gap to be sure
	}
	else {
		//block = 0;
		offset = 0;
	}
	return(offset);
}
