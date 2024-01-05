
// MosesStaffv1header: This file contains the 'main' function. Program execution begins and ends there.
// C++ Implementation of C/Adam/Opt/Boot/DskISORedSea.HC.Z
// Written by Austin Sierra contact@churchofaichrist.com
// Art by Miguel Costa miguel_gamba91@hotmail.com
// Last Change Jan 3 2023
// Church of AI Christ Licensed Attribution-ShareAlike CreativeCommons BY-SA
#pragma once

#include "resource.h"
#include <cstdint>
//https://learn.microsoft.com/en-us/previous-versions/dd183376(v=vs.85)
//https://web.archive.org/web/20220602165914/https://solarianprogrammer.com/2018/11/19/cpp-reading-writing-bmp-images/

//1st Structure
typedef struct myBITMAPFILEHEADER {
	uint16_t   bfType;
	uint32_t  bfSize;
	uint16_t  bfReserved1;
	uint16_t   bfReserved2;
	uint32_t  bfOffBits;

} myBITMAPFILEHEADER;

//2nd structure
typedef struct myBITMAPINFOHEADER {
	uint32_t  biSize;//	An integer (unsigned) representing the size of the header in bytes. It should be '40' in decimal to represent BITMAPINFOHEADER header type.
	uint32_t  biWidth;
	uint32_t   biHeight;
	uint16_t   biPlanes;//An integer (unsigned) representing the number of color planes of the target device. Should be '1' in decimal.
	uint16_t  biBitCount;//An integer (unsigned) representing the number of bits (memory) a pixel takes (in pixel data) to represent a color.
	uint32_t biCompression;
	uint32_t  biSizeImage;
	uint32_t   biXPelsPerMeter;
	uint32_t   biYPelsPerMeter;
	uint32_t  biClrUsed;
	uint32_t  biClrImportant;
} myBITMAPINFOHEADER;
/* biBitCount; EXTENDED NOTES
0	The number of bits per pixel is specified or is implied by the JPEG or PNG file format.

1	The bitmap is monochrome, and the bmiColors member of BITMAPINFO contains two entries. Each bit in the bitmap array represents a pixel.
If the bit is clear, the pixel is displayed with the color of the first entry in the bmiColors color table. If the bit is set, the pixel has the color of the second entry in the table.

4	The bitmap has a maximum of 16 colors, and the bmiColors member of BITMAPINFO contains up to 16 entries. Each pixel in the bitmap is represented by a 4-bit index into the color table.
For example, if the first byte in the bitmap is 0x1F, the byte represents two pixels. The first pixel contains the color in the second table entry, and the second pixel contains the color
in the sixteenth table entry. TempleOS Mode.

8	The bitmap has a maximum of 256 colors, and the bmiColors member of BITMAPINFO contains up to 256 entries.
In this case, each byte in the array represents a single pixel.

16	The bitmap has a maximum of 2^16 colors. If the bV5Compression member of the BITMAPV5HEADER structure is BI_RGB,
the bmiColors member of BITMAPINFO is NULL. Each WORD in the bitmap array represents a single pixel. The relative intensities of
red, green, and blue are represented with five bits for each color component. The value for blue is in the least significant five bits,
followed by five bits each for green and red. The most significant bit is not used. The bmiColors color table is used for optimizing colors
used on palette-based devices, and must contain the number of entries specified by the bV5ClrUsed member of the BITMAPV5HEADER.
If the bV5Compression member of the BITMAPV5HEADER is BI_BITFIELDS, the bmiColors member contains three DWORD color masks that specify the red, green, and blue components,
respectively, of each pixel. Each WORD in the bitmap array represents a single pixel.
When the bV5Compression member is BI_BITFIELDS, bits set in each DWORD mask must be contiguous and should not overlap the bits of another mask.
All the bits in the pixel do not need to be used.

24	The bitmap has a maximum of 2^24 colors, and the bmiColors member of BITMAPINFO is NULL.
Each 3-byte triplet in the bitmap array represents the relative intensities of blue, green, and red, respectively, for a pixel.
The bmiColors color table is used for optimizing colors used on palette-based devices, and must contain the number of entries specified by the bV5ClrUsed member of the BITMAPV5HEADER structure.

32	The bitmap has a maximum of 2^32 colors. If the bV5Compression member of the BITMAPV5HEADER is BI_RGB, the bmiColors member of BITMAPINFO is NULL.
Each DWORD in the bitmap array represents the relative intensities of blue, green, and red for a pixel. The value for blue is in the least significant 8 bits,
followed by 8 bits each for green and red. The high byte in each DWORD is not used. The bmiColors color table is used for optimizing colors used on palette-based devices,
and must contain the number of entries specified by the bV5ClrUsed member of the BITMAPV5HEADER.If the bV5Compression member of the BITMAPV5HEADER is BI_BITFIELDS,
the bmiColors member contains three DWORD color masks that specify the red, green, and blue components of each pixel. Each DWORD in the bitmap array represents a single pixel.
*/
//3rd structure colorArray
typedef struct RGBQUADs {
	unsigned char rgbBlue;
	unsigned char rgbGreen;
	unsigned char rgbRed;
	unsigned char rgbReserved;
} RGBQUADs;
typedef struct myBITMAPINFO {
	myBITMAPINFOHEADER bmiHeader;
	RGBQUADs          bmiColors[16];
} myBITMAPINFO;

