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
#include <util/delay.h>
#include "avrlibtypes.h"
#include "fat.h"
#include "atx.h"
#include "atx_avr.h"

// number of angular units in a full disk rotation
#define AU_FULL_ROTATION         26042
// number of angular units to read one sector
#define AU_ONE_SECTOR_READ       1208
// number of ms for each angular unit
#define MS_ANGULAR_UNIT_VAL      0.007999897601
// number of milliseconds drive takes to process a request
#define MS_DRIVE_REQUEST_DELAY_810  3.22
#define MS_DRIVE_REQUEST_DELAY_1050 3.22
// number of milliseconds to calculate CRC
#define MS_CRC_CALCULATION_810   2
#define MS_CRC_CALCULATION_1050  2
// number of milliseconds drive takes to step 1 track
#define MS_TRACK_STEP_810        5.3
#define MS_TRACK_STEP_1050       12.41
// number of milliseconds drive head takes to settle after track stepping
#define MS_HEAD_SETTLE_810       0
#define MS_HEAD_SETTLE_1050      40
// mask for checking FDC status "data lost" bit
#define MASK_FDC_DLOST           0x04
// mask for checking FDC status "missing" bit
#define MASK_FDC_MISSING         0x10
// mask for checking FDC status extended data bit
#define MASK_EXTENDED_DATA       0x40

#define MAX_RETRIES_1050         1
#define MAX_RETRIES_810          4

struct atxTrackInfo {
    u32 offset;   // absolute position within file for start of track header
};

extern unsigned char atari_sector_buffer[256];
extern u16 last_angle_returned; // extern so we can display it on the screen

u16 gBytesPerSector;                                 // number of bytes per sector
u08 gSectorsPerTrack;                                // number of sectors in each track
struct atxTrackInfo gTrackInfo[MAX_TRACK];                  // pre-calculated info for each track
u16 gLastAngle;
u08 gCurrentHeadTrack;

u16 loadAtxFile() {
    struct atxFileHeader *fileHeader;
    struct atxTrackHeader *trackHeader;

    // read the file header
    faccess_offset(FILE_ACCESS_READ, 0, sizeof(struct atxFileHeader));
#ifndef __AVR__
    byteSwapAtxFileHeader((struct atxFileHeader *) atari_sector_buffer);
#endif

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
    u08 track;
    for (track = 0; track < MAX_TRACK; track++) {
        if (!faccess_offset(FILE_ACCESS_READ, startOffset, sizeof(struct atxTrackHeader))) {
            break;
        }
        trackHeader = (struct atxTrackHeader *) atari_sector_buffer;

#ifndef __AVR__ // note that byte swapping is not needed on AVR platforms, so we remove the calls to conserve resources
        byteSwapAtxTrackHeader(trackHeader);
#endif
        gTrackInfo[track].offset = startOffset;
        startOffset += trackHeader->size;
    }

    return gBytesPerSector;
}

