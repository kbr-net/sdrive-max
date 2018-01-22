/*! \file fat.c \brief FAT16/32 file system driver. */
//*****************************************************************************
//
// File Name	: 'fat.c'
// Title		: FAT16/32 file system driver
// Author		: Pascal Stang
// Date			: 11/07/2000
// Revised		: 12/12/2000
// Version		: 0.3
// Target MCU	: ATmega103 (should work for Atmel AVR Series)
// Editor Tabs	: 4
//
// This code is based in part on work done by Jesper Hansen for his
//		YAMPP MP3 player project.
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
// 02.05.2014
// enhanced by kbr
// 24.10.2015
// added create file support
//
// This code is distributed under the GNU Public License
//		which can be found at http://www.gnu.org/licenses/gpl.txt
//
//*****************************************************************************

#include <stdio.h>
//#include <avr/io.h>
#include <avr/pgmspace.h>
#include <string.h>

#include "avrlibdefs.h"         // global AVRLIB defines
#include "avrlibtypes.h"        // global AVRLIB types definitions

#include "fat.h"
#include "mmc.h"

//void USART_SendString(char *buff);

#define FileNameBuffer atari_sector_buffer
extern unsigned char atari_sector_buffer[256];
extern unsigned char mmc_sector_buffer[512];	// one sector
extern struct GlobalSystemValues GS;
extern struct FileInfoStruct FileInfo;			//< file information for last file accessed
extern struct flags SDFlags;

u32 fatClustToSect(u32 clust)
{
	u32 ret;
	if(clust==MSDOSFSROOT) {
		return (u32)((u32)(FirstDataSector) - (u32)(RootDirSectors));
	}	
	ret = (u32)(clust-2);
	ret *= (u32)SectorsPerCluster;
	ret += (u32)FirstDataSector;
	return ((u32)ret);
}

u32 last_dir_start_cluster;
unsigned char last_dir_valid;
unsigned short last_dir_entry=0x0;
u32 last_dir_sector=0;
unsigned char last_dir_sector_count=0;
u32 last_dir_cluster=0;
unsigned char last_dir_index=0;

//u32 test;

