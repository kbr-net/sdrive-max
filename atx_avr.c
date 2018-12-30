/*! \file atx_avr.c \brief AVR-specific functions for ATX file handling. */
//*****************************************************************************
//
// File Name    : 'atx_avr.c'
// Title        : AVR-specific functions for ATX file handling
// Author       : Daniel Noguerol
// Date         : 21/01/2018
// Revised      : 21/01/2018
// Version      : 0.1
// Target MCU   : ???
// Editor Tabs  : 4
//
// NOTE: This code is currently below version 1.0, and therefore is considered
// to be lacking in some functionality or documentation, or may not be fully
// tested.  Nonetheless, you can expect most functions to work.
//
// This code is distributed under the GNU Public License
//      which can be found at http://www.gnu.org/licenses/gpl.txt
//
//*****************************************************************************

#include <stdlib.h>
#include <avr/io.h>
#include <util/delay.h>
#include "avrlibtypes.h"
#include "atx.h"
#include "fat.h"
#include "tft.h"

extern struct display tft;

void waitForAngularPosition(u16 pos) {
    // if the position is less than the current timer, we need to wait for a rollover 
    // to occur
    if (pos < TCNT1 / 2) {
        TIFR1 |= _BV(OCF1A);
        while (!(TIFR1 & _BV(OCF1A)));
    }
    // wait for the timer to reach the target position
    while (TCNT1 / 2 < pos);
}

u16 getCurrentHeadPosition() {
    // TCNT1 is a variable driven by an Atmel timer that ticks every 4 microseconds. A full 
    // rotation of the disk is represented in an ATX file by an angular positional value 
    // between 1-26042 (or 8 microseconds based on 288 rpms). So, TCNT1 / 2 always gives the 
    // current angular position of the drive head on the track any given time assuming the 
    // disk is spinning continously.
    return TCNT1 / 2;
}

#ifndef __AVR__ // note that byte swapping is not needed on AVR platforms, so we remove the functions to conserve resources
void byteSwapAtxFileHeader(struct atxFileHeader * header) {
    // AVR implementation is a NO-OP
}

void byteSwapAtxTrackHeader(struct atxTrackHeader * header) {
    // AVR implementation is a NO-OP
}

void byteSwapAtxSectorListHeader(struct atxSectorListHeader * header) {
    // AVR implementation is a NO-OP
}

void byteSwapAtxSectorHeader(struct atxSectorHeader * header) {
    // AVR implementation is a NO-OP
}

void byteSwapAtxTrackChunk(struct atxTrackChunk *header) {
    // AVR implementation is a NO-OP
}
#endif

u08 is_1050() {
    return(tft.cfg.drive_type);
}
