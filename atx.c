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

// number of angular units in a full disk rotation
#define AU_FULL_ROTATION         26042
// number of angular units to read one sector
#define AU_ONE_SECTOR_READ       1208
// number of ms for each angular unit
#define MS_ANGULAR_UNIT_VAL      0.007999897601
// number of milliseconds drive takes to process a request
#define MS_DRIVE_REQUEST_DELAY   3.22
// number of milliseconds to calculate CRC
#define MS_CRC_CALCULATION       2
// number of milliseconds drive takes to step 1 track
#define MS_TRACK_STEP            5.3
// number of milliseconds drive head takes to settle after track stepping
#define MS_HEAD_SETTLE           10

struct atxTrackInfo {
    u32 offset;   // absolute position within file for start of track header
};

extern unsigned char atari_sector_buffer[256];
extern u16 last_angle_returned; // extern so we can display it on the screen

u16 gBytesPerSector;                    // number of bytes per sector
u08 gSectorsPerTrack;                   // number of sectors in each track
struct atxTrackInfo gTrackInfo[2][40];  // pre-calculated info for each track and drive
                                        // support slot D1 and D2 only because of insufficient RAM!
u16 gLastAngle = 0;
u08 gCurrentHeadTrack = 0;

u16 loadAtxFile(u08 drive) {
    struct atxFileHeader *fileHeader;
    struct atxTrackHeader *trackHeader;

    // read the file header
    faccess_offset(FILE_ACCESS_READ, 0, sizeof(struct atxFileHeader));

    // validate the ATX file header
    fileHeader = (struct atxFileHeader *) atari_sector_buffer;
    if (fileHeader->signature[0] != 'A' ||
        fileHeader->signature[1] != 'T' ||
        fileHeader->signature[2] != '8' ||
        fileHeader->signature[3] != 'X' ||
        fileHeader->version != ATX_VERSION ||
        fileHeader->minVersion != ATX_VERSION) {
        return 0;
    }

    // enhanced density is 26 sectors per track, single and double density are 18
    gSectorsPerTrack = (fileHeader->density == 1) ? (u08) 26 : (u08) 18;
    // single and enhanced density are 128 bytes per sector, double density is 256
    gBytesPerSector = (fileHeader->density == 1) ? (u16) 256 : (u16) 128;

    // calculate track offsets
    u32 startOffset = fileHeader->startData;
    while (1) {
        if (!faccess_offset(FILE_ACCESS_READ, startOffset, sizeof(struct atxTrackHeader))) {
            break;
        }
        trackHeader = (struct atxTrackHeader *) atari_sector_buffer;
        gTrackInfo[drive][trackHeader->trackNumber].offset = startOffset;
        startOffset += trackHeader->size;
    }

    return gBytesPerSector;
}

