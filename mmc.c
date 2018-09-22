/*! \file mmc.c \brief MultiMedia and SD Flash Card Interface. */
//*****************************************************************************
//
// File Name	: 'mmc.c'
// Title		: MultiMedia and SD Flash Card Interface
// Author		: Pascal Stang - Copyright (C) 2004
// Created		: 2004.09.22
// Revised		: 2004.09.22
// Version		: 0.1
// Target MCU	: Atmel AVR Series
// Editor Tabs	: 4
//
// NOTE: This code is currently below version 1.0, and therefore is considered
// to be lacking in some functionality or documentation, or may not be fully
// tested.  Nonetheless, you can expect most functions to work.
//
// ----------------------------------------------------------------------------
// 17.8.2008
// Bob!k & Raster, C.P.U.
// Original code was modified especially for the SDrive device. 
// Some parts of code have been added, removed, rewrited or optimized due to
// lack of MCU AVR Atmega8 memory.
// ----------------------------------------------------------------------------
// 03.05.2014
// enhanced by kbr
//
// This code is distributed under the GNU Public License
//		which can be found at http://www.gnu.org/licenses/gpl.txt
//
//*****************************************************************************

//----- Include Files ---------------------------------------------------------
#include <avr/io.h>		// include I/O definitions (port names, pin names, etc)
//#include <avr/signal.h>		// include "signal" names (interrupt names)
#include <avr/interrupt.h>	// include interrupt support

#include "avrlibdefs.h"		// global AVRLIB defines
#include "avrlibtypes.h"	// global AVRLIB types definitions
#include "spi.h"		// include spi bus support
#include "global.h"
#include "mmc.h"
#include "fat.h"
#include "display.h"
// include project-specific hardware configuration
#include "mmcconf.h"

u32 n_actual_mmc_sector;
u08 n_actual_mmc_sector_needswrite;
extern unsigned char mmc_sector_buffer[512];
struct flags SDFlags;

unsigned char crc7 (unsigned char crc, unsigned char *pc, unsigned int len) { 
	//unsigned int i; 
	unsigned char ibit; 
	unsigned char c; 
 
	//for (i = 0; i < len; i++, pc++) { 
	while (len--) { 	// reverse byteorder!
		c = *(pc+len); 
		for (ibit = 0; ibit < 8; ibit++) { 
			crc <<= 1; 
			if ((c ^ crc) & 0x80) 
				crc ^= 0x09; 
			c <<= 1; 
		} 
		crc &= 0x7F; 
	} 
	return crc; 
} 

void mmcInit(void)
{
	// initialize SPI interface
	spiInit();
	//// allready done in spiInit()
	// release chip select
	//sbi(MMC_CS_PORT,MMC_CS_PIN);	//high
}

