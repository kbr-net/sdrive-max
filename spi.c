/*! \file spi.c \brief SPI interface driver. */
//*****************************************************************************
//
// File Name	: 'spi.c'
// Title		: SPI interface driver
// Author		: Pascal Stang - Copyright (C) 2000-2002
// Created		: 11/22/2000
// Revised		: 06/06/2002
// Version		: 0.6
// Target MCU	: Atmel AVR series
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
// 19.05.2014
// enhanced by kbr
// ----------------------------------------------------------------------------
//
// This code is distributed under the GNU Public License
//		which can be found at http://www.gnu.org/licenses/gpl.txt
//
//*****************************************************************************

#include <avr/io.h>
//#include <avr/signal.h>

#include "spi.h"

// access routines
void spiInit()
{
#ifdef __AVR_ATmega128__
	// setup SPI I/O pins
	sbi(PORTB, 0);	// SS high first
	sbi(DDRB, 0);	// SS must be output for Master mode to work
	sbi(PORTB, 1);	// set SCK hi
	sbi(DDRB, 1);	// set SCK as output
	cbi(DDRB, 3);	// set MISO as input
	//sbi(PORTB, 3);	// set MISO pullup !!! Now in hardware to 3.3V
	sbi(DDRB, 2);	// set MOSI as output
#elif defined(__AVR_ATmega8__) || defined(__AVR_ATmega328__) || defined(__AVR_ATmega168__)
	// setup SPI I/O pins
	sbi(PORTB, 2);	// SS high first
	sbi(DDRB, 2);   // SS must be output for Master mode to work
	sbi(PORTB, 5);  // set SCK hi
	sbi(DDRB, 5);   // set SCK as output
	cbi(DDRB, 4);   // set MISO as input
	//sbi(PORTB, 4);	// set MISO pullup !!! Now in hardware to 3.3V
	sbi(DDRB, 3);   // set MOSI as output
#else	// atmega32
	// setup SPI I/O pins
	sbi(PORTB, 4);	// SS high first
	sbi(DDRB, 4);	// SS must be output for Master mode to work
	sbi(PORTB, 7);	// set SCK hi
	sbi(DDRB, 7);	// set SCK as output
	cbi(DDRB, 6);	// set MISO as input
	//sbi(PORTB, 6);	// set MISO pullup !!! Now in hardware to 3.3V
	sbi(DDRB, 5);	// set MOSI as output
#endif
	
	// setup SPI interface :
	// clock = f/128	slowest speed for Init
/*
	cbi(SPSR, SPI2X);
	sbi(SPCR, SPR0);
	sbi(SPCR, SPR1);
	// master mode
	sbi(SPCR, MSTR);

	// select clock phase positive-going in middle of data
	cbi(SPCR, CPOL);
	// Data order MSB first
	cbi(SPCR,DORD);
	// enable SPI
	sbi(SPCR, SPE);
*/
	SPSR &= ~_BV(SPI2X);
	SPCR = (_BV(SPR0)|_BV(SPR1)|_BV(MSTR)|_BV(SPE));

	// clear status
	inb(SPSR);
	//inb(SPDR);
}

void spiSendByte(u08 data)
{
	// send a byte over SPI and ignore reply
	while(!(inb(SPSR) & (1<<SPIF)));

	outb(SPDR, data);
}

u08 spiTransferByte(u08 data)
{
	// send the given data
	outb(SPDR, data);

	// wait for transfer to complete
	while(!(inb(SPSR) & (1<<SPIF)));
		// *** reading of the SPSR and SPDR are crucial
		// *** to the clearing of the SPIF flag
		// *** in non-interrupt mode
		//inb(SPDR);
	// return the received data
	return inb(SPDR);
}

u08 spiTransferFF()
{
	return spiTransferByte(0xFF);
}

//#define spiTransferTwoFF()	{ spiTransferFF(); spiTransferFF(); }

/*
void spiTransfer10xFF()
{
	u08 num=10;
	do
	{
		spiTransferFF();
		num--;
	} 
	while(num);
}
*/

/*
u16 spiTransferWord(u16 data)
{
	u16 rxData = 0;

	// send MS byte of given data
	rxData = (spiTransferByte((data>>8) & 0x00FF))<<8;
	// send LS byte of given data
	rxData |= (spiTransferByte(data & 0x00FF));

	// return the received data
	return rxData;
}
*/