unsigned char fatInit()
{
	struct partsector *ps;
	struct partrecord PartInfo;
	struct bootsector710 *bs;
	struct bpb710 *bpb;

	// read partition table
	if(mmcReadCached(0)) return(1);	// read error
	// map first partition record	
	// save partition information to global PartInfo
	//PartInfo = *((struct partrecord *) ((struct partsector *) (char*)mmc_sector_buffer)->psPart);
	//break strict-aliasing!
	ps = (struct partsector *) mmc_sector_buffer;
	PartInfo = ps->psPart[0];

	// Read the Partition BootSector
	// **first sector of partition in PartInfo.prStartLBA
	if (mmcReadCached(PartInfo.prStartLBA)) return(2);
	//bpb = (struct bpb710 *) ((struct bootsector710 *) (char*)mmc_sector_buffer)->bsBPB;
	//break strict-aliasing!
	bs = (struct bootsector710 *) mmc_sector_buffer;
	bpb = &bs->bsBPB;

	RootDirSectors = bpb->bpbRootDirEnts>>4; // /16 ; 512/16 = 32  (sector size / max entries in one sector = number of sectors)

	// setup global disk constants
	FirstDataSector	= PartInfo.prStartLBA;

	if(bpb->bpbFATsecs) {
		// bpbFATsecs is non-zero and is therefore valid
		//FirstDataSector     += bpb->bpbResSectors + bpb->bpbFATs * bpb->bpbFATsecs + RootDirSectors;
		FATSectors = bpb->bpbFATsecs;
	}
	else {
		// bpbFATsecs is zero, real value is in bpbBigFATsecs
		//FirstDataSector     += bpb->bpbResSectors + bpb->bpbFATs * bpb->bpbBigFATsecs + RootDirSectors;
		FATSectors = bpb->bpbBigFATsecs;
	}
	FirstDataSector	+= bpb->bpbResSectors + bpb->bpbFATs * FATSectors + RootDirSectors;

	if(bpb->bpbSectors)
		SectorsTotal = bpb->bpbSectors - FATSectors*bpb->bpbFATs - bpb->bpbResSectors - RootDirSectors;
	else
		SectorsTotal = bpb->bpbHugeSectors - FATSectors*bpb->bpbFATs - bpb->bpbResSectors - RootDirSectors;
	//printf("SectorsTotal: %i\n", SectorsTotal);
	SectorsPerCluster	= bpb->bpbSecPerClust;
	BytesPerSector		= bpb->bpbBytesPerSec;
	FirstFATSector		= bpb->bpbResSectors + PartInfo.prStartLBA;
	//FirstFAT2Sector		= bpb->bpbResSectors + PartInfo.prStartLBA + bpb->bpbFATsecs; // bo ao

	//last_dir_start_cluster=0xffff;
	last_dir_start_cluster=CLUST_EOFE;
	//last_dir_valid=0;		//<-- neni potreba, protoze na zacatku fatGetDirEntry se pri zjisteni
							//ze je (FileInfo.vDisk->dir_cluster!=last_dir_start_cluster)
							//vynuluje last_dir_valid=0;

	switch (PartInfo.prPartType) {
	case PART_TYPE_DOSFAT16:
	case PART_TYPE_FAT16:
	case PART_TYPE_FAT16LBA:
		// first directory cluster is 2 by default (clusters range 2->big)

		//FirstDirSector    = CLUST_FIRST;  // rem by ao

		//FirstDirSector = MSDOSFSROOT;  // by ao
		FileInfo.vDisk->dir_cluster = MSDOSFSROOT; //RootDirStartCluster;
		//FileInfo.vDisk->dir_cluster = (FirstDataSector - RootDirSectors)/SectorsPerCluster; //RootDirStartCluster;

		// push data sector pointer to end of root directory area
		//FirstDataSector += (bpb->bpbRootDirEnts)/DIRENTRIES_PER_SECTOR;
		SDFlags.Fat32Enabled = FALSE;
		//printf("fatinit: FAT16\n");
		break;
	case PART_TYPE_FAT32LBA:
	case PART_TYPE_FAT32:
		// bpbRootClust field exists in FAT32 bpb710, but not in lesser bpb's
		//FirstDirSector = bpb->bpbRootClust;
		FileInfo.vDisk->dir_cluster = bpb->bpbRootClust; //RootDirStartCluster;
		// push data sector pointer to end of root directory area
		// need this? FirstDataSector += (bpb->bpbRootDirEnts)/DIRENTRIES_PER_SECTOR;
		SDFlags.Fat32Enabled = TRUE;
		//printf("fatinit: FAT32\n");
		break;
	default:
		//printf("INV Type %x\n", PartInfo.prPartType);
		return 3; //partition not supported
		break;
	}

	RootDirCluster = FileInfo.vDisk->dir_cluster;	// save for cd /
	return 0;
}

//////////////////////////////////////////////////////////////