//4th structure //transparancy
struct myBMPColorHeader {
	uint32_t red_mask{ 0x00ff0000 };         // Bit mask for the red channel
	uint32_t green_mask{ 0x0000ff00 };       // Bit mask for the green channel
	uint32_t blue_mask{ 0x000000ff };        // Bit mask for the blue channel
	uint32_t alpha_mask{ 0xff000000 };       // Bit mask for the alpha channel
	uint32_t color_space_type{ 0x73524742 }; // Default "sRGB" (0x73524742)
	uint32_t bmiColors[16];

};

struct CPalindromeU16
{
	uint16_t little, big;
}; CPalindromeU16;

struct CPalindromeU32
{
	uint32_t little, big;
}; CPalindromeU32;


//boot catalog class for ElTorito, ISO 9660 Addon
//https://www.intel.com/content/dam/support/us/en/documents/motherboards/desktop/sb/specscdrom.pdf
struct CElTorito
{

	uint8_t w[16];//Validation Entry
	uint8_t w2[16];
	//Initial Entry must contain a boot image which consists of generic programs/drivers that use only the BIOS
	uint8_t bootable;//88=bootable 00=not bootable
	uint8_t media_type; //0=no emulation 4=hard disk
	uint16_t load_seg; //0000->07C0 This is the load segment for the initial boot image
	uint8_t sys_Type; //System Type.This must be a copy of byte 5 (System Type) from the Partition Table
	uint8_t zero; //empty
	uint16_t sect_cnt; //virtual emulated sectors the system will store at the log segment
	uint32_t load_rba; //start addr of virtual disk
	uint8_t zero2[20]; //empty
};

//ISO Dir Entry
struct CISODirEntry
{

	uint8_t pad[2];
	CPalindromeU32 loc; //This field shall specify as a 32-bit number the Logical Block Number of the first Logical Block allocated to the extent
	uint8_t pad2[24];
};
//Root Directory and Sub Directories
struct CISODirMatchEntry
{
	uint8_t pad[512];
};

struct CDate
{
	uint32_t time;
	uint32_t date;
};

#define namelength 38//include zero terminator
struct CDirEntry
{
	uint8_t attr;
	uint8_t attr2;
	uint8_t name[namelength];
	uint64_t clus;
	uint8_t size[8];
	CDate datetime;
}; CDirEntry;

struct CDirEntryBMP
{
	uint8_t attr;
	uint8_t attr2;
	uint8_t name[namelength];
	uint64_t clus;
	uint64_t size;
	//uint8_t size[8];
	//uint8_t pad[4];
	CDate datetime;
}; CDirEntry;


