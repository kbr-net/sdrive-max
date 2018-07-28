/*! \file atx.h \brief ATX file handling. */
//*****************************************************************************
//
// File Name	: 'atx.h'
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

#ifndef ATX_TEST_ATX_H
#define ATX_TEST_ATX_H

#include <stdio.h>
#include <stdint.h>

typedef uint8_t uchar;
typedef uint16_t ushort;
typedef uint32_t u32;

struct atxDiskSector {
    uchar number;   // the sector number
    uchar status;   // the FDC status
    u32 seekTime; // the time (in microseconds) it took to read the data
    size_t length;  // the length of the sector data
    uchar *data;    // pointer to the sector data
};

#define ATX_VERSION		0x01
#define STS_EXTENDED	0x40

struct atxFileHeader {
    uchar signature[4];
    ushort version;
    ushort minVersion;
    ushort creator;
    ushort creatorVersion;
    u32 flags;
    ushort imageType;
    uchar density;
    uchar reserved0;
    u32 imageId;
    ushort imageVersion;
    ushort reserved1;
    u32 startData;
    u32 endData;
    uchar reserved2[12];
};

struct atxTrackHeader {
    u32 size;
    ushort type;
    ushort reserved0;
    uchar trackNumber;
    uchar reserved1;
    ushort sectorCount;
    ushort rate;
    ushort reserved3;
    u32 flags;
    u32 headerSize;
    uchar reserved4[8];
};

struct atxSectorListHeader {
    u32 next;
    ushort type;
    ushort pad0;
};

struct atxSectorHeader {
    uchar number;
    uchar status;
    ushort timev;
    u32 data;
};

struct atxExtendedSectorData {
    u32 size;
    uchar type;
    uchar sectorNumber;
    ushort data;
};

/***************************************************************/
/***************************************************************/

// load an ATX file (returns 0 if ATX file is successfully loaded; 1 if not)
int loadAtxFile(FILE *file);

// get data for a specific disk sector
// NOTE: The caller is responsible for calling free on both the returned pointer and its internal sector data pointer
uchar getAtxTrack(uchar num);

// dispose of the currently loaded ATX file
void closeAtxFile();

#endif //ATX_TEST_ATX_H
