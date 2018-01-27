
// SDrive
// Bob!k & Raster, C.P.U., 2008
// Ehanced by kbr, 2015, 2017
//*****************************************************************************

#include <avr/io.h>		// include I/O definitions (port names, pin names, etc)
#include <avr/interrupt.h>	// include interrupt support
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <util/delay.h>

#include "avrlibdefs.h"		// global AVRLIB defines
#include "avrlibtypes.h"	// global AVRLIB types definitions
#include "spi.h"		// include SPI interface functions
#include "mmc.h"		// include MMC card access functions
#include "fat.h"
#include "sboot.h"
#include "usart.h"
#include "global.h"
#include "tft.h"
#include "touchscreen.h"
#include "display.h"
#include "atx.h"

//#define DATE		"20140519"
#define SWVERSIONMAJOR	0
#define SWVERSIONMINOR	4
//#define DEVID		0x53444e47
//#define DEVID		0x474e4453	// SDNG reverse!

#define TWOBYTESTOWORD(ptr)	*((u16*)(ptr))
#define FOURBYTESTOLONG(ptr)	*((u32*)(ptr))

#define Delay100usX(delay)	_delay_us(delay)
#define Delay100us()		_delay_us(100)
#define Delay200us()		_delay_us(200)
#define Delay800us()		_delay_us(800)
#define Delay1000us()		_delay_ms(1)

//F_CPU = 14318180
//UBRR = (F_CPU/16/BAUD)-1
//BAUD = (F_CPU/16/(UBRR+1)
//Pokey_frq = 1789790
//BAUD = Pokey_frq/2/(AUDF+7)		//(16bit register)

#define US_POKEY_DIV_STANDARD	0x28		//#40  => 19040 bps
						//=> 19231 bps #51
#define ATARI_SPEED_STANDARD	(US_POKEY_DIV_STANDARD+11)	//#51 (o sest vic)
//#define ATARI_SPEED_STANDARD	(US_POKEY_DIV_STANDARD+6)	//#46 (o sest vic)

#define US_POKEY_DIV_DEFAULT	0x06		//#6   => 68838 bps

#define US_POKEY_DIV_MAX		(255-6)		//pokeydiv 249 => avrspeed 255 (vic nemuze)

/*
#define SIOSPEED_MODES	9	//pocet fastsio_mode=0..8
#define US_POKEY_DIV_DEFAULT	US_POKEY_DIV_7
#define ATARI_SPEED_DEFAULT		ATARI_SPEED_7

//sio 1x standard
#define US_POKEY_DIV_STANDARD	0x28			//=> 19040 bps
#define ATARI_SPEED_STANDARD	46				// normal speed
//=46 => F_CPU/16/(46+1) = 19040

//sio2x
#define US_POKEY_DIV_2		0x10			//=> 38908 bps
#define ATARI_SPEED_2		22				// sio2x speed
//=22 => F_CPU/16/(22+1) = 38908

//Happy
#define US_POKEY_DIV_3		0x0a			//=> 52641 bps
#define ATARI_SPEED_3		16				// happy
//=16 => F_CPU/16/(16+1) = 52640

//Speedy
#define US_POKEY_DIV_4		0x09			//=> 55931 bps
#define ATARI_SPEED_4		15				// speedy TOTO FUNGUJE
//=15 => F_CPU/16/(15+1) = 55930

//sio3x
#define US_POKEY_DIV_5		0x08			//=> 59660 bps
#define ATARI_SPEED_5		14				// sio3x speed
//=14 => F_CPU/16/(14+1) = 59659

//sio 64000
#define US_POKEY_DIV_6		0x07			//=> 63921 bps
#define ATARI_SPEED_6		13				// sio3x speed
//=13 => F_CPU/16/(13+1) = 63920

// works great!
#define US_POKEY_DIV_7		0x06			//=> 68838 bps
#define ATARI_SPEED_7		12				// ULTRA speed - closest match for both systems (AVR,ATARI)
//=12 => F_CPU/16/(12+1) = 68837

//sio 75000
#define US_POKEY_DIV_8		0x05			//=> 74575 bps
#define ATARI_SPEED_8		11				// sio4x speed
//=11 => F_CPU/16/(11+1) = 74574
*/

unsigned char debug = 0;
unsigned char mmc_sector_buffer[512];	// one SD sector
u32 n_actual_mmc_sector;
unsigned char n_actual_mmc_sector_needswrite;
unsigned char atari_sector_buffer[256];

#define FileFindBuffer (atari_sector_buffer+256-11)		//pri vyhledavani podle nazvu
char DebugBuffer[14];

struct GlobalSystemValues GS;
struct FileInfoStruct FileInfo;			//< file information for last file accessed

extern struct display tft;
extern unsigned char actual_page;
extern unsigned char file_selected;

//workaround to get version numbers converted to strings
#define STR_A(x)	#x
#define STR(x)	STR_A(x)

const uint8_t PROGMEM system_info[] = "SDrive" STR(SWVERSIONMAJOR) STR(SWVERSIONMINOR) " " STR(DATE) " by KBr";	//SDriveVersion info

//uint8_t EEMEM system_info[]="SDrive03 20140519 by kbr";	//SDriveVersion info
//                                 VVYYYYMMDD
//                                 VV cislo nove oficialne vydane verze, meni se jen pri vydani noveho oficialniho firmware
//									  s rozsirenymi/zmenenymi funkcemi zpetne nekompatibilni
//									  rostouci posloupnost 01 .. 09 0A 0B .. 0Z .. 0a 0b .. 0y 0z 10 11 ..... zz

//nasledujici parametry se daji modifiovat pres EEPROM configure
//                              |filenameext|
uint8_t EEMEM system_atr_name[]="SDRIVE  ATR";  //8+3 zamerne deklarovano za system_info,aby bylo pripadne v dosahu pres get status
//
uint8_t EEMEM system_fastsio_pokeydiv_default=US_POKEY_DIV_DEFAULT;

/*
1) 720*128=92160	   (  90,000KB)
2) 1040*128=133120         ( 130,000KB)
3) 3*128+717*256=183936    ( 179,625KB)
4) 3*128+1437*256=368256   ( 359,625KB)
5) 3*128+2877*256=736896   ( 719,625KB)
6) 3*128+5757*256=1474176  (1439,625KB)
7) 3*128+11517*256=2948736 (2879,625KB)
8) 3*128+...
*/
#define IMSIZE1	92160
#define IMSIZE2	133120
#define IMSIZE3	183936
#define IMSIZE4	368256
#define IMSIZE5	736896
#define	IMSIZE6	1474176
#define IMSIZE7	2948736
#define IMSIZES 7

uint8_t EEMEM system_percomtable[]= {
	0x28,0x01,0x00,0x12,0x00,0x00,0x00,0x80, IMSIZE1&0xff,(IMSIZE1>>8)&0xff,(IMSIZE1>>16)&0xff,(IMSIZE1>>24)&0xff,
	0x28,0x01,0x00,0x1A,0x00,0x04,0x00,0x80, IMSIZE2&0xff,(IMSIZE2>>8)&0xff,(IMSIZE2>>16)&0xff,(IMSIZE2>>24)&0xff,
	0x28,0x01,0x00,0x12,0x00,0x04,0x01,0x00, IMSIZE3&0xff,(IMSIZE3>>8)&0xff,(IMSIZE3>>16)&0xff,(IMSIZE3>>24)&0xff,
	0x28,0x01,0x00,0x12,0x01,0x04,0x01,0x00, IMSIZE4&0xff,(IMSIZE4>>8)&0xff,(IMSIZE4>>16)&0xff,(IMSIZE4>>24)&0xff,
	0x50,0x01,0x00,0x12,0x01,0x04,0x01,0x00, IMSIZE5&0xff,(IMSIZE5>>8)&0xff,(IMSIZE5>>16)&0xff,(IMSIZE5>>24)&0xff,
	0x50,0x01,0x00,0x24,0x01,0x04,0x01,0x00, IMSIZE6&0xff,(IMSIZE6>>8)&0xff,(IMSIZE6>>16)&0xff,(IMSIZE6>>24)&0xff,
	0x50,0x01,0x00,0x48,0x01,0x04,0x01,0x00, IMSIZE7&0xff,(IMSIZE7>>8)&0xff,(IMSIZE7>>16)&0xff,(IMSIZE7>>24)&0xff,
	0x01,0x01,0x00,0x00,0x00,0x04,0x01,0x00, 0x00,0x00,0x00,0x00
	};

#define DEVICESNUM	5	//	//D0:-D4:
virtual_disk_t vDisk[DEVICESNUM];

virtual_disk_t tmpvDisk;

struct SDriveParameters
{
	u08 p0;
	u08 p1;
	u08 p2;
	u08 p3;
	u32 p4_5_6_7;
};

struct sio_cmd {
	u08 dev;
	u08 cmd;
	union {
		struct {
			u08 aux1;
			u08 aux2;
		};
		u16 aux;
	};
	u08 cksum;
};

struct ATR_header {
	u16 magic;
	u16 pars;
	u16 secsize;
	u08 parshigh;
	u32 crc;
	u32 unused;
	u08 flags;
};

//sdrparams je nadeklarovano uvnitr main() (hned na zacatku)
#define actual_drive_number	sdrparams.p0
#define fastsio_active		sdrparams.p1
#define fastsio_pokeydiv	sdrparams.p2
#define bootloader_relocation	sdrparams.p3
#define extraSDcommands_readwritesectornumber	sdrparams.p4_5_6_7

#define LED_RED_ON(x) drive_led(x,2)
#define LED_GREEN_ON(x) drive_led(x,1)
#define LED_GREEN_OFF(x) drive_led(x,0)
#define LED_RED_OFF(x) drive_led(x,0)

void drive_led(unsigned char drive, unsigned char on) {
	unsigned int col = Grey;

	if(actual_page != 0)
		return;

	struct button *b = &tft.pages[0].buttons[drive];

	if(b->selected)
		col = Blue;

	switch(on) {
		default:
			Draw_Circle(b->x+5,b->y+5,3,1,col);
			break;

		case 1:
			Draw_Circle(b->x+5,b->y+5,3,1,Green);
			break;
		case 2:
			Draw_Circle(b->x+5,b->y+5,3,1,Red);
	}
}

// DISPLAY