unsigned char fatGetDirEntry(unsigned short entry, unsigned char use_long_names)
{
	u32 sector;
	struct direntry *de = 0;	// avoid compiler warning by initializing
	struct winentry *we;
	unsigned char haveLongNameEntry;
	unsigned char gotEntry;
	unsigned short b;
	u08 index;
	unsigned short entrycount = 0;
	u32 actual_cluster = FileInfo.vDisk->dir_cluster;
	unsigned char seccount=0;

	haveLongNameEntry = 0;
	gotEntry = 0;

	if (FileInfo.vDisk->dir_cluster!=last_dir_start_cluster)
	{
		//zmenil se adresar, takze musi pracovat s nim
		//a zneplatnit last_dir polozky
		last_dir_start_cluster=FileInfo.vDisk->dir_cluster;
		last_dir_valid=0;
	}

	if ( !last_dir_valid
		 || (entry<=last_dir_entry && (entry!=last_dir_entry || last_dir_valid!=1 || use_long_names!=0))
	   )
	{
		//musi zacit od zacatku
		sector = fatClustToSect(FileInfo.vDisk->dir_cluster);
		//index = 0;
		goto fat_read_from_begin;
	}
	else
	{
		//muze zacit od posledne pouzite
		entrycount=last_dir_entry;
		index = last_dir_index;
		sector=last_dir_sector;
		actual_cluster=last_dir_cluster;
		seccount = last_dir_sector_count;
		goto fat_read_from_last_entry;
	}

	while(1)
	{
		if(index==16)	// time for next sector ?
		{
			if ( actual_cluster==MSDOSFSROOT )
			{
				//v MSDOSFSROOT se nerozlisuji clustery
				//protoze MSDOSFSROOT ma sektory stale dal za sebou bez clusterovani
				if (seccount>=RootDirSectors && !SDFlags.Fat32Enabled) return 0; //prekrocil maximalni pocet polozek v MSDOSFSROOT
			}
			else //MUSI BYT!!! (aby neporovnaval seccount je-li actual_cluster==MSDOSFSROOT)
			if( seccount>=SectorsPerCluster )
			{
				//next cluster
				//pri prechodu pres pocet sektoru v clusteru
				actual_cluster = fatNextCluster(actual_cluster);
				sector=fatClustToSect(actual_cluster);
				seccount=0;
			}
fat_read_from_begin:
			index = 0;
			seccount++;		//ted bude nacitat dalsi sektor (musi ho zapocitat)
fat_read_from_last_entry:
			mmcReadCached( sector++ ); 	//prave ho nacetl
			de = (struct direntry *) (char*)mmc_sector_buffer;
			de+=index;
		}
		
		// check the status of this directory entry slot
		if(de->deName[0] == SLOT_EMPTY)		//0x00
		{
			// slot is empty and this is the end of directory
			gotEntry = 0;
			break;
		}
		else if(de->deName[0] == SLOT_DELETED)	//0xE5
		{
			// this is an empty slot
			// do nothing and move to the next one
		}
		else
		{
			// this is a valid and occupied entry
			// is it a part of a long file/dir name?
			if(de->deAttributes==ATTR_LONG_FILENAME)
			{
				// we have a long name entry
				// cast this directory entry as a "windows" (LFN: LongFileName) entry
				u08 i;
				unsigned char *fnbPtr;

				//pokud nechceme longname, NESMI je vubec kompletovat
				//a preskoci na dalsi polozku
				//( takze ani haveLongNameEntry nikdy nebude nastaveno na 1)
				//Jinak by totiz preplacaval dalsi kusy atari_sector_bufferu 12-255 ! BACHA!!!
				if (!use_long_names) goto fat_next_dir_entry;

				we = (struct winentry *) de;
				
				b = WIN_ENTRY_CHARS*( (unsigned short)((we->weCnt-1) & 0x0f));		// index into string
				fnbPtr = &FileNameBuffer[b];

				for (i=0;i<5;i++)	*fnbPtr++ = we->wePart1[i*2];	// copy first part
				for (i=0;i<6;i++)	*fnbPtr++ = we->wePart2[i*2];	// second part
				for (i=0;i<2;i++)	*fnbPtr++ = we->wePart3[i*2];	// and third part

				/*
				{
				//unsigned char * sptr;
				//sptr=we->wePart1;
				//do { *fnbPtr++ = *sptr++; sptr++; } while (sptr<(we->wePart1+10)); // copy first part
				//^-- pouziti jen tohoto prvniho a druhe dva pres normalni for(..) uspori 10 bajtu,
				//pri pouziti i dalsich dvou timto stylem uz se to ale zase prodlouzi - nechaaaaaapu!
				//sptr=we->wePart2;
				//do { *fnbPtr++ = *sptr++; sptr++; } while (sptr<(we->wePart2+12)); // second part
				//sptr=we->wePart3;
				//do { *fnbPtr++ = *sptr++; sptr++; } while (sptr<(we->wePart3+4));  // and third part
				}
				*/

				if (we->weCnt & WIN_LAST) *fnbPtr = 0;				// in case dirnamelength is multiple of 13, add termination
				if ((we->weCnt & 0x0f) == 1) haveLongNameEntry = 1;	// flag that we have a complete long name entry set
			}
			else
			{
				// we have a short name entry
				
				// check if this is the short name entry corresponding
				// to the end of a multi-part long name entry
				if(haveLongNameEntry)
				{
					// a long entry name has been collected
					if(entrycount == entry)		
					{
						// desired entry has been found, break out
						gotEntry = 2;
						break;
					}
					// otherwise
					haveLongNameEntry = 0;	// clear long name flag
					entrycount++;			// increment entry counter		
				}
				else
				{
					// entry is a short name (8.3 format) without a
					// corresponding multi-part long name entry

					//Zcela vynecha adresar "." a disk label
					if((de->deName[0]=='.' && de->deName[1]==' ' && (de->deAttributes & ATTR_DIRECTORY)) //je to "." adresar
						|| (de->deAttributes & ATTR_VOLUME) ) goto fat_next_dir_entry;

					if(entrycount == entry)
					{
						/*
						fnbPtr = &FileNameBuffer[string_offset];
						for (i=0;i<8;i++)	*fnbPtr++ = de->deName[i];	// copy name
						for (i=0;i<3;i++)	*fnbPtr++ = de->deExtension[i];	// copy extension
						*fnbPtr = 0;										// null-terminate
						*/
						u08 i;
						unsigned char *dptr;

						dptr = FileNameBuffer;
						for (i=0;i<11;i++)	*dptr++ = de->deName[i];	// copy name+ext
						*dptr=0;	//ukonceni za nazvem

						// desired entry has been found, break out
						//pokud chtel longname, tak to neni a proto upravi 8+3 jmeno na nazev.ext
						//aby to melo formatovani jako longname
						if (use_long_names)
						{
							//upravi 'NAME    EXT' na 'NAME.EXT'
							//(vyhazi mezery a prida tecku)
							//krome nazvu '.          ', a '..         ', tam jen vyhaze mezery
							unsigned char *sptr;
							//EXT => .EXT   (posune vcetne 0x00 za koncem extenderu)
							dptr=(FileNameBuffer+12);
							i=4; do { sptr=dptr-1; *dptr=*sptr; dptr=sptr; i--; } while(i>0);
							if (FileNameBuffer[0]!='.') *dptr='.'; //jen pro jine nez '.' a '..'
							//NAME    .EXT => NAME.EXT
							sptr=dptr=FileNameBuffer;
							do
							{
							 if ((*sptr)!=' ') *dptr++=*sptr;
							 sptr++;
							} while (*sptr);
							*dptr=0; //ukonceni
						}
						//
						gotEntry = 1;
						break;
					}
					// otherwise
					entrycount++;			// increment entry counter		
				}
			}
		}
fat_next_dir_entry:
		// next directory entry
		de++;
		// next index
		index++;
	}
	
	// we have a file/dir to return
	// store file/dir starting cluster (start of data)
	FileInfo.vDisk->start_cluster = ((u32) de->deHighClust<<16 | de->deStartCluster);
	// fileindex
	FileInfo.vDisk->file_index = entry;	//fileindex teto polozky
	// store file/dir size (note: size field for subdirectory entries is always zero)
	FileInfo.vDisk->size = de->deFileSize;
	// store file/dir attributes
	FileInfo.Attr = de->deAttributes;
	// store file/dir last update time
	FileInfo.Time = (unsigned short) (de->deMTime[0] | de->deMTime[1]<<8);
	// store file/dir last update date
	FileInfo.Date = (unsigned short) (de->deMDate[0] | de->deMDate[1]<<8);

	if(gotEntry)
	{
		last_dir_entry = entrycount;
		last_dir_index = index;
		last_dir_sector = sector-1; //protoze ihned po nacteni se posune
		last_dir_cluster = actual_cluster;
		last_dir_sector_count = seccount; //skace se az za inkrementaci seccount, takze tady se 1 neodecita!
		last_dir_valid=gotEntry;
	}

	return gotEntry;
}