u08 mmcReset(void)
{
	u16 retry;
	u08 r1;
	u08 i;
	u08 *buffer=mmc_sector_buffer;
	u08 *b=mmc_sector_buffer;

	n_actual_mmc_sector=0xFFFFFFFF;	//!!! pridano dovnitr
	n_actual_mmc_sector_needswrite=0;

	SDFlags.SDHC = 0;	// reset SDHC-Flag

	//
	//i=0xff; //255x
	i=10;	//10x8bit=80clocks (74 lt. Spec.)
	do { spiTransferFF(); i--; } while(i);

	//
	i=10;	//try more times, a Kingston 8GB SDHC returns 0x3f at first time
	do {
		r1 = mmcSendCommand(MMC_GO_IDLE_STATE, 0);	// CMD0
		if(r1 == 1) break;
	}
	while(--i);

	//if (r1!=1) {
	if (!i) {
		*b = r1;	// debug only
		return 1;
	}

	i=10;	//Transcent 32GB needs a bit more tries on first start
	do {
		cbi(MMC_CS_PORT,MMC_CS_PIN);
		r1 = mmcCommand(MMC_SEND_IF_COND, 0b000110101010);	// CMD8
		if (r1==1) {	// get the response
			i=4;
			do { *b++=spiTransferFF(); i--; } while(i);
			// check return values
			if (buffer[2] != 1 || buffer[3] != 0xaa) r1 = 6;	// fake error
		}
		sbi(MMC_CS_PORT,MMC_CS_PIN);
		spiTransferFF();	// send 8 clocks at end
		//exit on correct response or invalid command
		if(r1 == 1 || r1 == 5) break;
	} while(i--);

	if (r1>5) {
		*b = r1;	// debug only
		return(2);
	}

	buffer += 4;	// inc the buffer for next cmd
	b = buffer;

	// sundisk 64MB braucht 48 Zyklen
	retry=0x3ff;	// give card time to init
	do
	{
		r1 = mmcSendCommand(MMC_APP_CMD, 0);	// CMD55(APP_CMD)
		if (r1>1) break;	//MMC
					//bit 0 - in idle state, bit 2 - illegal command
		r1 = mmcSendCommand(MMC_SD_SEND_OP_COND, 1L<<30);	// 0x29(ACMD41) HCS-bit set
		if (r1==0) { //SD
			cbi(MMC_CS_PORT,MMC_CS_PIN);
			r1 = mmcCommand(MMC_READ_OCR, 0);		// CMD58
			i=4;
			do { *b++=spiTransferFF(); i--; } while(i);
			sbi(MMC_CS_PORT,MMC_CS_PIN);
			spiTransferFF();	// send 8 clocks at end
			//mark SDHC
			SDFlags.SDHC = (buffer[0] & 0x40) ? 1 : 0;
			break;	//and out
		}
		retry--;
	}
	while(retry);
	//return(r1);	//only for testing
	//return(0xff - retry);	//only for testing

	if(retry == 0) return(3);
	if(r1 == 5) {	// try for MMC
		r1 = mmcSendCommand(MMC_SEND_OP_COND, 0);	// CMD1
		if (r1!=0) return 4;
	}

	//// now we have a crc routine \o/
	// turn off CRC checking to simplify communication
	//r1 = 
	//mmcSendCommand(MMC_CRC_ON_OFF, 0);		// CMD59

	// set block length to 512 bytes
	//r1 = 
	r1 = mmcSendCommand(MMC_SET_BLOCKLEN, 512);		// CMD16
	if (r1!=0) return 5;

	// return success
	return 0;
}

u08 mmcSendCommand(u08 cmd, u32 arg)
{
	u08 r1;

	// assert chip select
	cbi(MMC_CS_PORT,MMC_CS_PIN);
	// issue the command
	r1 = mmcCommand(cmd, arg);
	// release chip select
	sbi(MMC_CS_PORT,MMC_CS_PIN);
	spiTransferFF();	// send 8 clocks at end

	return r1;
}

u08 mmcRead(u32 sector)
{
	u08 r1;
	u16 i;
	u08 *buffer=mmc_sector_buffer;	//natvrdo!

	//too expensive for atx support!
	//Draw_Circle(15,5,3,1,Green);

	// assert chip select
	cbi(MMC_CS_PORT,MMC_CS_PIN);
	// issue command
	if (!SDFlags.SDHC) sector<<=9;
	r1 = mmcCommand(MMC_READ_SINGLE_BLOCK, sector);

	// check for valid response
	if(r1 != 0x00) return r1;

	// wait for block start
	while(spiTransferFF() != MMC_STARTBLOCK_READ);

	//zacatek bloku

	//nacti 512 bytu
	i=0x200;	//512
	do { *buffer++ = spiTransferFF(); i--; } while(i);

	// read 16-bit CRC
	//2x FF:
	spiTransferFF();
	spiTransferFF();
	// release chip select
	sbi(MMC_CS_PORT,MMC_CS_PIN);
	spiTransferFF();	// send 8 clocks at end
	//
	//Draw_Circle(15,5,3,1,Black);
	return 0;	//success
}