void set_display(unsigned char n)
{
	unsigned char i;

	//clear all selections
	for(i = 0; i < tft.pages[0].nbuttons; i++)
		tft.pages[0].buttons[i].selected = 0;

	tft.pages[0].buttons[n].selected = 1;
	switch(n) {
		default:
			tft.pages[0].buttons[1].name[1] = '1';
			tft.pages[0].buttons[2].name[1] = '2';
			tft.pages[0].buttons[3].name[1] = '3';
			tft.pages[0].buttons[4].name[1] = '4';
			break;
		case 2:
			tft.pages[0].buttons[1].name[1] = '2';
			tft.pages[0].buttons[2].name[1] = '1';
			tft.pages[0].buttons[3].name[1] = '3';
			tft.pages[0].buttons[4].name[1] = '4';
			break;
		case 3:
			tft.pages[0].buttons[1].name[1] = '3';
			tft.pages[0].buttons[2].name[1] = '2';
			tft.pages[0].buttons[3].name[1] = '1';
			tft.pages[0].buttons[4].name[1] = '4';
			break;
		case 4:
			tft.pages[0].buttons[1].name[1] = '4';
			tft.pages[0].buttons[2].name[1] = '2';
			tft.pages[0].buttons[3].name[1] = '3';
			tft.pages[0].buttons[4].name[1] = '1';
	}
	//redraw display only, if we are on main page
	if(actual_page == 0)
		draw_Buttons();
}

u08 newFile (u32 size) {	// size without ATR header
	u32 sec;
	struct ATR_header hdr;
	memset(&hdr,0,sizeof(hdr));	//clear it

	//sec = fatFileNew(133136);
	sec = fatFileNew(size+16);
	if(sec) {	// write ATR-Header
		hdr.magic = 0x0296;
		hdr.pars = size/16;
		if(size > 130*1024UL)
			hdr.secsize = 0x0100;
		else
			hdr.secsize = 0x0080;
		mmcReadCached(sec);
		memcpy((char*)mmc_sector_buffer,(char*)&hdr,sizeof(hdr));
		//mmcWrite(sec);
		mmcWriteCached(0);
		return(0);
	}
	else {
		//blink(2);
		outbox_P(PSTR("error: create new file"));
		return(1);
	}
}

void PutDecimalNumberToAtariSectorBuffer(unsigned char num, unsigned char pos)
{
	u08 *ptr;
	ptr = (atari_sector_buffer+pos);
	*ptr++=0x30|(num/10);
	*ptr=0x30|(num%10);
}

void Clear_atari_sector_buffer(u16 len)
{
	//Maze atari_sector_buffer
	u08 *ptr;
	ptr=atari_sector_buffer;
	do { *ptr++=0; len--; } while (len);
}

#define Clear_atari_sector_buffer_256()	Clear_atari_sector_buffer(256)
/*
void Clear_atari_sector_buffer_256()
{
	Clear_atari_sector_buffer(256); //0 => maze 256 bytu
}
*/

////some more globals
struct sio_cmd cmd_buf;
unsigned char virtual_drive_number;
//Parameters
struct SDriveParameters sdrparams;

void process_command();	//define, because it's after main!

void sio_debug (char status) {
	//print the last cmd
	sprintf_P(DebugBuffer, PSTR("%.2x %.2x %.2x %.2x %c"), cmd_buf.dev, cmd_buf.cmd, cmd_buf.aux1, cmd_buf.aux2, status);
	outbox(DebugBuffer);
}

//----- Begin Code ------------------------------------------------------------
int main(void)
{
	// command-pin
	CMD_PORTREG |= 1 << CMD_PIN;	// with pullup
	CMD_DDR &= ~(1 << CMD_PIN);	// to input

	//interrupts
	PCICR = (1<<PCIE1);
	PCMSK1 = (1<<PCINT13);		// for CMD_PIN

//SD_CARD_EJECTED:

	fastsio_active=0;
	bootloader_relocation=0;
	actual_drive_number=0;	//nastavovano kousek dal (za SET_BOOT_DRIVE)
	virtual_drive_number=0;
	FileInfo.percomstate=0;		//inicializace
	//fastsio_pokeydiv=US_POKEY_DIV_DEFAULT;		//default fastsio
	fastsio_pokeydiv=eeprom_read_byte(&system_fastsio_pokeydiv_default); //definovano v EEPROM

	tft_Setup();
	tft.pages[0].draw();	//draw main page

	USART_Init(ATARI_SPEED_STANDARD);

	LED_GREEN_ON(virtual_drive_number);	// LED on

/*	TODO, but how, if we have no card detect signal?
	//while (inb(PIND)&0x04);	// wait for sd-card inserted
	while (inb(PIND)&0x04)	// wait for sd-card inserted
	{
		if (get_cmd_L() ) {
			debug(PSTR("Please insert SD-Card!"));
		}
	}
	//_delay_ms(250);		// wait for power-up
*/
	//pozor, v tomto okamziku nesmi byt nacacheovan zadny sektor
	//pro zapis, protoze init karty inicializuje i mmc_sector atd.!!!

	mmcInit();
	_delay_ms(100);		// wait for power-up
	u08 r = mmcReset();
	if (r) {				// error on sd-card init
		sprintf_P((char*)atari_sector_buffer, PSTR("Error init SD: %u"), r);
		outbox((char*)atari_sector_buffer);
		sprintf_P((char*)atari_sector_buffer, PSTR("%.08lx %.08lx"),
		    *(long*)mmc_sector_buffer, *(long*)&mmc_sector_buffer[4]);
		outbox((char*)atari_sector_buffer);
		//goto SD_CARD_EJECTED;
		goto ST_IDLE;
	}

	// set SPI speed on maximum (F_CPU/2)
	sbi(SPSR, SPI2X);
	cbi(SPCR, SPR1);
	cbi(SPCR, SPR0);

	//reset flags, all drives off
	{
	 u08 i;
	 for(i=0; i<DEVICESNUM; i++) vDisk[i].flags=0;
	}
	//FileInfo.vDisk->flags=0;		//special_xex_loader=0;

	//actual_drive_number=0;	//pro bootovani z jednotky vD0:

	r = fatInit();
	if (r)
	{
		sprintf_P((char*)atari_sector_buffer, PSTR("Error init FAT: %u"), r);
		outbox((char*)atari_sector_buffer);
		goto ST_IDLE;
	}

SET_SDRIVEATR_TO_D0:	//pro nastaveni SDRIVE.ATR do vD0: bez zmeny actual_drive_number !

	//set vDisk Ptr to drive D0:
	FileInfo.vDisk = &vDisk[0];
	//Vyhledani a nastaveni image SDRIVE.ATR do vD0:
	{
		unsigned short i;
		unsigned char m;

		i=0;
		while( fatGetDirEntry(i,0) )
		{
			for(m=0;m<11;m++)	//8+3
			{
				if(atari_sector_buffer[m]!=eeprom_read_byte(&system_atr_name[m]))
				{
					//odlisny znak
					goto find_sdrive_atr_next_entry;
				}
			}

			//Slava, nasel SDRIVE.ATR
			FileInfo.vDisk->current_cluster=FileInfo.vDisk->start_cluster;
			FileInfo.vDisk->ncluster=0;
			//FileInfo.vDisk->file_index = i; //dela se uvnitr fatGetDirEntry

			faccess_offset(FILE_ACCESS_READ,0,16); //ATR hlavicka vzdy

			FileInfo.vDisk->flags=FLAGS_DRIVEON;
			if ( (atari_sector_buffer[4]|atari_sector_buffer[5])==0x01 )
				FileInfo.vDisk->flags|=FLAGS_ATRDOUBLESECTORS;

			{
				unsigned long compute;
				unsigned long tmp;

				compute = FileInfo.vDisk->size - 16;		//je to vzdy ATR
				tmp = (FileInfo.vDisk->flags & FLAGS_ATRDOUBLESECTORS)? 0x100:0x80;
				compute /=tmp;

				if(compute>720) FileInfo.vDisk->flags|=FLAGS_ATRMEDIUMSIZE;
			}
			goto find_sdrive_atr_finished;
			//
find_sdrive_atr_next_entry:
			i++;
		} //end while

		//nenasel SDRIVE.ATR
		FileInfo.vDisk->flags=0; //ve vD0: neni aktivni disk
		//takze nastavi jednotku dle cisla SDrive (1-4)
		if (actual_drive_number==0)
		{
		 //actual_drive_number=(unsigned char)4-((unsigned char)(inb(PINB))&0x03);
		 actual_drive_number=0;
		}
		outbox_P(PSTR("SDRIVE.ATR not found"));
		//goto SD_CARD_EJECTED;
	}
find_sdrive_atr_finished:

	set_display(actual_drive_number);

/////////////////////

	unsigned long autowritecounter = 0;
	unsigned int select_file_counter = 0;
	unsigned char *sfp;
ST_IDLE:
	sfp = atari_sector_buffer;

	LED_GREEN_OFF(virtual_drive_number);  // LED OFF
	sei();	//enable interrupts

	//Mainloop: Wait for touchscreen input
	//Atari command is triggered by interrupt

	while(1)
	{
		struct button *b;
		unsigned int de;
		unsigned char drive_number;

		if (isTouching()) {
			b = check_Buttons();
			if (b) {
				cli();	//no interrupts so long we work on vDisk struct
				//display use tmp struct
				FileInfo.vDisk = &tmpvDisk;
				//remember witch D*-button on main page
				// we have pressed
				if(actual_page == 0 && b->name[0] == 'D')
					drive_number = b-tft.pages[0].buttons;
				//call the buttons function
				de = b->pressed(b);
				sei();
				//check if actual_drive has changed
				if (actual_drive_number != drive_number && b->selected) {
					actual_drive_number = drive_number;
					//if(b->name[1] == '0')
					if(drive_number == 0)
						goto SET_SDRIVEATR_TO_D0;
					set_display(actual_drive_number);
				}
				if(de) {	//if a direntry was returned
					cli();
					//was it the deactivation flag?
					if(de == -1) { // &&
					   //(vDisk[drive_number].flags & FLAGS_DRIVEON)) {
						cmd_buf.cmd = 0xE2;
						cmd_buf.aux1 = drive_number;
					}
					//or set cmd to change image
					else {
						cmd_buf.cmd = (0xF0 | drive_number);
						cmd_buf.aux = de;
					}
					cmd_buf.dev = 0x71;	//say we are a sdrive cmd
					process_command();
					sei();
				}
				//it was the N[ew]-Button? Create new file
				//(reset is done in deactivate drive)
				if(actual_page == 0 && b->name[0] == 'N' &&
				   actual_drive_number != 0) {
					vDisk[actual_drive_number].flags |= (FLAGS_ATRNEW);	// | FLAGS_DRIVEON);
					strncpy_P(&tft.pages[0].buttons[actual_drive_number].name[3], PSTR(">New<       "), 12);
					draw_Buttons();
				}
				sfp = atari_sector_buffer;
				//if matched, wait for button release
				while (isTouching());
				_delay_ms(50);	//wait a little for debounce
			}
		}

		if (tft.cfg.scroll && actual_page == 1 && file_selected != -1 && strlen(atari_sector_buffer) > 19) {
			if (select_file_counter > 20000) {
				sfp++;
				Draw_Rectangle(5,10,tft.width-1,26,1,SQUARE,Black,Black);
				print_strn(5, 10, 2, Yellow, Black, sfp, 19);
				select_file_counter = 0;
				if (strlen(sfp) == 19)
					sfp = atari_sector_buffer-1;
			}
			else
				select_file_counter++;
		}

		if (autowritecounter>1700000)
		{
			//  100'000 too less
			//1'000'000 about 1.75 seconds
			//2'000'000 about 3.5 seconds
			//=>1s = 571428
			//1'700'000 about 3 seconds
			mmcWriteCachedFlush(); //If you want some sector to write, write it down
			//goto autowritecounter_reset;
			autowritecounter=0;
		}
		else
			autowritecounter++;

	} //while
	return(0);
} //main