u32 fatNextCluster(u32 cluster)
{
	u32 nextCluster;
	u32 fatMask;
	u32 fatOffset;
	u32 sector;
	u32 offset;

	// get fat offset in bytes
	if(SDFlags.Fat32Enabled) {
		// four FAT bytes (32 bits) for every cluster
		fatOffset = cluster << 2;
		// set the FAT bit mask
		fatMask = FAT32_MASK;
	}
	else {
		// two FAT bytes (16 bits) for every cluster
		fatOffset = cluster << 1;
		// set the FAT bit mask
		fatMask = FAT16_MASK;
	}

	// calculate the FAT sector that we're interested in
	sector = FirstFATSector + (fatOffset / ((u32)BytesPerSector));
	// calculate offset of the our entry within that FAT sector
	offset = fatOffset % ((u32)BytesPerSector);

	// read sector of FAT table
	mmcReadCached( sector );

	// read the nextCluster value
	nextCluster = (*((u32*) &((char*)mmc_sector_buffer)[offset])) & fatMask;

	// check to see if we're at the end of the chain
	//if (nextCluster == (CLUST_EOFE & fatMask))
	if ((nextCluster & CLUST_EOFS) == (CLUST_EOFE & fatMask))	//fixed range from ...f8-ff
		nextCluster = 0;

	//return (nextCluster&0xFFFF);
	return (nextCluster);
}