u08 mmcWrite(u32 sector)
{
	u08 r1;
	u16 i;
	u08 *buffer=mmc_sector_buffer;	//natvrdo!

        //LED_RED_ON;	//TODO
	//Draw_Circle(15,5,3,1,Red);

	// assert chip select
	cbi(MMC_CS_PORT,MMC_CS_PIN);
	// issue command
	if (!SDFlags.SDHC) sector<<=9;
	r1 = mmcCommand(MMC_WRITE_BLOCK, sector);
	// check for valid response
	if(r1 != 0x00)
		return r1;
	// send dummy
	spiTransferFF();
	// send data start token
	spiTransferByte(MMC_STARTBLOCK_WRITE);

	// write data (512 bytes)
	i=0x200;
	do { spiTransferByte(*buffer++); i--; }	while(i);

	// write 16-bit CRC (dummy values)
	//2x FF:
	spiTransferFF();
	spiTransferFF();
	// read data response token
	r1 = spiTransferFF();
	if( (r1&MMC_DR_MASK) != MMC_DR_ACCEPT)
		return r1;
	// wait until card not busy
	while(!spiTransferFF());
	// release chip select
	sbi(MMC_CS_PORT,MMC_CS_PIN);
	spiTransferFF();	// send 8 clocks at end

        //LED_RED_OFF;	//TODO
	//Draw_Circle(15,5,3,1,Black);

	// return success
	return 0;
}

u08 mmcCommand(u08 cmd, u32 arg)
{
	u08 r1;
	u08 retry=0xff;	//255x
	unsigned char crc;
	//

/* what?
	spiTransferFF();
	spiTransferFF();	//pridano navic! 27.6.2008 doporucil Bob!k
	spiTransferFF();	//pridano navic! 27.6.2008 doporucil Bob!k
	spiTransferFF();	//pridano navic! 27.6.2008 doporucil Bob!k
*/
	cmd |= 0x40;
	// send command
	spiTransferByte(cmd);
	//
	spiTransferByte(arg>>24);
	spiTransferByte(arg>>16);
	spiTransferByte(arg>>8);
	spiTransferByte(arg);
	//Alternativni zpusob pres ((u08*)&arg)[] je delsi.

	//spiTransferByte(0x95);	// crc valid only for MMC_GO_IDLE_STATE
	// calculate crc
	crc = crc7(0, &cmd, 1);
	crc = crc7(crc, (u08*) &arg, 4)<<1 | 0x01;
	// and send it
	spiTransferByte(crc);
	// end command
	// wait for response
	// if more than 255 retries, card has timed-out
	// return the received 0xFF
	do
	{
		r1 = spiTransferFF();
		retry--;
	}
	//while(retry && (r1 == 0xFF) );
	while(retry && (r1 & 0x80) );

	// return response
	return r1;
}

u08 mmcReadCached(u32 sector)
{
        if(sector==n_actual_mmc_sector) return(-1);

        u08 ret,retry;
        //save cache before read another sector
        mmcWriteCachedFlush();
        //from now on
        retry=0; //maximal 256x tries
        do
        {
                ret = mmcRead(sector);  //returns 0 if ok
                retry--;
        } while (ret && retry);
        if(ret)	// exit on error
		return(-1);
        n_actual_mmc_sector=sector;
	return(0);
}

u08 mmcWriteCached(unsigned char force)
{
        //if ( get_readonly() ) return 0xff; //zakazany zapis
        //LED_RED_ON;	//signal cache is not written yet
	Draw_Circle(15,5,3,1,Red);
        if (force)
        {
                u08 ret,retry;
                retry=16; //maximal 16x tries
                do
                {
                        ret = mmcWrite(n_actual_mmc_sector); //returns 0 if ok
                        retry--;
                } while (ret && retry);
                while(ret); //and if it did not work, SDrive blocks it!
                n_actual_mmc_sector_needswrite = 0;
                //LED_RED_OFF;
		Draw_Circle(15,5,3,1,Black);
        }
        else
        {
                n_actual_mmc_sector_needswrite=1;
        }
        return 0; //return 0 if ok
}

void mmcWriteCachedFlush()
{
        if (n_actual_mmc_sector_needswrite)
        {
         while (mmcWriteCached(1)); //if it's forbidden to write, SDrive blocks it!
	//until the write is not allowed (you are hanging something in the cache)
        }
}