u16 loadAtxSector(u08 drive, u16 num, unsigned short *sectorSize, u08 *status) {
    struct atxTrackHeader *trackHeader;
    struct atxSectorListHeader *slHeader;
    struct atxSectorHeader *sectorHeader;
    struct atxExtendedSectorData *extSectorData;

    u16 i;
    u16 tgtSectorIndex = 0;         // the index of the target sector within the sector list
    u32 tgtSectorOffset = 0;        // the offset of the target sector data
    BOOL hasError = (BOOL) FALSE;   // flag for drive status errors

    // local variables used for weak data handling
    u08 extendedDataRecords = 0;
    u32 maxSectorOffset = 0;
    int weakOffset = -1;

    // calculate track and relative sector number from the absolute sector number
    u08 tgtTrackNumber = (num - 1) / gSectorsPerTrack + 1;
    u08 tgtSectorNumber = (num - 1) % gSectorsPerTrack + 1;

    // set initial status (in case the target sector is not found)
    *status = 0xF7;

    // immediately fail on track read > 40
    if (tgtTrackNumber > 40) {
        return 0;
    }

    // delay for the time the drive takes to process the request
    _delay_ms(MS_DRIVE_REQUEST_DELAY);

    // delay for track stepping if needed
    if (gCurrentHeadTrack != tgtTrackNumber) {
        signed char diff;
        diff = tgtTrackNumber - gCurrentHeadTrack;
        if (diff < 0) diff *= -1;
        // wait for each track (this is done in a loop since _delay_ms needs a compile-time constant)
        for (i = 0; i < diff; i++) {
            _delay_ms(MS_TRACK_STEP);
        }
        // delay for head settling
        _delay_ms(MS_HEAD_SETTLE);
    }

    // set new head track position
    gCurrentHeadTrack = tgtTrackNumber;

    // sample current head position
    u16 headPosition = getCurrentHeadPosition();

    // read the track header
    u32 currentFileOffset = gTrackInfo[drive][tgtTrackNumber - 1].offset;
    faccess_offset(FILE_ACCESS_READ, currentFileOffset, sizeof(struct atxTrackHeader));
    trackHeader = (struct atxTrackHeader *) atari_sector_buffer;
    u16 sectorCount = trackHeader->sectorCount;

    // if there are no sectors in this track or the track number doesn't match, return error
    if (sectorCount == 0 || trackHeader->trackNumber != tgtTrackNumber - 1) {
        return 0;
    }

    // read the sector list header
    currentFileOffset += trackHeader->headerSize;
    faccess_offset(FILE_ACCESS_READ, currentFileOffset, sizeof(struct atxSectorListHeader));
    slHeader = (struct atxSectorListHeader *) atari_sector_buffer;

    // sector list header is variable length, so skip any extra header bytes that may be present
    currentFileOffset += slHeader->next - sectorCount * sizeof(struct atxSectorHeader);

    // iterate through all sector headers to find the target sector
    int pTT = 0;
    for (i=0; i < sectorCount; i++) {
        if (faccess_offset(FILE_ACCESS_READ, currentFileOffset, sizeof(struct atxSectorHeader))) {
            sectorHeader = (struct atxSectorHeader *) atari_sector_buffer;
            // if the sector number matches the one we're looking for...
            if (sectorHeader->number == tgtSectorNumber) {
                // check if it's the next sector that the head would encounter angularly...
                int tt = sectorHeader->timev - headPosition;
                if (pTT == 0 || (tt > 0 && pTT < 0) || (tt > 0 && pTT > 0 && tt < pTT) || (tt < 0 && pTT < 0 && tt < pTT)) {
                    pTT = tt;
                    gLastAngle = sectorHeader->timev;
                    *status = ~(sectorHeader->status);
                    tgtSectorIndex = i;
                    tgtSectorOffset = sectorHeader->data;
                    maxSectorOffset = sectorHeader->data > maxSectorOffset ? sectorHeader->data : maxSectorOffset;
                    // if the sector status is not valid, flag it as an error
                    if (sectorHeader->status > 0) {
                        hasError = (BOOL) TRUE;
                    }
                    // if the extended data flag is set, increment extended record count for later reading
                    if (sectorHeader->status & 0x40) {
                        extendedDataRecords++;
                    }
                }
            }
            currentFileOffset += sizeof(struct atxSectorHeader);
        }
    }
    last_angle_returned = gLastAngle;

    // read through the extended data records if any were found
    if (extendedDataRecords > 0) {
        currentFileOffset = gTrackInfo[drive][tgtTrackNumber - 1].offset + maxSectorOffset + gBytesPerSector;
        for (i = 0; i < extendedDataRecords; i++) {
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
    tgtSectorIndex = tgtSectorOffset ? (u16) faccess_offset(FILE_ACCESS_READ, gTrackInfo[drive][tgtTrackNumber - 1].offset + tgtSectorOffset, gBytesPerSector) : (u16) 0;
    tgtSectorIndex = hasError ? (u16) 0 : tgtSectorIndex;

    // if a weak offset is defined, randomize the appropriate data
    if (weakOffset > -1) {
        for (i = (u16) weakOffset; i < gBytesPerSector; i++) {
            atari_sector_buffer[i] = (unsigned char) (rand() % 256);
        }
    }

    // calculate rotational delay of sector seek
    u16 rotationDelay;
    if (gLastAngle > headPosition) {
        rotationDelay = (gLastAngle - headPosition);
    } else {
        rotationDelay = (AU_FULL_ROTATION - headPosition + gLastAngle);
    }

    // determine the angular position we need to wait for by summing the head position, rotational delay and the number 
    // of rotational units for a sector read. Then wait for the head to reach that position.
    // (Concern: can the SD card read take more time than the amount the disk would have rotated?)
    waitForAngularPosition(incAngularDisplacement(incAngularDisplacement(headPosition, rotationDelay), AU_ONE_SECTOR_READ));

    // delay for CRC calculation
    _delay_ms(MS_CRC_CALCULATION);

    // return the number of bytes read
    return tgtSectorIndex;
}

u16 incAngularDisplacement(u16 start, u16 delta) {
    // increment an angular position by a delta taking a full rotation into consideration
    u16 ret = start + delta;
    if (ret > AU_FULL_ROTATION) {
        ret -= AU_FULL_ROTATION;
    }
    return ret;
}

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