u32 getClusterN(u32 ncluster)
{
        if(ncluster<FileInfo.vDisk->ncluster)
        {
                FileInfo.vDisk->current_cluster=FileInfo.vDisk->start_cluster;
                FileInfo.vDisk->ncluster=0;
        }

        while(FileInfo.vDisk->ncluster!=ncluster)
        {
                FileInfo.vDisk->current_cluster=fatNextCluster(FileInfo.vDisk->current_cluster);
                FileInfo.vDisk->ncluster++;
        }

        //return (FileInfo.vDisk->current_cluster&0xFFFF);
        return (FileInfo.vDisk->current_cluster);
}

unsigned short faccess_offset(char mode, u32 offset_start, unsigned short ncount)
{
        unsigned short j;
        u32 offset=offset_start;
        u32 end_of_file;
        u32 ncluster, nsector, current_sector;
        u32 bytespercluster=((u32)SectorsPerCluster)*((u32)BytesPerSector);

        if(offset_start>=FileInfo.vDisk->size)
                return 0;

        end_of_file = (FileInfo.vDisk->size - offset_start);

        ncluster = offset/bytespercluster;
        offset-=ncluster*bytespercluster;

        nsector = offset/((u32)BytesPerSector);
        offset-=nsector*((u32)BytesPerSector);

        getClusterN(ncluster);

        current_sector = fatClustToSect(FileInfo.vDisk->current_cluster) + nsector;
        mmcReadCached(current_sector);

        for(j=0;j<ncount;j++,offset++)
        {
                        if(offset>=((u32)BytesPerSector) )
                        {
                                if(mode==FILE_ACCESS_WRITE && mmcWriteCached(0))
                                        return 0;

                                offset=0;
                                nsector++;

                                if(nsector>=((u32)SectorsPerCluster) )
                                {
                                        nsector=0;
                                        ncluster++;
                                        getClusterN(ncluster);
                                }

                                current_sector = fatClustToSect(FileInfo.vDisk->current_cluster) + nsector;
                                mmcReadCached(current_sector);
                        }

                if(!end_of_file)
                        break; //return (j&0xFFFF);

                if(mode==FILE_ACCESS_WRITE)
                        mmc_sector_buffer[offset]=atari_sector_buffer[j]; //SDsektor<-atarisektor
                else
                        atari_sector_buffer[j]=mmc_sector_buffer[offset]; //atarisektor<-SDsektor

                end_of_file--;

        }

        if(mode==FILE_ACCESS_WRITE && mmcWriteCached(0))
                return 0;

        return (j&0xFFFF);
}

