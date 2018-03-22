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

unsigned int send_tape_block (unsigned int offset) {
	unsigned char *p = atari_sector_buffer+block_len-1;
	unsigned char i,r;

        if (offset < FileInfo.vDisk->size) {	//data record
		sprintf_P((char*)atari_sector_buffer,PSTR("Block %u / %u "),offset/block_len+1,(FileInfo.vDisk->size-1)/block_len+1);
		print_str(35,135,2,Yellow,Light_Grey, atari_sector_buffer);
		//read block
                r = faccess_offset(FILE_ACCESS_READ,offset,block_len);
		//shift buffer 3 bytes right
		for(i = 0; i < block_len; i++)
			*(p+3) = *p--;
		if(r < block_len) {	//no full record?
			atari_sector_buffer[2] = 0xfa;	//mark partial record
			atari_sector_buffer[130] = r;	//set size in last byte
		}
		else
			atari_sector_buffer[2] = 0xfc;	//mark full record

		offset += r;
        }
	else {	//this is the last/end record
		print_str_P(35,135,2,Yellow,Light_Grey, PSTR("End  "));
		Clear_atari_sector_buffer(block_len+3);
		atari_sector_buffer[2] = 0xfe;	//mark end record
		offset = 0;
	}
	atari_sector_buffer[0] = 0x55;	//sync marker
	atari_sector_buffer[1] = 0x55;
	USART_Send_Buffer(atari_sector_buffer,block_len+3);
	USART_Transmit_Byte(get_checksum(atari_sector_buffer,block_len+3));
	_delay_ms(300);	//PRG(0-N) + PRWT(0.25s) delay
	return(offset);
}

unsigned int load_FUJI_file () {
	unsigned int offset = 0;
	struct tape_FUJI_hdr *hdr = (struct tape_FUJI_hdr *)atari_sector_buffer;
	char *p = hdr->chunk_type;

	faccess_offset(FILE_ACCESS_READ,offset,sizeof(struct tape_FUJI_hdr));
	if (    p[0] == 'F' &&          //search for FUJI header
		p[1] == 'U' &&
		p[2] == 'J' &&
		p[3] == 'I' )
	{
		tape_flags.FUJI = 1;
	}
	else {
		tape_flags.FUJI = 0;
		return(0);
	}

        while (offset < FileInfo.vDisk->size) {
		offset += sizeof(struct tape_FUJI_hdr) + hdr->chunk_length;
		faccess_offset(FILE_ACCESS_READ,offset,sizeof(struct tape_FUJI_hdr));
		if (    p[0] == 'd' &&          //search for data header
			p[1] == 'a' &&
			p[2] == 't' &&
			p[3] == 'a' )
		{
			block = 1;
			return(offset);
		}
	}
	return(0);
}

unsigned int send_FUJI_tape_block (unsigned int offset) {
	unsigned char r;
	unsigned int gap, len;
	struct tape_FUJI_hdr *hdr = (struct tape_FUJI_hdr *)atari_sector_buffer;

	//read header
	faccess_offset(FILE_ACCESS_READ,offset,sizeof(struct tape_FUJI_hdr));
	gap = hdr->irg_length;	//save GAP
	len = hdr->chunk_length;

	while(gap--)
		_delay_ms(1);	//wait GAP

        if (offset < FileInfo.vDisk->size) {	//data record
		sprintf_P((char*)atari_sector_buffer,PSTR("Block %u     "),block);
		print_str(35,135,2,Yellow,Light_Grey, atari_sector_buffer);
		//read block
		offset += sizeof(struct tape_FUJI_hdr);	//skip chunk hdr
                r = faccess_offset(FILE_ACCESS_READ,offset,len);
		offset += r;
		block++;
		USART_Send_Buffer(atari_sector_buffer,len);
		if(atari_sector_buffer[2] == 0xfe) {
			//most multi stage loaders starting over by self
			// so do not stop here!
			//tape_flags.run = 0;
			block = 1;
			_delay_ms(200);	//add an end gap to be sure
		}
	}
	else {
		block = 1;
		offset = 0;
	}
	return(offset);
}
