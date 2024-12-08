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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "atx.h"

struct atxTrackInfo {
    u32 offset;   // absolute position within file for start of track header
    uchar xsCount;  // number of extended sector records
    u32 xsOffset; // absolute position within file for start of extended sector records
};

#define MAX_TRACK 80

FILE *gFile;                            // reference to currently loaded ATX file
ushort gBytesPerSector;                 // number of bytes per sector
uchar gSectorsPerTrack;                 // number of sectors in each track
struct atxTrackInfo gTrackInfo[MAX_TRACK];     // pre-calculated info for each track

int loadAtxFile(FILE *f) {
    struct atxFileHeader fileHeader;
    struct atxTrackHeader trackHeader;

    closeAtxFile();

    // read the file header
    fseek(f, 0, SEEK_SET);
    fread(&fileHeader, sizeof(fileHeader), 1, f);

    // validate the ATX file header
    if (fileHeader.signature[0] != 'A' ||
        fileHeader.signature[1] != 'T' ||
        fileHeader.signature[2] != '8' ||
        fileHeader.signature[3] != 'X' ||
        fileHeader.version != ATX_VERSION ||
        fileHeader.minVersion != ATX_VERSION) {
        return 1;
    }

    // save the file descriptor
    gFile = f;

    // enhanced density is 26 sectors per track, single and double density are 18
    gSectorsPerTrack = (fileHeader.density == 1) ? (uchar)26 : (uchar)18;
    // single and enhanced density are 128 bytes per sector, double density is 256
    gBytesPerSector = (fileHeader.density == 1) ? (ushort)256 : (ushort)128;

    // seek to first track header
    fseek(gFile, fileHeader.startData, SEEK_SET);

    // calculate track offsets
    u32 startOffset = fileHeader.startData;
    uchar track;
    for (track = 0; track < MAX_TRACK; track++) {
        if (!fread(&trackHeader, sizeof(trackHeader), 1, gFile)) {
            break;
        }
        gTrackInfo[track].offset = startOffset;
        fseek(gFile, trackHeader.size - sizeof(trackHeader), SEEK_CUR);
        startOffset += trackHeader.size;
    }

    return 0;
}

uchar getAtxTrack(uchar num) {
    struct atxTrackHeader trackHeader;
    struct atxSectorListHeader slHeader;
    uchar ret = 0;

    // calculate track and relative sector number from the absolute sector number
    uchar tgtTrackNumber = num;
    ushort absSectorNumber = (tgtTrackNumber - 1) * gSectorsPerTrack;

    // track present?
    if (!gTrackInfo[tgtTrackNumber - 1].offset)
	return ret;

    ret = 1;

    // read the track header
    fseek(gFile, gTrackInfo[tgtTrackNumber - 1].offset, SEEK_SET);
    fread(&trackHeader, sizeof(trackHeader), 1, gFile);
    ushort sectorCount = trackHeader.sectorCount;

    // read the sector list header
    // this seek does nothing, size - size = 0
    //fseek(gFile, trackHeader.headerSize - sizeof(trackHeader), SEEK_CUR);
    fread(&slHeader, sizeof(slHeader), 1, gFile);

    // sector list header is variable length, so skip any extra header bytes that may be present
    fseek(gFile, slHeader.next - sectorCount * 8 - sizeof(slHeader), SEEK_CUR);

    // indicator that extended sector data record exists for the sector we are reading
    uchar xsdFound = 0;

    // find the sector header
    printf("===========================================================\n");
    printf("Track: %i count: %i rate: %i flags: %04x size: %u\n", trackHeader.trackNumber, sectorCount, trackHeader.rate, trackHeader.flags, trackHeader.size);
    printf("===========================================================\n");
    uchar sectors[26];
    bzero(&sectors, sizeof(sectors));

    // read all sector headers
    struct atxSectorHeader sectorHeader[sectorCount];
    if (! fread(sectorHeader, sizeof(sectorHeader[0]) * sectorCount, 1, gFile))
	return(ret);

    ushort i;
    for (i=0; i < sectorCount; i++) {
	// if sector has extended data
	if ((sectorHeader[i].status & STS_EXTENDED) > 0) {
	    xsdFound++;
	    //TODO
	}
	if (!sectorHeader[i].status)
	    printf("i: %2i T: %02i S: %2i St: 0x%02x t: %5i o: %4u s: %3li Sec: %i\n",
		i,
		tgtTrackNumber,
		sectorHeader[i].number,
		sectorHeader[i].status,
		sectorHeader[i].timev,
		sectorHeader[i].data,
		(i == sectorCount-1 ? trackHeader.size - sectorHeader[i].data - sizeof(struct atxExtendedSectorData) * xsdFound - 8 : sectorHeader[i+1].data - sectorHeader[i].data),
		absSectorNumber + sectorHeader[i].number);
	else
	    printf("i: %2i T: %02i S: %2i St: \x1b[31;1m0x%02x\x1b[0m t: %5i o: %4u s: %3li Sec: %i\n",
		i,
		tgtTrackNumber,
		sectorHeader[i].number,
		sectorHeader[i].status,
		sectorHeader[i].timev,
		sectorHeader[i].data,
		(i == sectorCount-1 ? trackHeader.size - sectorHeader[i].data - sizeof(struct atxExtendedSectorData) * xsdFound - 8 : sectorHeader[i+1].data - sectorHeader[i].data),
		absSectorNumber + sectorHeader[i].number);

	sectors[sectorHeader[i].number]++;
    }

    for (i = 0; i < sectorCount; i++) {
	if (sectors[i] > 1)
	    printf("has doubles: %i: %i\n", i, sectors[i]);
    }
    if (xsdFound) {
	printf("has extended: %i\n", xsdFound);
    }
    return ret;
}

void closeAtxFile() {
    gFile = NULL;
    memset(&gTrackInfo, 0, 40*sizeof(struct atxTrackInfo));
}