u16 loadAtxSector(u16 num, unsigned short *sectorSize, u08 *status) {
    struct atxTrackHeader *trackHeader;
    struct atxSectorListHeader *slHeader;
    struct atxSectorHeader *sectorHeader;
    struct atxTrackChunk *extSectorData;

    u16 i;
    u16 tgtSectorIndex = 0;         // the index of the target sector within the sector list
    u32 tgtSectorOffset = 0;        // the offset of the target sector data
    BOOL hasError = (BOOL) FALSE;   // flag for drive status errors

    // local variables used for weak data handling
    u08 extendedDataRecords = 0;
    int16_t weakOffset = -1;

    // calculate track and relative sector number from the absolute sector number
    u08 tgtTrackNumber = (num - 1) / gSectorsPerTrack + 1;
    u08 tgtSectorNumber = (num - 1) % gSectorsPerTrack + 1;

    // set initial status (in case the target sector is not found)
    *status = 0x10;
    // set the sector size
    *sectorSize = gBytesPerSector;

    // delay for the time the drive takes to process the request
    if (is_1050()) {
        _delay_ms(MS_DRIVE_REQUEST_DELAY_1050);
    } else {
        _delay_ms(MS_DRIVE_REQUEST_DELAY_810);
    }

    // delay for track stepping if needed
    if (gCurrentHeadTrack != tgtTrackNumber) {
        signed char diff;
        diff = tgtTrackNumber - gCurrentHeadTrack;
        if (diff < 0) diff *= -1;
        // wait for each track (this is done in a loop since _delay_ms needs a compile-time constant)
        for (i = 0; i < diff; i++) {
            if (is_1050()) {
                _delay_ms(MS_TRACK_STEP_1050);
            } else {
                _delay_ms(MS_TRACK_STEP_810);
            }
        }
        // delay for head settling
        if (is_1050()) {
            _delay_ms(MS_HEAD_SETTLE_1050);
        } else {
            _delay_ms(MS_HEAD_SETTLE_810);
        }
    }

    // set new head track position
    gCurrentHeadTrack = tgtTrackNumber;

    // sample current head position
    u16 headPosition = getCurrentHeadPosition();

    // read the track header
    u32 currentFileOffset = gTrackInfo[tgtTrackNumber - 1].offset;
    // exit, if track not present
    if (!currentFileOffset) {
	goto error;
    }
    faccess_offset(FILE_ACCESS_READ, currentFileOffset, sizeof(struct atxTrackHeader));
    trackHeader = (struct atxTrackHeader *) atari_sector_buffer;
#ifndef __AVR__
    byteSwapAtxTrackHeader(trackHeader);
#endif
    u16 sectorCount = trackHeader->sectorCount;

    // if there are no sectors in this track or the track number doesn't match, return error
    if (trackHeader->trackNumber == tgtTrackNumber - 1 && sectorCount) {
        // read the sector list header if there are sectors for this track
	currentFileOffset += trackHeader->headerSize;
	faccess_offset(FILE_ACCESS_READ, currentFileOffset, sizeof(struct atxSectorListHeader));
	slHeader = (struct atxSectorListHeader *) atari_sector_buffer;
#ifndef __AVR__
	byteSwapAtxSectorListHeader(slHeader);
#endif

	// sector list header is variable length, so skip any extra header bytes that may be present
	currentFileOffset += slHeader->next - sectorCount * sizeof(struct atxSectorHeader);

        int pTT = 0;
        int retries = MAX_RETRIES_810;

        // if we are still below the maximum number of retries that would be performed by the drive firmware...
        u32 retryOffset = currentFileOffset;
        while (retries > 0) {
            retries--;
            currentFileOffset = retryOffset;
            // iterate through all sector headers to find the target sector
            for (i=0; i < sectorCount; i++) {
                if (faccess_offset(FILE_ACCESS_READ, currentFileOffset, sizeof(struct atxSectorHeader))) {
                    sectorHeader = (struct atxSectorHeader *) atari_sector_buffer;
#ifndef __AVR__
                    byteSwapAtxSectorHeader(sectorHeader);
#endif
                    // if the sector is not flagged as missing and its number matches the one we're looking for...
                    if (!(sectorHeader->status & MASK_FDC_MISSING) && sectorHeader->number == tgtSectorNumber) {
                        // check if it's the next sector that the head would encounter angularly...
                        int tt = sectorHeader->timev - headPosition;
                        if (pTT == 0 || (tt > 0 && pTT < 0) || (tt > 0 && pTT > 0 && tt < pTT) || (tt < 0 && pTT < 0 && tt < pTT)) {
                            pTT = tt;
                            gLastAngle = sectorHeader->timev;
                            *status = sectorHeader->status;
                            // On an Atari 810, we have to do some specific behavior 
                            // when a long sector is encountered (the lost data bit 
                            // is set):
                            //   1. ATX images don't normally set the DRQ status bit
                            //      because the behavior is different on 810 vs.
                            //      1050 drives. In the case of the 810, the DRQ bit 
                            //      should be set.
                            //   2. The 810 is "blind" to CRC errors on long sectors
                            //      because it interrupts the FDC long before 
                            //      performing the CRC check.
                            if (*status & MASK_FDC_DLOST) {
                                *status |= 0x02;
                            }
                            // if the extended data flag is set, increment extended record count for later reading
                            if (*status & MASK_EXTENDED_DATA) {
                                extendedDataRecords++;
                            }
                            tgtSectorIndex = i;
                            tgtSectorOffset = sectorHeader->data;
                        }
                    }
                    currentFileOffset += sizeof(struct atxSectorHeader);
                }
            }
            // if the sector status is bad, delay for a full disk rotation
            if (*status) {
                waitForAngularPosition(incAngularDisplacement(getCurrentHeadPosition(), AU_FULL_ROTATION));
            // otherwise, no need to retry
            } else {
                retries = 0;
            }
        }

        // store the last angle returned for the debugging window
        last_angle_returned = gLastAngle;

        // if the status is bad, flag as error
        if (*status) {
            hasError = (BOOL) TRUE;
        }

        // if an extended data record exists for this track, iterate through all track chunks to search
        // for those records (note that we stop looking for chunks when we hit the 8-byte terminator; length == 0)
        if (extendedDataRecords > 0) {
            currentFileOffset = gTrackInfo[tgtTrackNumber - 1].offset + trackHeader->headerSize;
            do {
                faccess_offset(FILE_ACCESS_READ, currentFileOffset, sizeof(struct atxTrackChunk));
                extSectorData = (struct atxTrackChunk *) atari_sector_buffer;
#ifndef __AVR__
                byteSwapAtxTrackChunk(extSectorData);
#endif
                if (extSectorData->size > 0) {
                    // if the target sector has a weak data flag, grab the start weak offset within the sector data
                    if (extSectorData->sectorIndex == tgtSectorIndex && extSectorData->type == 0x10) {
                        weakOffset = extSectorData->data;
                    }
                    currentFileOffset += extSectorData->size;
                }
            } while (extSectorData->size > 0);
        }

        // read the data (re-using tgtSectorIndex variable here to reduce stack consumption)
        if (tgtSectorOffset) {
            tgtSectorIndex = (u16) faccess_offset(FILE_ACCESS_READ, gTrackInfo[tgtTrackNumber - 1].offset + tgtSectorOffset, gBytesPerSector);
        }
        if (hasError) {
            tgtSectorIndex = 0;
        }

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
        if (is_1050()) {
            _delay_ms(MS_CRC_CALCULATION_1050);
        } else {
            _delay_ms(MS_CRC_CALCULATION_810);
        }
    }

error:
    // the Atari expects an inverted FDC status byte
    *status = ~(*status);

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
