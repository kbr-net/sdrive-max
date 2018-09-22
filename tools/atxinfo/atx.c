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
    ulong offset;   // absolute position within file for start of track header
    uchar xsCount;  // number of extended sector records
    ulong xsOffset; // absolute position within file for start of extended sector records
};

FILE *gFile;                            // reference to currently loaded ATX file
ushort gBytesPerSector;                 // number of bytes per sector
uchar gSectorsPerTrack;                 // number of sectors in each track
struct atxTrackInfo gTrackInfo[40];     // pre-calculated info for each track

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
    ulong startOffset = fileHeader.startData;
    while (1) {
        if (!fread(&trackHeader, sizeof(trackHeader), 1, gFile)) {
            break;
        }
        gTrackInfo[trackHeader.trackNumber].offset = startOffset;
        fseek(gFile, trackHeader.size - sizeof(trackHeader), SEEK_CUR);
        startOffset += trackHeader.size;
    }

    return 0;
}

uchar getAtxTrack(uchar num) {
    struct atxTrackHeader trackHeader;
    struct atxSectorListHeader slHeader;
    struct atxSectorHeader sectorHeader;
    uchar ret = NULL;

    // calculate track and relative sector number from the absolute sector number
    uchar tgtTrackNumber = num;
    ushort absSectorNumber = (tgtTrackNumber - 1) * gSectorsPerTrack;

    if (tgtTrackNumber > 40)
	return ret;

    // read the track header
    fseek(gFile, gTrackInfo[tgtTrackNumber - 1].offset, SEEK_SET);
    fread(&trackHeader, sizeof(trackHeader), 1, gFile);

    // read the sector list header
    fseek(gFile, trackHeader.headerSize - sizeof(trackHeader), SEEK_CUR);
    fread(&slHeader, sizeof(slHeader), 1, gFile);
    uchar sectorCount = trackHeader.sectorCount;

    // sector list header is variable length, so skip any extra header bytes that may be present
    fseek(gFile, slHeader.next - trackHeader.sectorCount * 8 - sizeof(slHeader), SEEK_CUR);

    // indicator that extended sector data record exists for the sector we are reading
    uchar xsdFound = 0;

    // find the sector header
    printf("=============================================\n");
    printf("Track: %i count: %i\n", trackHeader.trackNumber, sectorCount);
    printf("=============================================\n");
    uchar sectors[26];
    bzero(&sectors, sizeof(sectors));
    ushort i;
    for (i=0; i < trackHeader.sectorCount; i++) {
        if (! fread(&sectorHeader, sizeof(sectorHeader), 1, gFile))
		break;

	//printf("Sec: %i T: %i S: %i St: 0x%02x t: %i i: %i\n", absSectorNumber + sectorHeader.number, tgtTrackNumber, sectorHeader.number, sectorHeader.status, sectorHeader.timev, i);
	if (!sectorHeader.status)
	    printf("i: %2i T: %02i S: %2i St: 0x%02x t: %5i Sec: %i\n", i, tgtTrackNumber, sectorHeader.number, sectorHeader.status, sectorHeader.timev, absSectorNumber + sectorHeader.number);
	else
	    printf("i: %2i T: %02i S: %2i St: \x1b[31;1m0x%02x\x1b[0m t: %5i Sec: %i\n", i, tgtTrackNumber, sectorHeader.number, sectorHeader.status, sectorHeader.timev, absSectorNumber + sectorHeader.number);

	sectors[sectorHeader.number]++;
	// if sector has extended data
	if ((sectorHeader.status & STS_EXTENDED) > 0) {
	    xsdFound = 1;
	    //TODO
	}
    }

    for (i = 0; i < sectorCount; i++) {
	if (sectors[i] > 1)
	    printf("has doubles: %i: %i\n", i, sectors[i]);
    }
    return ret;
}

void closeAtxFile() {
    gFile = NULL;
    memset(&gTrackInfo, 0, 40*sizeof(struct atxTrackInfo));
}
