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

u08 loadAtxFile() {
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
        return 1;
    }

    // enhanced density is 26 sectors per track, single and double density are 18
    gSectorsPerTrack = (fileHeader->density == 1) ? (u08)26 : (u08)18;
    // single and enhanced density are 128 bytes per sector, double density is 256
    gBytesPerSector = (fileHeader->density == 1) ? (u08)256 : (u08)128;

    // calculate track offsets
    u32 startOffset = fileHeader->startData;
    while (1) {
        if (!faccess_offset(FILE_ACCESS_READ, startOffset, sizeof(struct atxTrackHeader))) {
            break;
        }
        trackHeader = (struct atxTrackHeader*)atari_sector_buffer;
        gTrackInfo[trackHeader->trackNumber - 1].offset = startOffset;
        startOffset += trackHeader->size;
    }

    return 0;
}

u16 loadAtxSector(u16 num) {
    struct atxTrackHeader *trackHeader;
    struct atxSectorListHeader *slHeader;
    struct atxSectorHeader *sectorHeader;

    // calculate track and relative sector number from the absolute sector number
    int tgtTrackNumber = (num - 1) / gSectorsPerTrack + 1;
    int tgtSectorNumber = (num - 1) % gSectorsPerTrack + 1;

    // read the track header
    u32 offset = gTrackInfo[tgtTrackNumber - 1].offset;
    faccess_offset(FILE_ACCESS_READ, offset, sizeof(struct atxTrackHeader));
    trackHeader = (struct atxTrackHeader*)atari_sector_buffer;
    u16 sectorCount = trackHeader->sectorCount;

    // read the sector list header
    offset += trackHeader->headerSize;
    faccess_offset(FILE_ACCESS_READ, offset, sizeof(struct atxSectorListHeader));
    slHeader = (struct atxSectorListHeader*)atari_sector_buffer;

    // sector list header is variable length, so skip any extra header bytes that may be present
    offset += slHeader->next - sectorCount * sizeof(struct atxSectorHeader);

    // iterate through all sector headers to find the requested one
    for (int i=0; i < sectorCount; i++) {
        if (faccess_offset(FILE_ACCESS_READ, offset, sizeof(struct atxSectorHeader))) {
            sectorHeader = (struct atxSectorHeader*)atari_sector_buffer;

            // TODO: for duplicate sectors, the sector that we return should be based on timing and not the first one that is found
            if (sectorHeader->number == tgtSectorNumber) {
                // TODO: how do we pass sector status back?

                // read the sector data
                return faccess_offset(FILE_ACCESS_READ, gTrackInfo[tgtTrackNumber - 1].offset + sectorHeader->data, gBytesPerSector);
            }
            offset += sizeof(struct atxSectorHeader);
        }
    }

    return 0;
}

