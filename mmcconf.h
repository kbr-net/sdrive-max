/*! \file mmcconf.h \brief MultiMedia and SD Flash Card Interface Configuration. */
//*****************************************************************************
//
// File Name	: 'mmcconf.h'
// Title		: MultiMedia and SD Flash Card Interface Configuration
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
//
// This code is distributed under the GNU Public License
//		which can be found at http://www.gnu.org/licenses/gpl.txt
//
//*****************************************************************************

#ifndef MMCCONF_H
#define MMCCONF_H

// MMC card chip select pin defines
#define MMC_CS_PORT			PORTB
#define MMC_CS_DDR			DDRB
#if defined(__AVR_ATmega8__) || defined(__AVR_ATmega168__) || defined(__AVR_ATmega328__)
	#define MMC_CS_PIN			2
#else
	#define MMC_CS_PIN			4
#endif

#endif
