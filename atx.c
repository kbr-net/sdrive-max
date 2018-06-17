/*! \file atx.c \brief ATX file handling. */
//*****************************************************************************
//
// File Name	: 'atx.c'
// Title		: ATX file handling
// Author		: Daniel Noguerol
// Date			: 21/01/2018
// Revised		: 21/01/2018
// Version		: 0.1
// Target MCU	: ???
// Editor Tabs	: 4
//
// NOTE: This code is currently below version 1.0, and therefore is considered
// to be lacking in some functionality or documentation, or may not be fully
// tested.  Nonetheless, you can expect most functions to work.
//
// This code is distributed under the GNU Public License
//		which can be found at http://www.gnu.org/licenses/gpl.txt
//
//*****************************************************************************

#include <stdlib.h>
#include <avr/io.h>
#include <util/delay.h>
#include "avrlibtypes.h"
#include "atx.h"
#include "fat.h"

struct atxTrackInfo {
    u32 offset;   // absolute position within file for start of track header
};

extern unsigned char atari_sector_buffer[256];

u16 gBytesPerSector;                    // number of bytes per sector
u08 gSectorsPerTrack;                   // number of sectors in each track
struct atxTrackInfo gTrackInfo[40];     // pre-calculated info for each track
u16 gLastAngle;
u08 gLastTrack = 0;

u16 loadAtxFile() {
    struct atxFileHeader *fileHeader;
    struct atxTrackHeader *trackHeader;

    // read the file header
    faccess_offset(FILE_ACCESS_READ, 0, sizeof(struct atxFileHeader));

    // validate the ATX file header
    fileHeader = (struct atxFileHeader*)atari_sector_buffer;
    if (fileHeader->signature[0] != 'A' ||
        fileHeader->signature[1] != 'T' ||
        fileHeader->signature[2] != '8' ||
        fileHeader->signature[3] != 'X' ||
        fileHeader->version != ATX_VERSION ||
        fileHeader->minVersion != ATX_VERSION) {
        return 0;
    }

    // enhanced density is 26 sectors per track, single and double density are 18
    gSectorsPerTrack = (fileHeader->density == 1) ? (u08)26 : (u08)18;
    // single and enhanced density are 128 bytes per sector, double density is 256
    gBytesPerSector = (fileHeader->density == 1) ? (u16)256 : (u16)128;

    // calculate track offsets
    u32 startOffset = fileHeader->startData;
    while (1) {
        if (!faccess_offset(FILE_ACCESS_READ, startOffset, sizeof(struct atxTrackHeader))) {
            break;
        }
        trackHeader = (struct atxTrackHeader*)atari_sector_buffer;
        gTrackInfo[trackHeader->trackNumber].offset = startOffset;
        startOffset += trackHeader->size;
    }

    return gBytesPerSector;
}