// return: 0 ok, 1 error
unsigned short fatFindFreeAllocUnit( u32 * clusterNo, u32 * sect )
{
	u32 fatSect;
	u32 clust;

	unsigned short i = 0;

	u32 clst = (*clusterNo) + 1;  // next

	unsigned short fatEntriesPerSector;
	unsigned short fatSize;
	u32 fatMask;
	if(SDFlags.Fat32Enabled) {
		// four FAT bytes (32 bits) for every cluster
		fatSize = 4;
		fatEntriesPerSector = BytesPerSector / fatSize;
		// set the FAT bit mask
		fatMask = FAT32_MASK;
	}
	else {
		// two FAT bytes (16 bits) for every cluster
		fatSize = 2;
		fatEntriesPerSector = BytesPerSector / fatSize;
		// set the FAT bit mask
		fatMask = FAT16_MASK;
	}

	//round to the containing fat cluster
	clust = clst / fatEntriesPerSector * fatEntriesPerSector;
	fatSect = FirstFATSector;
	fatSect += clst / fatEntriesPerSector;

	for( ; fatSect < (FirstFATSector + FATSectors); ++fatSect ) {
		//printf("clust: %x, fatSect: %x, clst: %x\n", clust, fatSect, clst);

		mmcReadCached(fatSect);

		for( i = clst % fatEntriesPerSector; i < fatEntriesPerSector; ++i ) {
			//printf("i: %i, rest: %i ", i, ((SectorsTotal / SectorsPerCluster) % fatEntriesPerSector) );
			if(fatSect == (FirstFATSector + FATSectors -1) )
				if(i > ((SectorsTotal / SectorsPerCluster) % fatEntriesPerSector + 1) )
					break;
			//if( *((unsigned short*) &mmc_sector_buffer[i*2]) == 0x0000 ) {
			//read always 32bits, fatMask drops the rest for fat16
			//printf("%lx: %lx ",clust +i , (*((u32*) &mmc_sector_buffer[i*fatSize]) & fatMask));
			if( (*((u32*) &mmc_sector_buffer[i*fatSize]) & fatMask) == 0 ) {
				*clusterNo = clust + i;
				//printf("\nnext free cluster: %x\n", *clusterNo);
				*sect = fatSect;
				return 0;
			}
		}

		//printf("\n");
		clst = 0;	// begin with first cluster on next sector
		clust += fatEntriesPerSector;
	}

	return 1;	// no free cluster found
}


// call after GetDirEntry