ISR(PCINT1_vect)
{
	if(CMD_PORT & (1<<CMD_PIN))	//do nothing on high
		return;

	//save actual vDisk pointer
	virtual_disk_t *vp = FileInfo.vDisk;

	cmd_buf.cmd = 0;	//clear cmd to allow read from atari
	process_command();
	LED_GREEN_OFF(virtual_drive_number);  // LED OFF
	FileInfo.vDisk = vp;	//restore vDisk pointer
}

//////process command frame

void process_command ()
{
	u32 *asb32_p = (u32*) atari_sector_buffer;

	if(!cmd_buf.cmd)
	{
		u08 err;
		err=USART_Get_Buffer_And_Check((unsigned char*)&cmd_buf,4,CMD_STATE_L);

		//It was due to timeout?
		if (err&0x02) {
			outbox_P(PSTR("SIO:CMD Timeout"));
			return; //exit. If Atari is off, it will hang here otherwise
		}

		//If the reason is to change cmd to H, do not wait to wait_cmd_LH ()
		//if (err&0x01) goto change_sio_speed; //The cmd rises to H, and the speed changes immediately

		Delay800us();	//t1 (650-950us) (Without this pause it does not work!!!)
		wait_cmd_LH();	//Wait until the signal command rises to H
		////dou to LED function never needed, i think
		//Delay100us();	//T2=100   (After lifting the command and before the ACK)

		if(err)
		{
			if (fastsio_pokeydiv!=US_POKEY_DIV_STANDARD)
			{
				//Convert from standard to fast or vice versa
				fastsio_active=!fastsio_active;
			}

change_sio_speed_by_fastsio_active:
			{
			 u08 as;
			 as=ATARI_SPEED_STANDARD;	//default speed
			 //if (fastsio_active) as=fastsio_pokeydiv+6;		//always about 6 vic
			 if (fastsio_active) as=fastsio_pokeydiv+8;

			 USART_Init(as);
			}
			return;
		}
	}

/////////////////////////////// disk commands

	if( cmd_buf.dev>=0x31 && cmd_buf.dev<(0x30+DEVICESNUM) ) //D1: to D4: (yes, from D1: !!!)
	{
		// Only D1: from D4: (from 0x31 to 0x34)
		// But via the SDrive (0x71 to 0x74 by the sdrive number)
		// always approach to vD0:

		// DISCONNECT OPERATION D0: to D4:
		//unsigned char virtual_drive_number;

		//if( cmd_buf.dev == (unsigned char)0x31+3-((unsigned char)(inb(PINB))&0x03) )
		if( cmd_buf.dev == 0x31)
		{
			//Wants to clean the Dx drive (D1: up to D4 :) which is dashing
		    virtual_drive_number = actual_drive_number;
		}
		else
		if ( cmd_buf.dev == (unsigned char)0x30+actual_drive_number )
		{
			//Wants to clean the drive Dx (D0: to D4 :) which is discarded with the drive number
			//virtual_drive_number = 4-((unsigned char)(inb(PINB))&0x03);
			virtual_drive_number = 4-3;
		}
		else
		{
			//Wants to clean from some other Dx:
disk_operations_direct_d0_d4:
			virtual_drive_number = cmd_buf.dev&0xf;
		}

		// vDisk <- vDisk[virtual_drive_number]
		FileInfo.vDisk = &vDisk[virtual_drive_number];

		if (!(FileInfo.vDisk->flags & FLAGS_DRIVEON) && !(FileInfo.vDisk->flags & FLAGS_ATRNEW))
		{
			//No disk is inserted into the Dx
			return;
		}

		//LED_GREEN_ON(virtual_drive_number);	// LED on

		//For anything other than 0x53 (get status), FLAGS_WRITEERROR is reset
		if (cmd_buf.cmd!=0x53) FileInfo.vDisk->flags &= (~FLAGS_WRITEERROR); //Resets the FLAGS_WRITEERROR bit

		//Formatting commands are processed in the preload here for ACK / NACK
		switch(cmd_buf.cmd)
		{
		case 0x21:	// format single + PERCOM
			//Single for 810 and stock 1050 or as defined by CMD $4F for upgraded drives (Speedy, Happy, HDI, Black Box, XF 551).
			//Returns 128 bytes (SD) or 256 bytes (DD) after format. Format ok requires that the first two bytes are $FF. It is said to return bad sector list (never seen one).

		   {	//Beginning of the section over two cases, 0x21,0x22
		    u08 formaterror;

			{
			 u32 singlesize = IMSIZE1;
			 if (FileInfo.vDisk->flags & FLAGS_ATRNEW) {	//create new image
				u08 err;
				send_ACK();
				LED_RED_ON(virtual_drive_number); // LED on
				if (FileInfo.percomstate == 2)	//XXX: Could not work until image exists!
					err = newFile(IMSIZE3);
				else
					err = newFile(singlesize);
				if(err)
					goto Send_NACK_and_set_FLAGS_WRITEERROR_and_ST_IDLE;

				else {
					// set new file to drive
					//cmd_buf.cmd = 0xff;
					cmd_buf.cmd = (0xf0 | virtual_drive_number);
					//remember direntry
					cmd_buf.aux = FileInfo.vDisk->file_index;
					goto Command_EC_F0_FF_found;
				}
			 }
			 if ( !(FileInfo.vDisk->flags & FLAGS_XFDTYPE) ) singlesize+=(u32)16; //for ATR add header size(16)

			 if (   ( FileInfo.percomstate==3 ) //He endeavored to write in the wrong way percom
				|| ( (FileInfo.percomstate==0) && (FileInfo.vDisk->size!=singlesize) ) //No single disk
			   )
			 {
				goto Send_NACK_and_set_FLAGS_WRITEERROR_and_ST_IDLE;
			 }
			}

format_new:
			{
				unsigned short i,size;
				if ( FileInfo.percomstate == 2)
				{
					size=256;
				}
				else
				{
format_medium:
					size=128;
				}

				FileInfo.percomstate=0; //after the first format percom has no effect

				//XEX can not be formatted
				if (FileInfo.vDisk->flags & FLAGS_XEXLOADER)
				{
					goto Send_NACK_and_set_FLAGS_WRITEERROR_and_ST_IDLE;
				}

				//check for new image
				if (FileInfo.vDisk->flags & FLAGS_ATRNEW) {	//new image created
					FileInfo.vDisk->flags &= ~FLAGS_ATRNEW;	//reset NEW flag
					//vDisk[virtual_drive_number].flags &= ~FLAGS_ATRNEW;	//also in vDisk
					//blink_off(1<<(4-virtual_drive_number));	//TODO
					//set_display(virtual_drive_number);
				}
				else {
					send_ACK();
					LED_RED_ON(virtual_drive_number); // LED on
				}

				formaterror=0;

				{
				 //Resets the formatted file
				 Clear_atari_sector_buffer_256();
				 {
				  unsigned long maz,psize;
				  maz=0;
				  if ( !(FileInfo.vDisk->flags & FLAGS_XFDTYPE) ) maz+=16;
				  while( (psize=(FileInfo.vDisk->size-maz))>0 )
				  {
				   if (psize>256) psize=256;
				   if (!faccess_offset(FILE_ACCESS_WRITE,maz,(u16)psize))
				   {
					formaterror=1; //Failed to write
					break;
				   }
				   maz+=psize;
				  }
				 }
				}
				
				//Predicts the values 0xff or 0x00 in the return sector (what returns the format command)
				//and set FLAGS_WRITEERROR on error
				{
				 u08 *ptr;
				 ptr=atari_sector_buffer;
				 i=size;
				 do { *ptr++=0xff; i--; } while(i>0);
				 if (formaterror)
				 {
				  atari_sector_buffer[0]=0x00; //err
				  atari_sector_buffer[1]=0x00; //err
				  FileInfo.vDisk->flags |= FLAGS_WRITEERROR;	//set FLAGS_WRITEERROR
				  //blink(3);
				  outbox_P(PSTR("error: write after format"));
				 }
				}

				//send answer to Atari

				USART_Send_cmpl_and_atari_sector_buffer_and_check_sum(size);
				return; //no more work
			}
			break;

		case 0x22: // format medium
			// 	Formats medium density on an Atari 1050. Format medium density cannot be achieved via PERCOM block settings!
			if (FileInfo.vDisk->flags & FLAGS_ATRNEW) {	//create new image
				send_ACK();
				LED_RED_ON(virtual_drive_number); // LED on
				if(newFile(IMSIZE2))
					goto Send_NACK_and_set_FLAGS_WRITEERROR_and_ST_IDLE;
				else {
					// set new file to drive
					//cmd_buf.cmd = 0xff;
					cmd_buf.cmd = (0xf0 | virtual_drive_number);
					//remember direntry
					cmd_buf.aux = FileInfo.vDisk->file_index;
					goto Command_EC_F0_FF_found;
				}
			}

			if (! (FileInfo.vDisk->flags & FLAGS_ATRMEDIUMSIZE))
			{
				//FileInfo.percomstate=0;
				//goto Send_ERR_and_Delay;
Send_NACK_and_set_FLAGS_WRITEERROR_and_ST_IDLE:
				FileInfo.vDisk->flags |= FLAGS_WRITEERROR;	//set FLAGS_WRITEERROR
				goto Send_NACK_and_ST_IDLE;
			}

			goto format_medium;
		   } //End of section over two cases, 0x21,0x22
		   break;

	   case 0x50: //(write)
	   case 0x57: //(write verify)
			//for these commands no LED_GREEN_ON !
			LED_RED_ON(virtual_drive_number); // LED on
			goto device_command_accepted;

	   default:
			//For all the other red green LEDs
			LED_GREEN_ON(virtual_drive_number); // LED on

		}
		//It's some other than the supported command?
		if( (
		     (cmd_buf.cmd!=0x52) 
			 //&& (cmd_buf.cmd!=0x50) //Compare the pre-set (switch) to the device_command_accepted;
			 && (cmd_buf.cmd!=0x53) 
			 //&& (cmd_buf.cmd!=0x57) //Compare the pre-set (switch) to the device_command_accepted;
			 //&& (cmd_buf.cmd!=0x3f) //Processed within this condition
			 && (cmd_buf.cmd!=0x4e) && (cmd_buf.cmd!=0x4f)
			 //&& (cmd_buf.cmd!=0x21) && (cmd_buf.cmd!=0x22) //Already processed before
		    )
		    || (FileInfo.vDisk->flags & FLAGS_ATRNEW)	//no other commands on newfile
		)
		{
			if (cmd_buf.cmd==0x3f && fastsio_pokeydiv!=US_POKEY_DIV_STANDARD) goto device_command_accepted;
Send_NACK_and_ST_IDLE:
			send_NACK();
			return;
		}
device_command_accepted:

		send_ACK();
//			Delay1000us();	//delay_us(COMMAND_DELAY);

		switch(cmd_buf.cmd)
		{

		case 0x3f:	// read speed index
			{
				atari_sector_buffer[0]=fastsio_pokeydiv;	//write Pokey divisor
				USART_Send_cmpl_and_atari_sector_buffer_and_check_sum(1);
				/*
				Delay800us();	//t5
				send_CMPL();
				//Delay800us();	//t6 <-- This was not working here, for Fast had to boot under Qmegem4
				Delay200us();
				USART_Transmit_Byte(pdiv);	//US_POKEY_DIV);
				Delay100usX(6);	//special 600us
				USART_Transmit_Byte(pdiv);	//US_POKEY_DIV);	// check_sum
				*/
				/*
				send_CMPL();
				Delay200us(); 	//delay_us(200); //delay_us(COMMAND_DELAY); doesn't work in Qmeg 3.2!
				USART_Transmit_Byte(US_POKEY_DIV);
				USART_Transmit_Byte(US_POKEY_DIV);	// check_sum
				*/
			}
			break;

		case 0x4e:	// read cfg (PERCOM)

/*
byte 0:    Tracks per side (0x01 for HDD, one "long" track)
byte 1:    Interface version (0x10)
byte 2:    Sectors/Track - high byte
byte 3:    Sectors/Track - low byte
byte 4:    Side Code (0 - single sided)
byte 5:    Record method (0=FM, 4=MFM double)
byte 6:    High byte of Bytes/Sector (0x01 for double density)
byte 7:    Low byte of Bytes/Sector (0x00 for double density)
byte 8:    Translation control (0x40)
	  bit 6: Always 1 (to indicate drive present)
bytes 9-11 Drive interface type string "IDE"
*/

/*
			atari_sector_buffer[0] = 40; //0x01; // HDD, one track
			atari_sector_buffer[1] =  0; //0x01; // version 0.1
			{
				unsigned long s;
				// There is no need to substract the header of the ATR, since it will be the same at the div of 40!
				s= FileInfo.vDisk->size / 40;
				if (FileInfo.vDisk->flags & FLAGS_ATRDOUBLESECTORS)
				 s=(s>>8)+1; //s=s/256; +1, Because the first 3 sectors are 128 and so 384 should be added to the size, and then it will divide 40 and 256, but it will be the first one to the following sectors
				else
				 s=(s>>7); //s=s/128;
				atari_sector_buffer[2] = (s>>8) & 0xff;   //hb sectors
				atari_sector_buffer[3] = ( s & 0xff ); //lb sectors
				atari_sector_buffer[4] = (s>>16) & 0xff; //nr. of sides - 1  (0=> one side)
			}
			//[5] =0 for single, =4 for mediumtype or doublesize sectors
			atari_sector_buffer[5] = (FileInfo.vDisk->flags & (FLAGS_ATRMEDIUMSIZE|FLAGS_ATRDOUBLESECTORS))? 0x04 : 0x00;
			if (FileInfo.vDisk->flags & FLAGS_ATRDOUBLESECTORS)
			{
				atari_sector_buffer[6]=0x01;	//sector size HB
				atari_sector_buffer[7]=0x00;	//sector size LB
			}
			else
			{
				atari_sector_buffer[6]=0x00;	//sector size HB
				atari_sector_buffer[7]=0x80;	//sector size LB
			}
			atari_sector_buffer[8]  = 0x01;	//0x01; //0x40; // drive exist
			atari_sector_buffer[9]  = 'S';	//0x41;	//0xc0; //'I'; 
			atari_sector_buffer[10] = 'D';	//0x00;	//0xf2; //'D'; 
			atari_sector_buffer[11] = 'r';	//0x00;	//0x42; //'E'; 
*/
			//XXXXXXXXXXXXXXXXXXXXXX
		{
			u08 *ptr;
			u08 isxex;
			u16 secsize;
			u32 fs;

			isxex = ( FileInfo.vDisk->flags & FLAGS_XEXLOADER );

			fs=FileInfo.vDisk->size;

			secsize=(FileInfo.vDisk->flags & FLAGS_ATRDOUBLESECTORS)? 0x100:0x80;
			
			ptr = system_percomtable;

			//if ( isxex ) ptr+=(IMSIZES*12);

			do
			{
			 u08 m;
			 //12 values per row in system_percomtable
			 //0x28,0x01,0x00,0x12,0x00,0x00,0x00,0x80, IMSIZE1&0xff,(IMSIZE1>>8)&0xff,(IMSIZE1>>16)&0xff,(IMSIZE1>>24)&0xff,
			 //...
			 //0x01,0x01,0x00,0x00,0x00,0x04,0x01,0x00, 0x00,0x00,0x00,0x00
			 for(m=0;m<12;m++) atari_sector_buffer[m]=eeprom_read_byte(ptr++);
			 if (    (!isxex) 
				  && ( FOURBYTESTOLONG(atari_sector_buffer+8)==(fs & 0xffffff80) )  // &0xff... Due to the deletion of the eventual 16 ATR headings
				  && ( atari_sector_buffer[6]==(secsize>>8) ) //sectorsize hb
				)
			 {
			  //File size and sector consent
			   goto percom_prepared;
			 }
			} while (atari_sector_buffer[0]!=0x01);

			//no known floppy format, set size and number of sectors
			atari_sector_buffer[6]=(secsize>>8);	//hb
			atari_sector_buffer[7]=(secsize&0xff);	//db

			if ( isxex )
			{
			 secsize=125; //-=3; //=125
			 fs+=(u32)46125;		//((u32)0x171*(u32)125);
			}
			if ( FileInfo.vDisk->flags & FLAGS_ATRDOUBLESECTORS )
			{
			 fs+=(u32)(3*128); //+384bytes for the first three sectors of 128bytes
			}

			//FOURBYTESTOLONG(atari_sector_buffer+8)=fs; //DEBUG !!!!!!!!!!!

			fs/=((u32)secsize); //convert filesize to sectors

			atari_sector_buffer[2] = ((fs>>8) & 0xff);	//hb sectors
			atari_sector_buffer[3] = (fs & 0xff );		//lb sectors
			atari_sector_buffer[4] = ((fs>>16) & 0xff);	//sides - 1  (0=> one side)

percom_prepared:
//*
			atari_sector_buffer[8]  = 0x01;	//0x01; //0x40; // drive exist
			atari_sector_buffer[9]  = 'S';	//0x41;	//0xc0; //'I'; 
			atari_sector_buffer[10] = 'D';	//0x00;	//0xf2; //'D'; 
			atari_sector_buffer[11] = 'r';	//0x00;	//0x42; //'E'; 
// */
		}

			USART_Send_cmpl_and_atari_sector_buffer_and_check_sum(12);

			break;


		case 0x4f:	// write cfg (PERCOM)

			if (USART_Get_atari_sector_buffer_and_check_and_send_ACK_or_NACK(12))
			{
				break;
			}

			//check, if the size in percom is equal to the image size
			{
				u32 s;
				s = ((u32)atari_sector_buffer[0])
					 * ( (u32) ( (((u16)atari_sector_buffer[2])<<8) + ((u16)atari_sector_buffer[3]) ) )
					 * (((u32)atari_sector_buffer[4])+1)
					 * ( (u32) ( (((u16)atari_sector_buffer[6])<<8) + ((u16)atari_sector_buffer[7]) ) );
				if ( !(FileInfo.vDisk->flags & FLAGS_XFDTYPE) ) s+=16; //16bytes ATR header
				if ( FileInfo.vDisk->flags & FLAGS_ATRDOUBLESECTORS ) s-=384;	//3 single sectors at begin of DD
				if (s!=FileInfo.vDisk->size)
				{
					FileInfo.percomstate=3;	//percom write bad
					//goto Send_ERR_and_Delay;
					goto Send_CMPL_and_Delay; //Percom write is ok but values do not match
				}
			}

			FileInfo.percomstate=1;		//ok
			if ( (atari_sector_buffer[6]|atari_sector_buffer[7])==0x01) FileInfo.percomstate++; //=2 ok (double)

			goto Send_CMPL_and_Delay;
			break;


		case 0x50:	//write
		case 0x57:	//write (verify)
		case 0x52:	//read
			{
			unsigned short proceeded_bytes;
			unsigned short atari_sector_size;
			u32 n_data_offset;
			u16 n_sector;
			u16 file_sectors;

			FileInfo.percomstate=0;

			//n_sector = TWOBYTESTOWORD(cmd_buf+2);	//2,3
			n_sector = cmd_buf.aux;	//2,3

			if(n_sector==0)
				goto Send_ERR_and_Delay;

			if( !(FileInfo.vDisk->flags & FLAGS_XEXLOADER) )
			{
                if(FileInfo.vDisk->flags & FLAGS_ATXTYPE)
                {
                    loadAtxSector(n_sector);
                }
                else
                {
                    //ATR or XFD
                    if(n_sector<4)
                    {
                        //sector 1 to 3
                        atari_sector_size = (unsigned short)0x80;	//128
                        //Optimization: n_sector = 1 to 3 and sector size is fixed 128!
                        //Old way: n_data_offset = (u32) ( ((u32)(((u32)n_sector)-1) ) * ((u32)atari_sector_size));
                        n_data_offset = (u32) ( (n_sector-1) << 7 ); //*128;
                    }
                    else
                    {
                        //sector 4 or greater
                        atari_sector_size = (FileInfo.vDisk->flags & FLAGS_ATRDOUBLESECTORS)? ((unsigned short)0x100):((unsigned short)0x80);	//FileInfo.vDisk->atr_sector_size;
                        n_data_offset = (u32) ( ((u32)(((u32)n_sector)-4) ) * ((u32)atari_sector_size)) + ((u32)384);
                    }

                    //ATR or XFD?
                    if (! (FileInfo.vDisk->flags & FLAGS_XFDTYPE) ) n_data_offset+= (u32)16; //ATR header;

                    if(cmd_buf.cmd==0x52)
                    {
                        //read
                        proceeded_bytes = faccess_offset(FILE_ACCESS_READ,n_data_offset,atari_sector_size);
                        if(proceeded_bytes==0)
                        {
                            goto Send_ERR_and_Delay;
                        }
                    }
                    else
                    {
                        //write do image
                        if (USART_Get_atari_sector_buffer_and_check_and_send_ACK_or_NACK(atari_sector_size))
                        {
                            break;
                        }

                        //if ( get_readonly() )
                        //	 goto Send_ERR_and_Delay; //READ ONLY

                        proceeded_bytes = faccess_offset(FILE_ACCESS_WRITE,n_data_offset,atari_sector_size);
                        if(proceeded_bytes==0)
                        {
                            goto Send_ERR_and_Delay;
                        }

                        goto Send_CMPL_and_Delay;

                    }
                    //atari_sector_size= (bud 128 nebo 256 bytu)
                }
			}
			else
			{
				//XEX
#define	XEX_SECTOR_SIZE	128

				if (cmd_buf.cmd!=0x52)
				{
					//Trying to write to XEX
					//retrieves data
					if (USART_Get_atari_sector_buffer_and_check_and_send_ACK_or_NACK(XEX_SECTOR_SIZE))
					{
						//He did not vote for checksum (sent NACK)
						break;
					}
					//And return error
					//because we can not write to XEX (beee, beee ... ;-)))
					goto Send_ERR_and_Delay;
				}

				//clear buffer
				Clear_atari_sector_buffer(XEX_SECTOR_SIZE);

				if(n_sector<=2)		//n_sector>0 && //==0 Is verified at the beginning
				{
					//xex bootloader sectors, 1 or 2
					u08 i,b;
					u08 *spt, *dpt;
					spt= &boot_xex_loader[(u16)(n_sector-1)*((u16)XEX_SECTOR_SIZE)];
					dpt= atari_sector_buffer;
					i=XEX_SECTOR_SIZE;
					do 
					{
					 b=eeprom_read_byte(spt++);
					 //Relocation of bootloader from $0700 to another location
					 if (b==0x07) b+=bootloader_relocation;
					 *dpt++=b;
					 i--;
					} while(i);
				}

				//file_sectors is used for $168 to $169 (optimization)
				//Alignment up, ie = (size + 124) / 125
				file_sectors = ((FileInfo.vDisk->size+(u32)(XEX_SECTOR_SIZE-3-1))/((u32)XEX_SECTOR_SIZE-3));

				if(n_sector==0x168)
				{
					//Returns the diskette sector number
					//byte 1,2
					goto set_number_of_sectors_to_buffer_1_2;
				}
				else
				if(n_sector==0x169)
				{
					//fatGetDirEntry(FileInfo.vDisk->file_index,5,0);
					fatGetDirEntry(FileInfo.vDisk->file_index,0); //But it has to move 5 bytes to the right
		
					{
						u08 i,j;
						for(i=j=0;i<8+3;i++)
						{
							if( ((atari_sector_buffer[i]>='A' && atari_sector_buffer[i]<='Z') ||
								(atari_sector_buffer[i]>='0' && atari_sector_buffer[i]<='9')) )
							{
							  //The character is applicable to Atari
							  atari_sector_buffer[j]=atari_sector_buffer[i];
							  j++;
							}
							if ( (i==7) || (i==8+2) )
							{
								for(;j<=i;j++) atari_sector_buffer[j]=' ';
							}
						}
						//Moves the name from 0-10 to 5-15 (0-4 will be the system directory data)
						//You need a snap
						for(i=15;i>=5;i--) atari_sector_buffer[i]=atari_sector_buffer[i-5];
						//And then clean up the rest of the sector
						for(i=5+8+3;i<XEX_SECTOR_SIZE;i++)
							atari_sector_buffer[i]=0x00;
					}
					//Only now can add the first 5 bytes at the beginning of the directory (before the name)
					//atari_sector_buffer[0]=0x42;
					//If the file enters sector number 1024 and the file status is $46 instead of $42
					
					atari_sector_buffer[0]=(file_sectors>(0x400-0x171))? 0x46 : 0x42; //0

					TWOBYTESTOWORD(atari_sector_buffer+3)=0x0171;			//3,4
set_number_of_sectors_to_buffer_1_2:
					TWOBYTESTOWORD(atari_sector_buffer+1)=file_sectors;		//1,2
				}
				else
				if(n_sector>=0x171)
				{
					proceeded_bytes = faccess_offset(FILE_ACCESS_READ,((u32)n_sector-0x171)*((u32)XEX_SECTOR_SIZE-3),XEX_SECTOR_SIZE-3);

					if(proceeded_bytes<(XEX_SECTOR_SIZE-3))
						n_sector=0; //This is the last sector
					else
						n_sector++; //Pointer to the next

					atari_sector_buffer[XEX_SECTOR_SIZE-3]=((n_sector)>>8); //First HB !!!
					atari_sector_buffer[XEX_SECTOR_SIZE-2]=((n_sector)&0xff); //then DB!!! (it is HB,DB)
					atari_sector_buffer[XEX_SECTOR_SIZE-1]=proceeded_bytes;
				}

				//sector size is always 128 bytes
				atari_sector_size=XEX_SECTOR_SIZE;
			}

			//Send either 128 or 256 (atr / xfd) or 128 (xex)
			USART_Send_cmpl_and_atari_sector_buffer_and_check_sum(atari_sector_size);

			}
			break;

		case 0x53:	//get status

			FileInfo.percomstate=0;

			atari_sector_buffer[0] = 0x10;	//0x00 motor off	0x10 motor on
			//(FileInfo.vDisk->atr_medium_size);	// medium/single
			if (FileInfo.vDisk->flags & FLAGS_ATRMEDIUMSIZE) atari_sector_buffer[0]|=0x80;
			//((FileInfo.vDisk->atr_sector_size==256)?0x20:0x00); //	double/normal sector size
			if (FileInfo.vDisk->flags & FLAGS_ATRDOUBLESECTORS) atari_sector_buffer[0]|=0x20;
			if (FileInfo.vDisk->flags & FLAGS_WRITEERROR)
			{
			 atari_sector_buffer[0]|=0x04;			//Write error status bit
			 //vynuluje FLAGS_WRITEERROR bit
			 FileInfo.vDisk->flags &= (~FLAGS_WRITEERROR);
			}
			//if (get_readonly()) atari_sector_buffer[0]|=0x08;	//write protected bit

			atari_sector_buffer[1] = 0xff;
			atari_sector_buffer[2] = 0xe0; 		//(244s) timeout pro nejdelsi operaci
			atari_sector_buffer[3] = 0x00;		//timeout hb

			USART_Send_cmpl_and_atari_sector_buffer_and_check_sum(4);

			break;
		} //switch
	} // end diskcommands


/////////////////////////////// sdrive commands

	else
	//if( cmd_buf.dev == (unsigned char)0x71+3-((unsigned char)(inb(PINB))&0x03) )
	if( cmd_buf.dev == 0x71 )
	{
		//if ( cmd_buf.cmd==0x50 || cmd_buf.cmd==0x52 || cmd_buf.cmd==0x53)
		//schvalne prehozeno poradi kvuli rychlosti (pri normalnich operacich s SDrive nebude platit hned prvni cast podminky)
		if ( cmd_buf.cmd<=0x53 && cmd_buf.cmd>=0x50 )
		{
			//(0x50,x52,0x53)
			//pro povely readsector,writesector a status
			//pristup na D0: pres SDrive zarizeni 0x71-0x74
			cmd_buf.dev=0x30;	//zmeni pozadovane zarizeni na D0:
			goto disk_operations_direct_d0_d4; //vzdy napevno vD0: bez prehazovani
		}


		send_ACK();
//			Delay1000us();	//delay_us(COMMAND_DELAY);

		//set Ptr to temp vDisk buffer, except for Get vDisk flags
		if ( cmd_buf.cmd != 0xDB )
			FileInfo.vDisk = &tmpvDisk;

		switch(cmd_buf.cmd)
		{
/*
		case 0xXX:
			{
			 unsigned char* p = 0x060;
			 //check_sum = get_checksum(p,1024);	//nema smysl - checksum cele pameti by se ukladal zase do pameti a tim by to zneplatnil
			 Delay800us();	//t5
			 send_CMPL();
			 Delay800us();	//t6

			 USART_Send_Buffer(p,1024);
			 //USART_Transmit_Byte(check_sum);
			}
			break;

		case 0xXX:	//STACK POINTER+DEBUG_ENDOFVARIABLES
			{
			 Clear_atari_sector_buffer_256();
			 atari_sector_buffer[0]= SPL;	//inb(0x3d);
			 atari_sector_buffer[1]= SPH;	//inb(0x3e);
			 *((u32*)&(atari_sector_buffer[2]))=debug_endofvariables;

			 //check_sum = get_checksum(atari_sector_buffer,256);
			 //send_CMPL();
			 //Delay1000us();	//delay_us(COMMAND_DELAY);
			 //USART_Send_Buffer(atari_sector_buffer,256);
			 //USART_Transmit_Byte(check_sum);
			 USART_Send_cmpl_and_atari_sector_buffer_and_check_sum(6);
			}
			break;

		case 0xXX:	//$XX nn mm	 config EEPROM par mm = value nn
			{
				eeprom_write_byte((&system_atr_name)+cmd_buf.aux2,cmd_buf.aux1);
				goto Send_CMPL_and_Delay;
			}
			break;
*/

		//--------------------------------------------------------------------------

		case 0xC0:	//$C0 xl xh	Get 20 filenames from xhxl. 8.3 + attribute (11+1)*20+1 [<241]
					//		+1st byte of filename 21 (+1)
			{
				u08 i,j;
				u08 *spt,*dpt;
				u16 fidx;
				Clear_atari_sector_buffer_256();
				//fidx=TWOBYTESTOWORD(command+2); //cmd_buf.aux1,[3]
				fidx=cmd_buf.aux;
				dpt=atari_sector_buffer+12;
				i=21;
				while(1)
				{	
					i--;
					if (!fatGetDirEntry(fidx++,0)) break;
					//8+3+ukonceni0
					spt=atari_sector_buffer;
					if (!i)
					{
						*dpt++=*spt; //prvni znak jmena 21 souboru
						break;
					}
					j=11; //8+3znaku=11
					do { *dpt++=*spt++; j--; } while(j);
					*dpt++=FileInfo.Attr; //na pozici[11] da atribut
				}// while(i);
				//a ted posunout 241 bajtu o 12 dolu [12-252] => [0-240]
				spt=atari_sector_buffer+12;
				dpt=atari_sector_buffer;
				i=241;
				do { *dpt++=*spt++; i--; } while(i);
				USART_Send_cmpl_and_atari_sector_buffer_and_check_sum(241); //241! bajtu
			}
			break;


		case 0xC1:	//$C1 nn ??	set fastsio pokey divisor
			
			if (cmd_buf.aux1>US_POKEY_DIV_MAX) goto Send_ERR_and_Delay;
			fastsio_pokeydiv = cmd_buf.aux1;
			fastsio_active=0;	//zmenila se rychlost, musi prejit na standardni
			
			/*
			//takhle to zlobilo
			//vzdy to zabrucelo a vratio error #$8a (i kdyz se to provedlo)
			USART_Init(ATARI_SPEED_STANDARD); //a zinicializovat standardni
			goto Send_CMPL_and_Delay;
			*/

			Delay800us();	//t5
			send_CMPL();
			goto change_sio_speed_by_fastsio_active;
			//USART_Init(ATARI_SPEED_STANDARD); //a zinicializovat standardni
			//break;


		case 0xC2:	//$C2 nn ??	set bootloader relocation = $0700+$nn00

			bootloader_relocation = cmd_buf.aux1;
			goto Send_CMPL_and_Delay;

		//--------------------------------------------------------------------------

		case 0xDA:	//get system values
			{
				u08 *sptr,*dptr;
				u08 i;
				dptr=atari_sector_buffer;
				//
				//Globalni sada promennych
				sptr=(u08*)&GS;
				i=sizeof(struct GlobalSystemValues);	//13
				do { *dptr++=*sptr++; i--; } while(i>0);
				//tzv. lokalni vDisk
				//sptr=(u08*)&FileInfo.vDisk;
				sptr=(u08*)FileInfo.vDisk;
				i=sizeof(virtual_disk_t);				//15
				do { *dptr++=*sptr++; i--; } while(i>0);

				//celkem 26 bytu
				USART_Send_cmpl_and_atari_sector_buffer_and_check_sum( (sizeof(struct GlobalSystemValues)+sizeof(virtual_disk_t)) );
			}				
			break;

		case 0xDB:	//get vDisk flag [<1]
			{
				//atari_sector_buffer[0] = (cmd_buf.aux1<DEVICESNUM)? vDisk[cmd_buf.aux1].flags : FileInfo.vDisk->flags;
				atari_sector_buffer[0] = FileInfo.vDisk->flags;
				USART_Send_cmpl_and_atari_sector_buffer_and_check_sum(1);
			}
			break;

		case 0xDC:	//$DC xl xh	fatClusterToSector xl xh [<4]
			{	//brocken for FAT32!!!
				//FOURBYTESTOLONG(atari_sector_buffer) = fatClustToSect(TWOBYTESTOWORD(command+2));
				//strict aliasing
				//*asb32_p = fatClustToSect(TWOBYTESTOWORD(command+2));
				*asb32_p = fatClustToSect(cmd_buf.aux);
				USART_Send_cmpl_and_atari_sector_buffer_and_check_sum(4);
			}
			break;

		case 0xDD:	//set SDsector number

			if (USART_Get_atari_sector_buffer_and_check_and_send_ACK_or_NACK(4))
			{
				break;
			}

			//extraSDcommands_readwritesectornumber = *((u32*)&atari_sector_buffer);
			extraSDcommands_readwritesectornumber = *asb32_p;

			//uz si to hned ted nacte do cache kvuli operaci write SDsector
			//ve ktere Atarko zacne posilat data brzo po ACKu a nemuselo by se to stihat
			mmcReadCached(extraSDcommands_readwritesectornumber);

			goto Send_CMPL_and_Delay;
			break;

		case 0xDE:	//read SDsector

			mmcReadCached(extraSDcommands_readwritesectornumber);

			Delay800us();	//t5
			send_CMPL();
			Delay800us();	//t6
			{
			 u08 check_sum;
			 check_sum = get_checksum(mmc_sector_buffer,512);
			 USART_Send_Buffer(mmc_sector_buffer,512);
			 USART_Transmit_Byte(check_sum);
			}
			//USART_Send_cmpl_and_atari_sector_buffer_and_check_sum(atari_sector_size); //nelze pouzit protoze checksum je 513. byte (prelezl by ven z bufferu)
			break;

		case 0xDF:	//write SDsector
			
			//Specificky problem:
			//Atarko zacina predavat data za t3(min)=1000us,
			//ale tak rychle nestihneme nacist (Flushnout) mmcReadCached
			//RESENI => mmcReadCached(extraSDcommands_readwritesectornumber);
			//          se vola uz pri nastavovani cisla SD sektoru, takze uz je to
			//          nachystane. (tedy pokud si nezneplatnil cache jinymi operacemi!)
		
			//nacte ho do mmc_sector_bufferu
			//tim se i Flushne pripadny odlozeny write
			//a nastavi se n_actual_mmc_sector
			mmcReadCached(extraSDcommands_readwritesectornumber);

			//prepise mmc_buffer daty z Atarka
			{
			 u08 err;
			 err=USART_Get_Buffer_And_Check(mmc_sector_buffer,512,CMD_STATE_H);
			 Delay1000us();
			 if(err)
			 {
				//obnovi puvodni stav mc_sector_bufferu z SDkarty!!!
				mmcReadCached(extraSDcommands_readwritesectornumber);
				send_NACK();
				break;
			 }
			}

			send_ACK();

			//zapise zmenena data v mmc_sector_bufferu do prislusneho SD sektoru
			if (mmcWriteCached(0)) //klidne s odlozenym zapisem
			{
				goto Send_ERR_and_Delay; //nepovedlo se (zakazany zapis)
			}

			goto Send_CMPL_and_Delay;
			break;

		//--------------------------------------------------------------------------

		case 0xE0: //get status
			//$E0  n ??	Get status. n=bytes of infostring "SDriveVV YYYYMMDD..."

			if (!cmd_buf.aux1) goto Send_CMPL_and_Delay;

			{
			 u08 i;
			 //for(i=0; i<cmd_buf.aux1; i++) atari_sector_buffer[i]=eeprom_read_byte(&system_info[i]);
			 for(i=0; i<cmd_buf.aux1; i++) atari_sector_buffer[i]=pgm_read_byte(&system_info[i]);
			}

			USART_Send_cmpl_and_atari_sector_buffer_and_check_sum(cmd_buf.aux1);

			break;


		case 0xE1: //init drive
			//$E1  n ??	Init drive. n=0 (komplet , jako sdcardejected), n<>0 (jen setbootdrive d0: bez zmeny actual drive)

			//protoze muze volat prvni cast s mmcReset,
			//musi ulozit pripadny nacacheovany sektor
			mmcWriteCachedFlush(); //pokud ceka nejaky sektor na zapis, zapise ho

			Delay800us();	//t5
			send_CMPL();
			
			//if ( cmd_buf.aux1==0 ) goto SD_CARD_EJECTED;
			if ( cmd_buf.aux1==0 ) goto *0x0000;
			//goto SET_SDRIVEATR_TO_D0; //bez zmeny actual_drive XXX BROCKEN!!!
			break;


		case 0xE2: //$E2  n ??	Deactivation of device Dn:
			if ( cmd_buf.aux1 >= DEVICESNUM ) goto Send_ERR_and_Delay;
			vDisk[cmd_buf.aux1].flags &= ~FLAGS_DRIVEON; //vDisk[cmd_buf.aux1].flags &= ~FLAGS_DRIVEON;
			//check for new flag and delete it
			if (FileInfo.vDisk->flags & FLAGS_ATRNEW)
				FileInfo.vDisk->flags &= ~FLAGS_ATRNEW;
			strncpy_P(&tft.pages[0].buttons[cmd_buf.aux1].name[3], PSTR("<empty>     "), 12);
			set_display(actual_drive_number);
			goto Send_CMPL_and_Delay;
			break;

		case 0xE3: //$E3  n ??	Set actual directory by device Dn: and get filename. 8.3 + attribute + fileindex [11+1+2=14]
		   {
			if ( cmd_buf.aux1 >= DEVICESNUM) goto Send_ERR_and_Delay;
			//if (!(vDisk[cmd_buf.aux1].flags & FLAGS_DRIVEON))
			if (!(vDisk[cmd_buf.aux1].flags & FLAGS_DRIVEON))
			{
				//v teto jednotce neni nic, takze vrati 14 bytu prazdny buffer
				//a NEBUDE menit adresar!

Command_FD_E3_empty:	//sem skoci z commandu FD kdyz chce vratit
					//prazdnych 14 bytu jakoze neuspesny vysledek misto nalezeneho
					//fname 8+3+atribut+fileindex, tj. jako command E3

Command_ED_notfound:	//sem skoci z commandu ED kdyz nenajde hledany nazev souboru
					//vraci 14 prazdnych bytu jakoze neuspesny vysledek

				Clear_atari_sector_buffer(14);

				USART_Send_cmpl_and_atari_sector_buffer_and_check_sum(14);
				break;
			}
		   }

			//set pointer to drive
			FileInfo.vDisk = &vDisk[cmd_buf.aux1];
			//copy to tmp vDisk, otherwise Get Path...etc. wouldn't work!
			tmpvDisk.dir_cluster = FileInfo.vDisk->dir_cluster;
			//set parameters from drive for GET DETAILED FILENAME
			cmd_buf.aux1=(unsigned char)(FileInfo.vDisk->file_index & 0xff);
			cmd_buf.aux2=(unsigned char)(FileInfo.vDisk->file_index >> 8);

			// a pokracuje dal!!!

		case 0xE4: 	//$E4 xl xh	Get filename xhxl. 8.3 + attribute [11+1=12]
		case 0xE5:	//$E5 xl xh	Get more detailed filename xhxl. 8.3 + attribute + size/date/time [11+1+8=20]
		   {
			unsigned char ret,len;
			//ret=fatGetDirEntry((cmd_buf.aux1+(cmd_buf.aux2<<8)),0);
			//ret=fatGetDirEntry(TWOBYTESTOWORD(command+2),0);
			ret=fatGetDirEntry(cmd_buf.aux,0);

			if (0)
			{
Command_FD_E3_ok:	//sem skoci z commandu FD kdyz chce vratit
				//fname 8+3+atribut+fileindex, tj. stejne jako vraci command E3
Command_ED_found:	//sem skoci z commandu ED kdyz najde hledane filename a chce vratit
				//fname 8+3+atribut+fileindex, tj. stejne jako vraci command E3
				cmd_buf.cmd=0xE3;
				ret=1;
			}

			atari_sector_buffer[11]=(unsigned char)FileInfo.Attr;
			if (cmd_buf.cmd == 0xE3)
			{
				//command 0xE3
				/*
				atari_sector_buffer[12]=(unsigned char)(FileInfo.vDisk->file_index&0xff);
				atari_sector_buffer[13]=(unsigned char)(FileInfo.vDisk->file_index>>8);
				*/
				TWOBYTESTOWORD(atari_sector_buffer+12)=FileInfo.vDisk->file_index; //12,13
				len=14;
			}
			else
			if (cmd_buf.cmd == 0xE5)
			{
				unsigned char i;
				//command 0xE5
				/*
				atari_sector_buffer[12]=(unsigned char)(FileInfo.vDisk->Size&0xff);
				atari_sector_buffer[13]=(unsigned char)(FileInfo.vDisk->Size>>8);
				atari_sector_buffer[14]=(unsigned char)(FileInfo.vDisk->Size>>16);
				atari_sector_buffer[15]=(unsigned char)(FileInfo.vDisk->Size>>24);
				*/
				FOURBYTESTOLONG(atari_sector_buffer+12)=FileInfo.vDisk->size; //12,13,14,15
				//atari_sector_buffer[16]=(unsigned char)(FileInfo.Date&0xff);
				//atari_sector_buffer[17]=(unsigned char)(FileInfo.Date>>8);
				TWOBYTESTOWORD(atari_sector_buffer+16)=FileInfo.Date; //16,17
				//atari_sector_buffer[18]=(unsigned char)(FileInfo.Time&0xff);
				//atari_sector_buffer[19]=(unsigned char)(FileInfo.Time>>8);
				TWOBYTESTOWORD(atari_sector_buffer+18)=FileInfo.Time; //18,19

				//1234567890 YYYY-MM-DD HH:MM:SS
				//20-49
				//20-29 string decimal size
				for(i=20; i<=49; i++) atari_sector_buffer[i]=' ';
				{
				 unsigned long size=FileInfo.vDisk->size;
				 if ( !(FileInfo.Attr & (ATTR_DIRECTORY|ATTR_VOLUME)) )
				 {
					//pokud to neni directory nebo nazev disku (volume to ani nemuze byt - getFatDirEntry VOLUME vynechava)
					//zapise na posledni pozici "0" (jakoze velikost souboru = 0 bytes)
					//(bud ji nasledne prepise skutecnou velikosti, nebo tam zustane)
					atari_sector_buffer[29]='0';
				 }
				 for(i=29; (i>=20); i--)
				 {
				  if (size) //>0
				  {
				   atari_sector_buffer[i]= 0x30 | ( size % 10 );	//cislice '0'-'9'
				   size/=10;
				  }
				 }
			    }
//					atari_sector_buffer[32]=' '; //je promazane ve smycce predtim
//					atari_sector_buffer[43]=' '; //je promazane ve smycce predtim
				//33-42 string date YYYY-MM-DD
				atari_sector_buffer[35]='-';
				atari_sector_buffer[38]='-';
				//44-51 string time HH:MM:SS
				atari_sector_buffer[44]=':';
				atari_sector_buffer[47]=':';
				{
				 unsigned short d;
				 //date
				 d=(FileInfo.Date>>9)+1980;
				 PutDecimalNumberToAtariSectorBuffer(d/100,31);
				 PutDecimalNumberToAtariSectorBuffer(d%100,33);
				 //
				 PutDecimalNumberToAtariSectorBuffer((FileInfo.Date>>5)&0x0f,36);
				 //atari_sector_buffer[36]=0x30|(d/10);
				 //atari_sector_buffer[37]=0x30|(d%10);
				 PutDecimalNumberToAtariSectorBuffer((FileInfo.Date)&0x1f,39);
				 //atari_sector_buffer[39]=0x30|(d/10);
				 //atari_sector_buffer[40]=0x30|(d%10);
				 //time
				 PutDecimalNumberToAtariSectorBuffer((FileInfo.Time>>11),42);
				 //atari_sector_buffer[42]=0x30|(d/10);
				 //atari_sector_buffer[43]=0x30|(d%10);
				 PutDecimalNumberToAtariSectorBuffer((FileInfo.Time>>5)&0x3f,45);
				 //atari_sector_buffer[45]=0x30|(d/10);
				 //atari_sector_buffer[46]=0x30|(d%10);
				 PutDecimalNumberToAtariSectorBuffer(((FileInfo.Time)&0x1f)<<1,48);	//doubleseconds
				 //atari_sector_buffer[48]=0x30|(d/10);
				 //atari_sector_buffer[49]=0x30|(d%10);
				}
				len=50;
			}
			else
				len=12;	//command 0xE4

			//pokud fatGetDirEntry neproslo, promaze buffer
			//(delka len musi zustat, aby posilal prazdny buffer prislusne delky)
			if (!ret)
			{
			 //50 je maximalni delka ze 12,14 a 50
			 //unsigned char i;
			 //for(i=0; i<50; i++) atari_sector_buffer[i]=0x00;	
			 Clear_atari_sector_buffer(50);
			}

			USART_Send_cmpl_and_atari_sector_buffer_and_check_sum(len);
		   }
		   break;

		case 0xE6:	//$E6 xl xh	Get longfilename xhxl. [256]
		case 0xE7:
		case 0xE8:
		case 0xE9:
		   {
			unsigned short i;

			i=0;
			//if (fatGetDirEntry(TWOBYTESTOWORD(command+2),1))
			if (fatGetDirEntry(cmd_buf.aux,1))
			{
			 //nasel, takze se posune i-ckem az na konec longname
			 while(atari_sector_buffer[i++]!=0);	//!!!!!!!!!!!!!!!!! pokud by longname melo 256 znaku, zasekne se to tady!!! Bacha!!!
			}
			while(i<256) atari_sector_buffer[i++]=0x00; //smaze zbytek

			//jakou delku chce z Atarka prebirat?
			i=(0x0100>>(cmd_buf.cmd-0xE6)); //=256,128,64,32

			USART_Send_cmpl_and_atari_sector_buffer_and_check_sum(i);
		   }
		   break;

		case 0xEA:	//$EA ?? ??	Get number of items in actual directory [<2]
		   {
			unsigned short i;
			i=0;
			while (fatGetDirEntry(i,0)) i++;
			//TWOBYTESTOWORD(atari_sector_buffer+0)=i;	//0,1
			TWOBYTESTOWORD((u16*)asb32_p)=i;	//0,1
			USART_Send_cmpl_and_atari_sector_buffer_and_check_sum(2);
		   }
		   break;

		case 0xEB:	//$EB  n ??	GetPath (n=maximum updirs, if(n==0||n>20) n=20 )   [<n*12]  pro n==0 [<240]
				//Get path (/DIRECTOREXT/../DIRECTOREXT followed by 0x00)
				//(does not change the current directory!)
				//if path is more then max updirs, first char is '?'
		   {
		     unsigned long ocluster, acluster;
			 unsigned char pdi;
			 u08 updirs;
			 if (cmd_buf.aux1==0 || cmd_buf.aux1>20) cmd_buf.aux1=20; //maximum [<240]
			 updirs=cmd_buf.aux1; //max updirs
			 pdi=0; //index to buffered writes
			 ocluster=FileInfo.vDisk->dir_cluster; //save old directory cluster
			 while ( fatGetDirEntry(0,0) //first entry is ".." ("." will be skipped)
				  && (FileInfo.Attr & ATTR_DIRECTORY)
				  && atari_sector_buffer[0]=='.' && atari_sector_buffer[1]=='.' )
			 {
				if (!updirs)	//max updirs reached?
				{
					//mark it and stop this action
					atari_sector_buffer[pdi]='?';
					break;
				}
				updirs--;
				//save cluster of current directory
				acluster=FileInfo.vDisk->dir_cluster;
				//set current directory to next parent directory we have found now
				FileInfo.vDisk->dir_cluster=FileInfo.vDisk->start_cluster;
				//find the name of last current directory by it's cluster number
				// and save it to the end of the buffer
				unsigned short i;
				for(i=0; ; i++)
				{
					if (!fatGetDirEntry(i,0)) {
						//blink(i); //TODO
						break;
					}
					if (acluster==FileInfo.vDisk->start_cluster)
					{
						//found, save it
						u08 *spt,*dpt;
						u08 j;
						pdi-=12;
						spt=atari_sector_buffer;
						dpt=atari_sector_buffer+pdi;
						//copy the dirname to the end
						*dpt++='/';	//start with '/'
						j=11; do { *dpt++=*spt++; j--; } while(j);
						break;
					}
				}
			  }
			  //recover old directory
			  FileInfo.vDisk->dir_cluster=ocluster;
			  //copy path to start of buffer and send it
			  {
				  unsigned short k;
				  unsigned char m;
				  for(k=pdi,m=0; k<256; k++,m++)
					atari_sector_buffer[m]=atari_sector_buffer[k];
				  if (!m) { atari_sector_buffer[0]=0; m++; }
				  for(; m!=0; m++)
					atari_sector_buffer[m]=0; //end and clear rest of buffer
			  }
			  USART_Send_cmpl_and_atari_sector_buffer_and_check_sum(cmd_buf.aux1*12);
		    }
			break;

		case 0xEC: //$EC  n  m	Find filename 8+3.
			   //(Predava se vzor 11 bytu, nulove znaky jsou ignorovany).
				   // If (m<>0) set fileOrdirectory to vDn:

			//FileFindBuffer <> atari_sector_buffer !!!
			if (USART_Get_buffer_and_check_and_send_ACK_or_NACK(FileFindBuffer,11))
			{
				break;
			}

			if (!cmd_buf.aux2) goto Send_CMPL_and_Delay;

			//zmeni command na 0xf0-0xff (pro nastaveni nalezeneho souboru/adresare)				
			cmd_buf.cmd=(0xf0|cmd_buf.aux1);
			cmd_buf.aux1=cmd_buf.aux2=0; //vyhledavat od indexu 0
			//a pokracuje dal...
			//commandem ED...

		case 0xED: //$ED xl xh       Find filename (8+3) from xhxl index in actual directory, get filename 8.3+attribute+fileindex [11+1+2=14]

		    {
				unsigned short i;
				u08 j;

				//i=TWOBYTESTOWORD(command+2);  //zacne hledat od tohoto indexu
				i=cmd_buf.aux;  //zacne hledat od tohoto indexu

				//if we come from cmd 0xEC, set vDisk to drive
				if (cmd_buf.cmd>=0xf0 && (cmd_buf.cmd&0xf) < DEVICESNUM) {
					FileInfo.vDisk = &vDisk[cmd_buf.cmd&0xf];
					//and copy dir_cluster into
					FileInfo.vDisk->dir_cluster=tmpvDisk.dir_cluster;
				}

				while(fatGetDirEntry(i,0))
				{
				 for(j=0; (j<11); j++)
				 {
					if (!FileFindBuffer[j]) continue; //znaky 0x00 ve vzoru ignoruje (mohou byt jakekoliv)
					if (FileFindBuffer[j]!=atari_sector_buffer[j]) goto Different_char; //rozdilny znak
				 }
				 //nalezeny nazev vyhovuje vzoru
				 if (cmd_buf.cmd>=0xf0)
				 {
					//if we come from cmd 0xEC
					if ((cmd_buf.cmd&0xf) < DEVICESNUM) {
						//and cmd is set entry to drive,
						//set pointer to vDisk
						FileInfo.vDisk = &vDisk[cmd_buf.cmd&0xf];
						//and copy dir_cluster into
						FileInfo.vDisk->dir_cluster=tmpvDisk.dir_cluster;
						//and let reset the values
						fatGetDirEntry(i,0);
					}
					cmd_buf.aux = i;	//set found direntry to aux
					//proceed with Set Direntry...
					goto Command_EC_F0_FF_found;
				 }
				 goto Command_ED_found;
	Different_char:
				 i++;
				}

				//nenalezl zadny nazev vyhovujici vzoru

				if (cmd_buf.cmd>=0xf0)
				{
					//prisel sem z commandu EC (ktery ho nastavil na $f0-$ff)
					goto Send_ERR_and_Delay;
				}

				goto Command_ED_notfound;
			}
			break;

		case 0xEE:	//$EE  n ??	Swap vDn: (n=0..4) with D{SDrivenum}:  (if n>=5) Set default (all devices with relevant numbers).

			//Prepnuti prislusneho drive do Dsdrivenum:
			//(if >=5) SetDefault
			//actual_drive_number = ( cmd_buf.aux1 < DEVICESNUM )? cmd_buf.aux1 : 4-((unsigned char)(inb(PINB))&0x03);
			actual_drive_number = ( cmd_buf.aux1 < DEVICESNUM )? cmd_buf.aux1 : 4-3;
			set_display(actual_drive_number);
			goto Send_CMPL_and_Delay;
			break;

		case 0xEF:	//$EF  n  m	Get SDrive parameters (n=len, m=from) [<n]
			{
			 u08 *spt,*dpt;
			 u08 n;
			 n=cmd_buf.aux1;
			 spt=((u08*)&sdrparams)+cmd_buf.aux2;	//m
			 dpt=atari_sector_buffer;
			 do { *dpt++=*spt++; n--; } while(n);
			 USART_Send_cmpl_and_atari_sector_buffer_and_check_sum(cmd_buf.aux1);
			}
			break;

		case 0xFD:	//$FD  n ??	Change actual directory up (..). If n<>0 => get dirname. 8+3+attribute+fileindex [<14]
			 if (fatGetDirEntry(0,0))	//0.polozka by mela byt ".." ("." se vynechava)		//fatGetDirEntry(1,0)
			 {
				if ( (FileInfo.Attr & ATTR_DIRECTORY)
					&& atari_sector_buffer[0]=='.' && atari_sector_buffer[1]=='.')
				{
					//Schova si cluster zacatku nynejsiho adresare
					unsigned long acluster;
					acluster=FileInfo.vDisk->dir_cluster;
					//Meni adresar
					//if (FileInfo.vDisk->start_cluster == MSDOSFSROOT)
					if (FileInfo.vDisk->start_cluster == RootDirCluster)
						FileInfo.vDisk->dir_cluster=RootDirCluster;
					else
						FileInfo.vDisk->dir_cluster=FileInfo.vDisk->start_cluster;
					//aux1 ?
					if (cmd_buf.aux1)
					{
					 //chce vratit 8+3+atribut+fileindex
					 unsigned short i;
					 for(i=0; fatGetDirEntry(i,0); i++)
					 {
						if (acluster==FileInfo.vDisk->start_cluster)
						{
						 //skoci do casti predavani jmena 8+3+atribut+fileindex
						 goto Command_FD_E3_ok; //nasel ho!
						}
					 }
					 //nenasel ho vubec
					 //tohle nemusime resit, nebot to nikdy nemuze nastat!
					 //(znamenalo by to nekonzistentni FATku)
					 //tj. cluster z ".." by ukazoval na adresar ve kterem by nebyl adresar
					 //s clusterem ukazujicim na tento podadresar.
					}
				}
			 }
			 //nulta polozka neexistuje nebo neni ".."
			 //coz znamena ze jsme uz v rootu
			 if (cmd_buf.aux1)
			 {
				  //chce vratit filename 8+3+atribut+fileindex
				  //takze musi dostat 14 prazdnych bajtu
				  //u08 i;
				  //for(i=0; i<14; i++) atari_sector_buffer[i]=0;
				  //USART_Send_cmpl_and_atari_sector_buffer_and_check_sum(14);

				  //reset to RootDir, if card has changed, old dir_cluster may be wrong
				  FileInfo.vDisk->dir_cluster=RootDirCluster;
				  goto Command_FD_E3_empty;
			 }
			 goto Send_CMPL_and_Delay;
			 break;

		case 0xFE:	//$FE ?? ??	Change actual directory to rootdir.
			//FileInfo.vDisk->dir_cluster=MSDOSFSROOT;
			FileInfo.vDisk->dir_cluster=RootDirCluster;
			fatGetDirEntry(0,0);
			goto Send_CMPL_and_Delay;
			break;

		case 0xF0:	// set direntry to vD0:
		case 0xF1:	// set direntry to vD1:
		case 0xF2:	// set direntry to vD2:
		case 0xF3:	// set direntry to vD3:
		case 0xF4:	// set direntry to vD4:
		case 0xFF:  // set actual directory
			{
			unsigned char ret;

			if ( (cmd_buf.cmd&0xf) < DEVICESNUM )
			{
				//set pointer to corresponding drive
				FileInfo.vDisk = &vDisk[cmd_buf.cmd&0xf];
				//copy dir_cluster from tmp Struct
				FileInfo.vDisk->dir_cluster=tmpvDisk.dir_cluster;
			}

			//ret=fatGetDirEntry(TWOBYTESTOWORD(command+2),0); //2,3
			ret=fatGetDirEntry(cmd_buf.aux,0); //2,3

			if(ret)
			{
Command_EC_F0_FF_found:
				if( (FileInfo.Attr & ATTR_DIRECTORY) )
				{
					//Meni adresar
					FileInfo.vDisk->dir_cluster=FileInfo.vDisk->start_cluster;
				}
				else
				{
					//Aktivuje soubor
					FileInfo.vDisk->current_cluster=FileInfo.vDisk->start_cluster;
					FileInfo.vDisk->ncluster=0;
					//reset flags except ATRNEW
					FileInfo.vDisk->flags &= FLAGS_ATRNEW;

					if( atari_sector_buffer[8]=='A' &&
						 atari_sector_buffer[9]=='T' &&
						  atari_sector_buffer[10]=='R' )
					{
						// ATR
						unsigned long compute;
						unsigned long tmp;

						faccess_offset(FILE_ACCESS_READ,0,16); //je to ATR
								
						FileInfo.vDisk->flags|=FLAGS_DRIVEON;
						//FileInfo.vDisk->atr_sector_size = (atari_sector_buffer[4]+(atari_sector_buffer[5]<<8));
						if ( (atari_sector_buffer[4]|atari_sector_buffer[5])==0x01 )
							FileInfo.vDisk->flags|=FLAGS_ATRDOUBLESECTORS;

						compute = FileInfo.vDisk->size - 16;		//je to ATR
						tmp = (FileInfo.vDisk->flags & FLAGS_ATRDOUBLESECTORS)? 0x100:0x80;		//FileInfo.vDisk->atr_sector_size;
						compute /=tmp;
						if(compute>720) FileInfo.vDisk->flags|=FLAGS_ATRMEDIUMSIZE; //atr_medium_size = 0x80;
					}
					else
					if( atari_sector_buffer[8]=='X' &&
						 atari_sector_buffer[9]=='F' &&
						  atari_sector_buffer[10]=='D' )
					{
						//XFD
						FileInfo.vDisk->flags|=(FLAGS_DRIVEON|FLAGS_XFDTYPE);
						//if ( FileInfo.vDisk->Size == 92160 ) //normal XFD
						if ( FileInfo.vDisk->size>IMSIZE1)
						{
							if ( FileInfo.vDisk->size<=IMSIZE2)
							{
								// Medium size
								FileInfo.vDisk->flags|=FLAGS_ATRMEDIUMSIZE;
							}
							else
							{
								// Double (FileInfo.vDisk->Size == 183936)
								FileInfo.vDisk->flags|=FLAGS_ATRDOUBLESECTORS;
							}
						}
					}
                    else
                    if( atari_sector_buffer[8]=='A' &&
                         atari_sector_buffer[9] =='T' &&
                          atari_sector_buffer[10] == 'X' )
                    {
                        //ATX
                        loadAtxFile(); // TODO: check return value
                        FileInfo.vDisk->flags|=(FLAGS_DRIVEON|FLAGS_ATXTYPE);
                    }
					else
					{
						// XEX
						FileInfo.vDisk->flags|=FLAGS_DRIVEON|FLAGS_XEXLOADER|FLAGS_ATRMEDIUMSIZE;
					}

					if((cmd_buf.cmd&0xf) != 0 && ((cmd_buf.cmd&0xf) < DEVICESNUM)) {
						//set new filename to button
						fatGetDirEntry(cmd_buf.aux,0);
						pretty_name((char*) atari_sector_buffer);
						strncpy(&tft.pages[0].buttons[cmd_buf.cmd&0xf].name[3], (char*)atari_sector_buffer, 12);
						//redraw display only, if we are on
						//main page
						if(actual_page == 0)
							draw_Buttons();
					}

				}

				if (FileInfo.vDisk->flags & FLAGS_ATRNEW) {	//new image created
					goto format_new;
				}

				goto Send_CMPL_and_Delay;
			}
			else
			{
				goto Send_ERR_and_Delay;
			}
			
			}
			break;

		default:
			goto Send_ERR_and_Delay;	//nerozpoznany SDrive command


		} //switch

		if (0)
		{
Send_CMPL_and_Delay:
			if (debug) {
				sio_debug('C');
			}
			else
				Delay800us();	//t5
			send_CMPL();
		}
		if (0)
		{
Send_ERR_and_Delay:
			if (debug) {
				sio_debug('E');
			}
			else
				Delay800us();	//t5
			send_ERR();
		}
		//za poslednim cmpl nebo err neni potreba pauza, jde hned cekat na command
	} //sdrive operations

}