u16 loadAtxSector(u16 num, unsigned short *sectorSize, u08 *status) {
    struct atxTrackHeader *trackHeader;
    struct atxSectorListHeader *slHeader;
    struct atxSectorHeader *sectorHeader;
    struct atxExtendedSectorData *extSectorData;

    u16 i;
    u16 tgtSectorIndex = 0;         // the index of the target sector within the sector list
    u32 tgtSectorOffset = 0;        // the offset of the target sector data
    BOOL hasError = (BOOL)FALSE;    // flag for drive status errors

    // local variables used for weak data handling
    u08 extendedDataRecords = 0;
    u32 maxSectorOffset = 0;
    int weakOffset = -1;

    // calculate track and relative sector number from the absolute sector number
    u08 tgtTrackNumber = (num - 1) / gSectorsPerTrack + 1;
    u08 tgtSectorNumber = (num - 1) % gSectorsPerTrack + 1;

    // set initial status (in case the target sector is not found)
    *status = 0xF7;

    if (tgtTrackNumber > 40)
	return 0;

    // read the track header
    u32 currentFileOffset = gTrackInfo[tgtTrackNumber - 1].offset;
    faccess_offset(FILE_ACCESS_READ, currentFileOffset, sizeof(struct atxTrackHeader));
    trackHeader = (struct atxTrackHeader*)atari_sector_buffer;
    u16 sectorCount = trackHeader->sectorCount;
    if (sectorCount == 0 || trackHeader->trackNumber != tgtTrackNumber - 1)
	return 0;

    // read the sector list header
    currentFileOffset += trackHeader->headerSize;
    faccess_offset(FILE_ACCESS_READ, currentFileOffset, sizeof(struct atxSectorListHeader));
    slHeader = (struct atxSectorListHeader*)atari_sector_buffer;

    // sector list header is variable length, so skip any extra header bytes that may be present
    currentFileOffset += slHeader->next - sectorCount * sizeof(struct atxSectorHeader);

    if (gLastTrack != tgtTrackNumber) {
	signed char diff;
	diff = tgtTrackNumber - gLastTrack;
	if (diff < 0) diff *= -1;
	//simulate track seek time, some protection seems to depend on it also
	for (i = 0; i < diff; i++) {
	    _delay_ms(40);		// wait for each track
	}
	TIFR1 |= _BV(OCF1A);		// clear interrupt flag
	while (! (TIFR1 & _BV(OCF1A)));	// additional wait for counter overflow,
					// there should be a track header
    }
    gLastTrack = tgtTrackNumber;

    // calculate starting sector by timer. This is not really perfect,
    // because sector position may vari in the files, but it's good for now.
    u16 start = (TCNT1 / 2) / (26042 / sectorCount);
    // add start position to file offset
    currentFileOffset += start * sizeof(struct atxSectorHeader);
    // iterate through all sector headers to find the target sector
    i = start;
    do {
	if (i == sectorCount) {	// turn over on last sector index
	    i = 0;
	    currentFileOffset -= sectorCount * sizeof(struct atxSectorHeader);
	    continue;
	}
        if (faccess_offset(FILE_ACCESS_READ, currentFileOffset, sizeof(struct atxSectorHeader))) {
            sectorHeader = (struct atxSectorHeader*)atari_sector_buffer;
            // if the sector number matches the one we're looking for...
            if (sectorHeader->number == tgtSectorNumber) {
		//save angle
		gLastAngle = sectorHeader->timev;
                *status = ~(sectorHeader->status);
                tgtSectorIndex = i;
                tgtSectorOffset = sectorHeader->data;
                maxSectorOffset = sectorHeader->data > maxSectorOffset ? sectorHeader->data : maxSectorOffset;
                // if the sector status is not valid, flag it as an error
                if (sectorHeader->status > 0) {
                    hasError = (BOOL)TRUE;
                }
                // if the extended data flag is set, increment extended record count for later reading
                if (sectorHeader->status & 0x40) {
                    extendedDataRecords++;
                }
		break;
            }
            currentFileOffset += sizeof(struct atxSectorHeader);
        }
	i++;
    } while (i != start);	// exit if reached start sector index again

    // read through the extended data records if any were found
    if (extendedDataRecords > 0) {
        currentFileOffset = gTrackInfo[tgtTrackNumber - 1].offset + maxSectorOffset + gBytesPerSector;
        for (i=0; i < extendedDataRecords; i++) {
            if (faccess_offset(FILE_ACCESS_READ, currentFileOffset, sizeof(struct atxExtendedSectorData))) {
                extSectorData = (struct atxExtendedSectorData *) atari_sector_buffer;
                // if the target sector has a weak data flag, grab the start weak offset within the sector data
                if (extSectorData->sectorIndex == tgtSectorIndex && extSectorData->type == 0x10) {
                    weakOffset = extSectorData->data;
                }
            }
            currentFileOffset += sizeof(struct atxExtendedSectorData);
        }
    }

    // set the sector status and size
    *sectorSize = gBytesPerSector;

    // read the data (re-using tgtSectorIndex variable here to reduce stack consumption)
    tgtSectorIndex = tgtSectorOffset ? (u16)faccess_offset(FILE_ACCESS_READ, gTrackInfo[tgtTrackNumber - 1].offset + tgtSectorOffset, gBytesPerSector) : (u16)0;
    tgtSectorIndex = hasError ? (u16)0 : tgtSectorIndex;

    // if a weak offset is defined, randomize the appropriate data
    if (weakOffset > -1) {
        for (i=(u16)weakOffset; i < gBytesPerSector; i++) {
            atari_sector_buffer[i] = (unsigned char)(rand() % 256);
        }
    }

    // wait how long real drive needs to read the sector
    u16 waitpos = (gLastAngle + 1208) % 26042;	// add 1208 as time to read for current sector
						// (may not work on enhanced density disks, but never seen yet)
    if (waitpos < TCNT1/2)	// if we have an overflow in waitpos
	TIFR1 |= _BV(OCF1A);	// clear interrupt flag first, may be cached here, because interrupts disabled!
	while (! (TIFR1 & _BV(OCF1A)));	// additional wait for counter overflow
    while (TCNT1/2 < waitpos);	// wait timer reaches position

    // return the number of bytes read
    return tgtSectorIndex;
}