u32 fatFileCreate (char *fileName, u32 fileSize) {

	u32 fatSect,firstSec;
	unsigned short fatSize;
	u32 fatMask;
	u32 dirSectNo = last_dir_sector;

	unsigned short entryNo = (last_dir_index + 1) % 16;

	//if (entryNo == 0) last_dir_sector++;	//sector full?
	if (entryNo == 0) dirSectNo++;		//sector full?
						//if this was the last free dir sector,
						//we have a problem here!

	if(SDFlags.Fat32Enabled) {
		// four FAT bytes (32 bits) for every cluster
		fatSize = 4;
		// set the FAT bit mask
		fatMask = FAT32_MASK;
	}
	else {
		// two FAT bytes (16 bits) for every cluster
		fatSize = 2;
		// set the FAT bit mask
		fatMask = FAT16_MASK;
	}

	struct direntry * de = (struct direntry*) mmc_sector_buffer;
	u32 clusterNo = 0;

	//printf("newentry: %i\n", entryNo);

	//find a free cluster
	if (fatFindFreeAllocUnit( &clusterNo, &fatSect )) return(0);

	//record cluster as start cluster
	mmcReadCached(dirSectNo);	//reread dir sector
	de[entryNo].deHighClust = clusterNo >> 16;
	de[entryNo].deStartCluster = clusterNo & 0xFFFF;
	strncpy(de[entryNo].deName,fileName,8);
	strncpy(de[entryNo].deExtension,"ATR",3);
	de[entryNo].deFileSize = fileSize;
	de[entryNo].deAttributes = 0x20;	//normal file, archive bit

	//mmcWrite(dirSectNo);			//direct use of mmcWrite doesn't check write protect!
						// and we can use mmcWriteCached also, because it's the same sector
	if (mmcWriteCached(0)) return(0);	//save new entry, exit on write error

	firstSec = fatClustToSect(clusterNo);	//remember first file sector to return

	// update FAT
	char full = 0;
	unsigned short cl;
	u32 prevFatSect;
	u32 prevClust;
	u16 NrOfClusters = fileSize/BytesPerSector/SectorsPerCluster;

	//if filesize exactly fits into last cluster, we dont't need one more
	if(fileSize % (BytesPerSector * SectorsPerCluster) == 0)
		NrOfClusters--;

	fatSect = FirstFATSector;
	fatSect += clusterNo / (BytesPerSector / fatSize);

	for(cl = 0; cl <= (NrOfClusters); ++cl ) {
		//printf("cl: %i\n", cl);

		prevClust = clusterNo % (BytesPerSector / fatSize);
		prevFatSect = fatSect;

		if(fatFindFreeAllocUnit( &clusterNo, &fatSect )) {
			NrOfClusters = cl;	//no more free, then it's the last cluster
			full++;
		}
		if(prevFatSect != fatSect) {
			mmcReadCached(prevFatSect);
		}

		if(cl < NrOfClusters) {
			//* ((unsigned short*) (&mmc_sector_buffer[ prevClust * fatSize ])) = 0x1234 ;
			//clusterNo & 0x0000FFFFL;
			if(SDFlags.Fat32Enabled) {
				* ((u32*) &mmc_sector_buffer[prevClust * fatSize]) = clusterNo & fatMask ;
			}
			else {
				* ((unsigned short*) &mmc_sector_buffer[prevClust * fatSize]) = clusterNo & fatMask ;
			}
			//printf("clPtr: %p, prevClust: %lx, fatSize: %i\n", clPtr, prevClust, fatSize);
		}
		else {
			//printf("Last cluster\n");
			//* (unsigned short*) &mmc_sector_buffer[ prevClust * fatSize ] = 0xFFFF;
			if(SDFlags.Fat32Enabled) {
				* ((u32*) &mmc_sector_buffer[prevClust * fatSize]) = CLUST_EOFE & fatMask ;
			}
			else {
				* ((unsigned short*) &mmc_sector_buffer[prevClust * fatSize]) = CLUST_EOFE & fatMask ;
			}
		}
		mmcWriteCached(0);
		mmcWrite(prevFatSect + FATSectors);	//second FAT
		//extern u32 n_actual_mmc_sector = prevFatSect + FATSectors;	//alternate, but needs 18bytes more
		//mmcWriteCached(0);
	}

	if(full) {	//if no more free clusters, correct file size
		mmcReadCached(dirSectNo);	//reread dir sector
		de[entryNo].deFileSize = ((NrOfClusters+1) * BytesPerSector * SectorsPerCluster);
		mmcWriteCached(1);			//save new entry
	}

	fatGetDirEntry(last_dir_entry+1,0);	//reread dir_entry to set the values to the new entry
	return(firstSec);
}


u32 fatFileNew (u32 size) {

	unsigned short found = 0;
	unsigned short f = 0;
	char filename[9];

	//check size
	if (size == 0 || size > (SectorsPerCluster*BytesPerSector*(1UL<<16)))
		return(0);

	//set directory to last used
	FileInfo.vDisk->dir_cluster = last_dir_start_cluster;

	//find new filename
	//struct direntry * de = (struct direntry*) mmc_sector_buffer;
	unsigned short i = 0;
	while ( fatGetDirEntry(i++,0) ) {
		sscanf((char*)atari_sector_buffer, "SDMX%hu", &f);
		if(f > found) found = f;
		//printf("entry: %i, clust: %i attr: 0x%02x\n", i, FileInfo.vDisk->start_cluster, FileInfo.Attr);
		//printf("filename: %s, found: %i\n", atari_sector_buffer, found);
	}
	if(i == 1) return(0);			//empty disk unsupported

	found++;
	if(found > 9999) return(0);		//9999 new files max, should be enough
		
	sprintf_P(filename, PSTR("SDMX%.4u"), found);
	return(fatFileCreate(filename, size));	//create file and return first sector, if ok
}