/*
struct myDrive
{
	uint8_t* zerobuf;
	CRedSeaBoot* directoryHeader;
	CDirEntry* dotrecord,dotdotrecord;
	uint8_t* zerobuf2;
	uint8_t* zerobuf;
	uint8_t* zerobuf;

};*/

//PriDesc stands for Primary Volume Descriptor
struct CISOPriDesc
{

	uint8_t type,
		id[5],
		version,
		pad1[73];
	CPalindromeU32 vol_space_size;
	uint8_t pad2[32];
	CPalindromeU16 vol_set_size;
	CPalindromeU16 vol_seq_num;
	CPalindromeU16 log_block_size;
	uint8_t pad3[16];//16
	//Contained within the primary volume descriptor is the root directory record describing the location of the contiguous root directory.
	CISODirEntry root_dir_record;
	uint8_t pad4[130],//128
		publisher_id[128],
		pad5[435],
		file_structure_version,
		pad6[1170];//1166

};

//unused
struct CFAT32DirEntry
{
	uint8_t name[11],
		attr, NTres, CrtTimeTenth;
	uint16_t CrtTime, CrtDate, LstAccDate, clus_hi, WritTime, WrtDate, cluslo;
	uint32_t size;
};

struct CMBRPrt
{
	uint8_t active, //0x80=active 0x00=inactive
		start_head;
	uint16_t start_cyl;
	uint8_t type,
		end_head;
	uint16_t end_cyl;
	uint32_t offset, //sects between MasterBootRecord and first sect
		size;		//sects in drv
};
struct CMasterBoot {
	uint8_t code[440];
	uint32_t media_id;
	uint16_t zero;
	CMBRPrt p[4];
	uint16_t signature2;//AA55
};

struct CRedSeaBoot {
	uint8_t jump_and_nop[3], signature, reserved[4];//MBR_PT_REDSEA=0x88. Distinguish from real FAT32
	uint64_t drv_offset; //FOR CD/DVD image copy
	uint64_t sects;
	uint64_t	root_clus,
		bitmaps_sects,
		unique_id;
	uint8_t code[462];
	uint16_t signature2;//0xAA55
};



struct CFAT32FileInfoSect
{
	uint32_t signature1;
	uint8_t unknown[480];
	uint32_t signature2, free_clus, most_recently_alloced;
	uint8_t reserved[12];
	uint32_t signature3;
};

struct CFreeLst {
	CFreeLst* next, * last;
	uint64_t start, size;
};
//Simulated TDrive
struct TDrv
{
	uint64_t locked_flags;
	uint32_t dv_signature;
	uint8_t drv_let, pad;
	uint16_t fs_type;
	uint64_t drvOffset,
		size,
		prt_num,
		file_system_info_sect,
		fat1, fat2,
		root_clus,
		data_area,
		spc;//sectors per clus
	CDate fat32_local_time_offset;
	//CTask* owning_task;
	//CBlkDev* bd;
	CFAT32FileInfoSect* fis;
	uint64_t fat_blk_dirty,
		cur_fat_blk_num;
	uint32_t* cur_fat_blk;
	CFreeLst* nextfree, * lastfree;
};

struct CFile {
	uint64_t flags;
	CDirEntry de;
	TDrv* dv;
	uint64_t fblk_num, clus, file_clus_num, max_blk;
	uint8_t* clus_buf;
};

struct CBlkDev
{
	//CBlkDev* lock_fwding;
	uint64_t locked_flags;
	uint32_t bdsignature, type, flags;
	uint8_t first_drv_let, unit, pad[2];
	uint32_t base0, base1,
		blk_size;
	uint64_t drv_offset, init_root_dir_blks,
		max_blk;
	uint16_t* dev_id_record;
	uint8_t* RAM_dsk,
		* file_dsk_name;
	//CFile* file_dsk;
	//CTask* owning_task;
	double last_time;
	uint32_t max_reads, max_writes;
};