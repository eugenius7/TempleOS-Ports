// MosesStaffv1.cpp : Defines the entry point for the application. Program execution begins and ends there.
//
// C++ Implementation of C/Adam/Opt/Boot/DskISORedSea.HC.Z
// Written by Austin Sierra contact@churchofaichrist.com
// Art by Miguel Costa miguel_gamba91@hotmail.com
// Last Changed April 09 2023
// version 1.1: previous build March 13 2023
// 
// Moses' Staff© is a registered copyright under the US Copyright Office 
// Reproduction or sharing of this work without proper licensing is punishable under US Copyright Law
// 
// If Using VS2019 or C==<17: Open Project\<project name> Properties
// Select Configuration Properties\General
// To the right, there is a field named C++ Standard, select one of these :
// ISO C++17 Standard(/ std:c++17)
// Preview - Features from the latest C++ working draft(/ std:c++latest)
//
// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu
//
// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project

#include "framework.h"
#include "MosesStaffv1.h"
//need this pragma directive to provide extra required details for binary parse to work correctly
#pragma pack(push, 1)
#pragma warning(disable : 4996)
#pragma warning(disable:6385)
//streams
#include <iostream>
#include <fstream>
#include <istream>
#include <sstream>

//libraries and filesystem
# include <stdio.h>
#include <string.h>
#include <filesystem>
#include <ctime>
#include <cmath>

//objects
#include <string>
#include <cstring>
#include <atlstr.h>
#include <vector>
#include <bitset>
#include <cstddef>
#include <cstring>

//gui
#include <Windows.h>
#include <mmsystem.h>
#include <mciapi.h>
#include "framework.h"

//Drive definitions

//The #define creates a macro, which is the association of an identifier or parameterized identifier with a token string. 
#define BLK_SIZE_BITS 9 //TempleOS uses 9bit Bytes
#define BLK_SIZE (1<<BLK_SIZE_BITS)// 1<<9 =512 The default sector-size for almost every single HDD
#define DVD_BLK_SIZE (4*BLK_SIZE)//2048 Palindrome integer 16 binary
#define myDVD_BLK_SIZE (32768 / 8)//Systembuffer- templeos system uses a zerobuf allocation for 2048 is 16384 bits . so 4096 is  32768 bits
#define DVD_BOOT_LOADER_SIZE DVD_BLK_SIZE*1

//ISO9660 and Drive Constants from TempleOS
#define ISOT_BOOT_RECORD 0
#define ISOT_PRI_VOL_DESC 1
#define ISOT_SUPPLEMENTARY_DESC 2
#define ISOT_VOL_DRV_DESC 3
#define ISOT_TERMINATOR 255
#define RS_ATTR_READ_ONLY 0x01
#define RS_ATTR_HIDDEN 0x02
#define RS_ATTR_SYSTEM 0x04
#define RS_ATTR_SYSTEM_16 0x0004
#define RS_ATTR_VOL_ID 0x08
#define RS_ATTR_DIR 0x10//16
#define RS_ATTR_ARCHIVE 0x20//32
#define RS_ATTR_DELETED 0x100
#define RS_ATTR_RESIDENT 0x200
#define RS_ATTR_COMPRESSED 0x400
#define RS_ATTR_CONTIGUOUS 0x800
#define RS_ATTR_FIXED 0x1000
#define RS_BLK_SIZE 512
#define RS_ROOT_CLUS 0x5A;//90
#define RS_DRV_OFFSET 0xB000
#define INVALID_CLUS (-1)
#define SECTORSIZE 512
#define BDT_RAM 1
#define BDT_ATA 2
#define BDT_ISO_FILE_READ 3
#define BDT_ISO_FILE_WRITE 4
#define BDT_ATAPI 5
#define BDF_READ_CACHE 0x020
#define BDF_REMOVABLE 0x001
#define BDF_READ_ONLY 0x004
#define BD_SIGNATURE_VAL 'BDSV'
#define DRV_SIGNATURE_VAL 'DVSV'
#define FFB_NEXT_BLK I64_MAX
#define FSt_NULL 0
#define FSt_REDSEA  1
#define FSt_UNKNOWN 5  
#define FSt_FAT32 2//Supported except for short names, to some degree
#define FSt_ISO9660 3 //Not Supported
#define FSt_NTFS 4 //Not Supported
#define MBR_PT_FAT12 0x01
#define MBR_PT_FAT32a 0x0B
#define MBR_PT_FAT32b 0x0C
#define MBR_PT_FAT32c 0x1B
#define MBR_PT_FAT32d 0x1C
#define MBR_PT_FAT32e 0x8B
#define MBR_PT_FAT32f 0x8C
#define MBR_PT_NTFS 0x07
#define MBR_PT_REDSEA 0x88
#pragma comment(lib, "Winmm.lib")
#define MAX_LOADSTRING 100

//
// Global Variables:
//

HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
HBITMAP hBitmap = NULL;
HWND hwndButton = NULL;
HWND directory, prefix, output = NULL;
wchar_t text1[255], text2[255], text3[255];
int input1,input2,input3;
char arg1c[255], arg2c[255], arg3c[255];
// Forward declarations of functions included in this code module:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);


//BMP Structure for BMP 24 and associated utility functions
struct BMP {

	//https://en.wikipedia.org/wiki/BMP_file_format
	myBITMAPFILEHEADER file_header;
	myBITMAPINFOHEADER bmp_info_header;
	myBITMAPINFO bmp_info;
	myBMPColorHeader bmp_color_header;
	std::vector<uint8_t> data;
	uint64_t filesize = 0;

	//constructor 
	//calls read function
	BMP(const char* fname) {
		read(fname);

	}

	//takes a filename and saves to structure 
	void read(const char* fname) {

		//
		//read file info header and header
		//

		std::ifstream file(fname, std::ios::binary);
		if (file)
		{
			//std::streampos fileSize;

			file.read((char*)&file_header, sizeof(file_header));//read header
			if (file_header.bfType != 0x4D42) {
				//not bitmap
				throw std::runtime_error("Wrong image file format");
			}
			file.read((char*)&bmp_info, sizeof(bmp_info_header) + sizeof(bmp_info.bmiColors));
			//file.read((char*)&bmp_info_header, sizeof(bmp_info_header));


			// The BMPColorHeader is used only for transparent images
			if (bmp_info_header.biBitCount == 32) {
				// Check if the file has bit mask color information
				if (bmp_info_header.biSize >= (sizeof(myBITMAPINFOHEADER) + sizeof(myBMPColorHeader))) {
					file.read((char*)&bmp_color_header, sizeof(bmp_color_header));
					// Check if the pixel data is stored as BGRA and if the color space type is sRGB
					check_color_header(bmp_color_header);

				}
				else {
					std::cerr << "Warning! The file \"" << fname << "\" does not contain bit mask information\n";
					throw std::runtime_error("Error! Unrecognized file format.");
				}
			}
			else if (bmp_info_header.biBitCount == 4)//see
			{


			}


			// Jump to the pixel data location
			file.seekg(file_header.bfOffBits, file.beg);

			//
			// Save file sizes
			// Some editors will put extra info in the image file, we only save the headers and the data.
			//bit count reserve
			if (bmp_info.bmiHeader.biBitCount == 32) {
				bmp_info.bmiHeader.biSize = sizeof(myBITMAPINFOHEADER) + sizeof(myBMPColorHeader);
				file_header.bfOffBits = sizeof(myBITMAPFILEHEADER) + sizeof(myBITMAPINFOHEADER) + sizeof(myBMPColorHeader);
			}
			else {
				bmp_info.bmiHeader.biSize = sizeof(myBITMAPINFOHEADER);
				file_header.bfOffBits = sizeof(myBITMAPFILEHEADER) + sizeof(myBITMAPINFOHEADER);

			}
			file_header.bfSize = file_header.bfOffBits;

			//Save device context size
			if (bmp_info.bmiHeader.biHeight < 0) {
				throw std::runtime_error("The program can treat only BMP images with the origin in the bottom left corner!");

			}

			data.resize(bmp_info.bmiHeader.biWidth * bmp_info.bmiHeader.biHeight * bmp_info.bmiHeader.biBitCount / 8);

			// Here we check if we need to take into byte row start and stop bits
			if (bmp_info.bmiHeader.biWidth % 4 == 0) {
				file.read((char*)data.data(), data.size());
				file_header.bfSize += data.size();

			}
			else {
				row_stride = bmp_info.bmiHeader.biWidth * bmp_info.bmiHeader.biBitCount / 8;
				uint32_t new_stride = make_stride_aligned(4);
				std::vector<uint8_t> padding_row(new_stride - row_stride);

				for (int y = 0; y < bmp_info.bmiHeader.biHeight; ++y) {
					file.read((char*)(data.data() + row_stride * y), row_stride);
					file.read((char*)padding_row.data(), padding_row.size());

				}
				file_header.bfSize += data.size() + bmp_info.bmiHeader.biHeight * padding_row.size();

			}
			file.seekg(0, std::ios::end);
			filesize = file.tellg();

		}
		else {
			throw std::runtime_error("Unable to open the input image file.");

		}


	}

	//Utility function to make a new BMP (unused)
	BMP(int32_t width, int32_t height, bool has_alpha = true) {
		if (width <= 0 || height <= 0) {
			throw std::runtime_error("The image width and height must be positive numbers.");

		}

		bmp_info.bmiHeader.biWidth = width;
		bmp_info.bmiHeader.biHeight = height;
		if (has_alpha) {
			bmp_info.bmiHeader.biSize = sizeof(myBITMAPINFOHEADER) + sizeof(myBMPColorHeader);
			file_header.bfOffBits = sizeof(myBITMAPFILEHEADER) + sizeof(myBITMAPINFOHEADER) + sizeof(myBMPColorHeader);

			bmp_info.bmiHeader.biBitCount = 32;
			bmp_info.bmiHeader.biCompression = 3;
			row_stride = width * 4;
			data.resize(row_stride * height);
			file_header.bfSize = file_header.bfOffBits + data.size();
		}
		else {
			bmp_info.bmiHeader.biSize = sizeof(myBITMAPINFOHEADER);
			file_header.bfOffBits = sizeof(myBITMAPFILEHEADER) + sizeof(myBITMAPINFOHEADER);

			bmp_info.bmiHeader.biBitCount = 24;
			bmp_info.bmiHeader.biCompression = 0;
			row_stride = width * 3;
			data.resize(row_stride * height);

			uint32_t new_stride = make_stride_aligned(4);
			file_header.bfSize = file_header.bfOffBits + data.size() + bmp_info.bmiHeader.biHeight * (new_stride - row_stride);
		}
	}

	//write BMP structure to file
	void write(const char* fname) {
		std::ofstream of{ fname, std::ios_base::binary };


		if (of) {
			if (bmp_info.bmiHeader.biBitCount == 32) {
				write_headers_and_data(of);

			}
			else if (bmp_info.bmiHeader.biBitCount == 24) {
				if (bmp_info.bmiHeader.biWidth % 4 == 0) {
					write_headers_and_data(of);

				}
				else {
					uint32_t new_stride = make_stride_aligned(4);
					std::vector<uint8_t> padding_row(new_stride - row_stride);

					write_headers(of);

					for (int y = 0; y < bmp_info.bmiHeader.biHeight; ++y) {
						of.write((const char*)(data.data() + row_stride * y), row_stride);
						of.write((const char*)padding_row.data(), padding_row.size());

					}

				}

			}
			else if (bmp_info.bmiHeader.biBitCount == 4)
			{
				write_headers_and_data(of);
			}
			else {
				throw std::runtime_error("The program can treat only 24 or 32 bits per pixel BMP files");

			}

		}
		else {
			throw std::runtime_error("Unable to open the output image file.");
		}

	}

private:
	//Internal Helper functions for reading
	//the bit array for a bitmap. Uses the Header


	//
	//stride over the bit array until the start of the next data point
	//
	uint32_t row_stride{ 0 };
	// Add 1 to the row_stride until it is divisible with align_stride
	uint32_t make_stride_aligned(uint32_t align_stride) {
		uint32_t new_stride = row_stride;
		while (new_stride % align_stride != 0) {
			new_stride++;

		}
		return new_stride;

	}

	//write headers 
	void write_headers(std::ofstream& of) {
		of.write((const char*)&file_header, sizeof(file_header));
		of.write((const char*)&bmp_info, sizeof(bmp_info));
		if (bmp_info.bmiHeader.biBitCount == 32) {
			of.write((const char*)&bmp_color_header, sizeof(bmp_color_header));

		}

	}
	void write_headers_and_data(std::ofstream& of) {
		write_headers(of);
		of.write((const char*)data.data(), data.size());

	}


	// Check if the pixel data is stored as BGRA and if the color space type is sRGB
	void check_color_header(myBMPColorHeader& bmp_color_header) {
		myBMPColorHeader expected_color_header;
		if (expected_color_header.red_mask != bmp_color_header.red_mask ||
			expected_color_header.blue_mask != bmp_color_header.blue_mask ||
			expected_color_header.green_mask != bmp_color_header.green_mask ||
			expected_color_header.alpha_mask != bmp_color_header.alpha_mask) {
			throw std::runtime_error("Unexpected color mask format! The program expects the pixel data to be in the BGRA format");
		}
		if (expected_color_header.color_space_type != bmp_color_header.color_space_type) {
			throw std::runtime_error("Unexpected color space type! The program expects sRGB values");
		}
	}
};
typedef unsigned char BYTE;


/*
		IMAGE to RedSEA Functions  
*/

//class for converting images to iso.c files
class Convertor {
public:

	//variables
	FILE* binaryImage;
	const char* filename;
	int filenum = 0;

	/*Constructor*/
	int Converter()
	{
		return 0;
	}

	//Functions called from Main
	//Converts an entire image directory to a binary array
	void convertImageDirectoryToBinaryArray(std::string kernelImage, std::string prefix)
	{
		/*
			PART 1:
		*/
		//Convert the image directory to binary arrays
		int iteration = 0;

		if (dirExist(kernelImage))
		{

			try {
				do {
					std::string file = getNextFile(kernelImage, prefix, iteration);
					std::string ext = ".bin";
					int length = file.length();

					// Extract directory path and file name
					std::filesystem::path filePath(file);
					std::string dirPath = filePath.parent_path().string();
					std::string fileName = filePath.filename().string();
					// Create binary directory if it doesn't exist
					if(iteration==0)
						std::filesystem::create_directory(dirPath + "\\binary");

					// Create new file path with .bin extension and binary directory
					std::string binFilePath = dirPath + "\\binary\\" + fileName.substr(0, fileName.length() - 4) + ext;

					// declaring character array (+1 for null terminator)
					char* char_array = new char[length + 1];
					char* char_array_new = new char[binFilePath.length() + 1];
					iteration++;
					// copying the contents of the
					// string to char array
					strcpy(char_array, file.c_str());
					strcpy(char_array_new, binFilePath.c_str());
					WriteImgToBinary(char_array, char_array_new);
					filenum++;
				} while (fileExists(getNextFile(kernelImage, prefix, iteration)));

			}
			catch (std::exception* e)
			{


			}

		}
		else
		{
			std::cout << "Error: Directory Not Found. Exiting...";
		}
	}

	//Following utility functions are unused and saved for reference
	//roundup to nearest clusters
	uint64_t roundup(uint64_t x)
	{
		if (x % 2048 == 0)
			return x;
		else
			return x + 2048 - x % 2048;
	}

	//make a free drive slot (unused)
	TDrv* drvMakeFreeSlot()
	{
		uint64_t i = 'T';
		TDrv* slot;
		memset(slot, 0, sizeof(TDrv));
		slot->drv_let = i;
		return slot;
	}

	//Custom ceil functions (unused)
	uint64_t CeilU64(uint64_t num, uint64_t to)
	{
		num += to - 1;
		return num - num % to;
	}
	uint64_t CeilI64(uint64_t num, uint64_t to)
	{
		if (num > 0)
		{
			num += to - 1;
			return num - num % to;
		}
		else
		{
			num += to - 1;
			return num - num % to - to;
		}

	}

	//From C:/Kernel/BlkDev/DskAddDev.HC.Z (unused)
	uint64_t BlkDevAdd(CBlkDev* bd, bool whole_drv, bool make_free)
	{
		uint64_t i, j, ext_base, offset, res = 0, num = 0;
		TDrv* dv;
		CRedSeaBoot redSeaBR;
		CMasterBoot masterBR;
		bd->bdsignature = BD_SIGNATURE_VAL;
		if (make_free)
		{
			dv = drvMakeFreeSlot();
		}
		//dv->bd = bd;
		dv->drvOffset + bd->drv_offset;
		dv->size = bd->max_blk + 1 - bd->drv_offset;
		switch (bd->type)
		{

		}

	}

	//end unused (TODO cleanup)

	//Retrieve the Filename from a directory.
	char* extractFilename(char* directory) {
		int len = strlen(directory);
		int start = len - 1;
		// Find the last path separator
		while (start >= 0 && directory[start] != '\\' && directory[start] != '/') {
			start--;
		}
		// Extract the filename
		char* filename = new char[len - start];
		strcpy(filename, directory + start + 1);
		return filename;
	}

	//get FileSize in uint64 from full file path
	uint64_t tellFile(const char* filePath)
	{
		FILE* fp;
		uint64_t file_size;

		// open the file in binary mode
		fp = fopen(filePath, "rb");
		if (fp == NULL) {
			printf("Error opening file\n");
			return 1;
		}
		// move the file pointer to the end of the file
		if (fseek(fp, 0L, SEEK_END) != 0) {
			printf("Error seeking to end of file\n");
			return 1;
		}
		// get the current position of the file pointer
		file_size = ftell(fp);
		if (file_size == -1) {
			printf("Error getting file size\n");
			return 1;
		}

		// close the file
		fclose(fp);
		return file_size;
	}


	//Create a new TDrive from our binary arrays
	//save to TDrv.bin 
	//once built, we can create the primary descriptor
	void makeTDrv(std::string src_directory, std::string prefix)
	{
		FILE* outFile = NULL; // name: 
		fopen_s(&outFile, "TDRV.bin", "wb");

		uint8_t* zero_buf = (uint8_t*)calloc(1, DVD_BLK_SIZE);//system area & reserved fields
		//Non ISO File System Information
		CRedSeaBoot* directoryHeader = (CRedSeaBoot*)calloc(1, DVD_BLK_SIZE);
		uint8_t* zero_buf1 = (uint8_t*)calloc(1, DVD_BLK_SIZE);//system area & reserved fields
		//Root Directory
		CDirEntry* dotrecord = (CDirEntry*)calloc(1, sizeof(CDirEntry));
		CDirEntry* dotdotrecord = (CDirEntry*)calloc(1, sizeof(CDirEntry));


		// Create a vector of BMP Objects
		std::vector<BMP*> ptrToImgs(filenum);
		//Allocate new BMP Directory Records
		CDirEntryBMP* record = (CDirEntryBMP*)calloc(filenum, sizeof(CDirEntryBMP));
		std::string ext = ".bin";
		for (int i = 0; i < filenum; i++)
		{
			// Extract directory path and file name
			std::string file = getNextFile(src_directory, prefix, i);
			std::filesystem::path filePath(file);
			int length = file.length();
			std::string dirPath = filePath.parent_path().string();
			std::string fileName = filePath.filename().string();

			// Create new file path with .bin extension and binary directory
			std::string binFilePath = dirPath + "\\binary\\" + fileName.substr(0, fileName.length() - 4) + ext;

			// declaring character array (+1 for null terminator)
			char* file_name_char_array = new char[binFilePath.length() + 1];

			strcpy(file_name_char_array, binFilePath.c_str());
			//initialize BMP Object
			ptrToImgs[i] = new BMP(file_name_char_array); 

		}



		char zeroBuffer2[512] = { 0 };
		//uint8_t* zero_buf2 = (uint8_t*)calloc(1, DVD_BLK_SIZE);//system area & reserved fields
		for (int i = 0; i < 256; i++)//write 256 bytes to the next sector
			fwrite(zero_buf, sizeof(zero_buf), 1, outFile);

		directoryHeader->signature = MBR_PT_REDSEA;//signature 0x88
		directoryHeader->drv_offset = 0x58;//drv_offset
		//Calculate cluster offset for files

		uint64_t* imageclusters = new uint64_t[filenum +2]();
	
		uint64_t finalcluster = RS_ROOT_CLUS;
		for (int clusterIndex = 0; clusterIndex < filenum +2; clusterIndex++)
		{
			//. and .. record
			if (clusterIndex == 0 || clusterIndex==1)
			{
				//set to root cluster
				imageclusters[clusterIndex] = RS_ROOT_CLUS;
			}
			else
			{
				//the first image is after the directoryentry clusters, plus the start root cluster, plus 1 for the empty buffer.
				if (clusterIndex == 2)
				{
					uint64_t directorysLength = int(1 + ((sizeof(CDirEntry) * (filenum + 2)) / RS_BLK_SIZE));
					imageclusters[clusterIndex] = imageclusters[clusterIndex - 1] + directorysLength;//+1;
				}
				else
				{
					//1st one works, but the second one doesn't
					//get previous file size
					std::string file = getNextFile(src_directory, prefix, clusterIndex - 3);
					const char* ptr = file.c_str();
					uint64_t fileSize = tellFile(ptr);
					uint64_t ct_size = int(1 + (fileSize / RS_BLK_SIZE));
					//Subsequent images begin after Previous file clusters, plus the last filesize plus 1 for the empty buffer
					imageclusters[clusterIndex] = imageclusters[clusterIndex - 1] + ct_size;// +1;
				}
			}
			finalcluster = imageclusters[clusterIndex];

		}

		directoryHeader->sects = roundup(finalcluster); //roundup();//4096; ;//clustersize;// //filecount//# sectors
		directoryHeader->root_clus = 0x5A;//RS_ROOT_CLUS;//Either 90 or 00?? system type copy of byte 5 from the partition table boot image
		directoryHeader->bitmaps_sects = 8;//;(filecount + BLK_SIZE << 3 - 1) / BLK_SIZE << 3;//bitmaps_sects
		time_t currentTime;
		struct tm* localTime;
		time(&currentTime);
		localTime = localtime(&currentTime);
		directoryHeader->unique_id = 0x4D4F535446 + localTime->tm_hour + localTime->tm_min + localTime->tm_sec;//M // //UNIQUE ID
		directoryHeader->signature2 = 0xAA55;
		dotrecord->attr = RS_ATTR_DIR;
		dotrecord->attr2 = 0x08;
		std::string str = ".";
		for (int i = 0; i < str.length(); i++)
		{
			dotrecord->name[i] = str[i];
		}
		dotrecord->clus = imageclusters[0];
		dotrecord->size[1] = RS_ATTR_SYSTEM;
		dotrecord->datetime.date = localTime->tm_year << localTime->tm_mon << localTime->tm_mday;
		dotrecord->datetime.time = localTime->tm_hour << localTime->tm_min << localTime->tm_sec;

		dotdotrecord->attr = RS_ATTR_DIR;
		dotdotrecord->attr2 = 0x08;//0x08 uncompressed.  0x0C .Z
		str = "..";
		for (int i = 0; i < str.length(); i++)
		{
			dotdotrecord->name[i] = str[i];
		}
		dotdotrecord->clus = imageclusters[1];
		dotdotrecord->size[1] = 0; //RS_ATTR_SYSTEM;
		dotdotrecord->datetime.date = localTime->tm_year << localTime->tm_mon << localTime->tm_mday;
		dotdotrecord->datetime.time = localTime->tm_hour << localTime->tm_min << localTime->tm_sec;

		//write img records
		for (int i = 0; i < filenum; i++)
		{
			record[i].attr = RS_ATTR_ARCHIVE;
			record[i].attr2 = 0x08;

			//get nextBMP name
			std::string file = getNextFile(src_directory, prefix, i);
			int length = file.length();
			// declaring character array (+1 for null terminator)
			char* file_name_char_array = new char[length + 5];
			std::string ext = ".bmp";
			strcpy(file_name_char_array, (file.substr(0, length - 4)).append(ext).c_str());
			//initialize BMP Object

			str = extractFilename(file_name_char_array);
			for (int j = 0; j < str.length(); j++)
			{
				record[i].name[j] = str[j];
			}
			record[i].clus = imageclusters[i+2];// (int)&ptrToImg;//not assigning value error?
			record[i].size = ptrToImgs[i]->filesize;
			record[i].datetime.date = localTime->tm_year << localTime->tm_mon << localTime->tm_mday;
			record[i].datetime.time = localTime->tm_hour << localTime->tm_min << localTime->tm_sec;
		}
		

		
		fwrite(directoryHeader, 512, 1, outFile);//FBlkWrite???? 20<<2
		for (int i = 0; i < 64; i++)//write 64 bytes to the next sector
			fwrite(zero_buf1, sizeof(zero_buf1), 1, outFile);
		fwrite(dotrecord, sizeof(CDirEntry), 1, outFile);//FBlkWrite???? 20<<2
		fwrite(dotdotrecord, sizeof(CDirEntry), 1, outFile);//FBlkWrite???? 20<<2
		for (int i = 0; i < filenum; i++)
		{
			fwrite(&record[i], sizeof(CDirEntryBMP), 1, outFile);//FBlkWrite???? 20<<2
		}
		uint64_t remainingBytes = bytesUntilNextBlock((sizeof(CDirEntryBMP) *filenum) + (sizeof(CDirEntry) * 2 ));
		while (remainingBytes > 0) {
			uint64_t bytesToWrite = remainingBytes < 512 ? remainingBytes : 512;
			fwrite(zeroBuffer2, 1, bytesToWrite, outFile);
			remainingBytes -= bytesToWrite;
		}


		//uint8_t* zero_buf3 = (uint8_t*)calloc(1, DVD_BLK_SIZE);//write between bmps
		for (int i = 0; i < filenum; i++)
		{
			// Extract directory path and file name
			std::string file = getNextFile(src_directory, prefix, i);
			std::filesystem::path filePath(file);
			int length = file.length();
			std::string dirPath = filePath.parent_path().string();
			std::string fileName = filePath.filename().string();

			// Create new file path with .bin extension and binary directory
			std::string binFilePath = dirPath + "\\binary\\" + fileName.substr(0, fileName.length() - 4) + ext;

			// declaring character array (+1 for null terminator)
			char* file_name_char_array = new char[binFilePath.length() + 1];

			strcpy(file_name_char_array, binFilePath.c_str());
			//Write Img Binary
			remainingBytes = WriteImg(outFile, file_name_char_array);
			while (remainingBytes > 0) {
				uint64_t bytesToWrite = remainingBytes < 512 ? remainingBytes : 512;
				fwrite(zeroBuffer2, 1, bytesToWrite, outFile);
				remainingBytes -= bytesToWrite;
			}
		}

		for (int i = 0; i < 4096; i++)//write empty bytes 
			fwrite(zero_buf1, sizeof(zero_buf1), 1, outFile);
		free(directoryHeader);
		free(dotrecord);
		free(dotdotrecord);
		free(zero_buf1);
		fclose(outFile);
	}



	//
	// Utility functions //
	//
	/*Does the Directory Exist*/
	bool dirExist(const std::string& s)
	{
		struct stat buffer;
		return (stat(s.c_str(), &buffer) == 0);
	}
	/*Does the File Exist*/
	bool fileExists(const std::string& filePath) {
		std::filesystem::path path(filePath);
		return std::filesystem::exists(path);
	}
	/*Return path of the next incremented file based on the prefix and iteration*/
	std::string getNextFile(std::string kernelImage, std::string prefix, int eration)
	{
		char intAsString[12];
		sprintf_s(intAsString, "%d", eration);
		std::string file = kernelImage + "\\" + prefix + "." + intAsString + ".bmp";
		return file;
	}

	//convert a string to binary (unused)
	std::string toBinary(std::string byte)
	{
		std::string binaryString = "";
		for (char& _char : byte)
		{
			binaryString += std::bitset<8>(_char).to_string();
		}
		return binaryString;
	}

	//replace a byte (unused)
	std::string placeBinary(int i, uint8_t structArr, std::string byte)
	{
		std::string binaryString = "";
		structArr = byte[i];// bitset<8>(byte[i]);
		return binaryString;
	}

	//returns number of bytes until next block (512 per cluster)
	uint64_t bytesUntilNextBlock(uint64_t objectSize) {
		uint64_t blockSize = RS_BLK_SIZE;
		if (objectSize >= blockSize) {
			uint64_t remainder = objectSize % blockSize;
			uint64_t bytesUntilNextBlock = blockSize - remainder;
			return bytesUntilNextBlock;
		}
		else {
			return blockSize - objectSize;
		}
	}

	//Write the image at directory 'filename' to our current position in FILE outfile
	//return the number of bytes until the next cluster.
	uint64_t WriteImg(FILE* outFile, const char* filename)
	{
		std::string byte;
		std::ifstream myfile;
		myfile.open(filename, std::ios::in | std::ios::binary);//binary mode to prevent FF 1A reading error
		uint64_t count = -1;
		uint64_t bytesInBlock = RS_BLK_SIZE;
		uint64_t fileSize = tellFile(filename);
		char c;


		//instead of using fwrite() us fputc. This ensures that the 0xFF 0x1A byte sequence is not interpreted 
		//as the EOF marker, 
		while (myfile.get(c))
		{
			if (count == -1)
				count = 0;
			fputc(c, outFile);
			count++;
			
		}
		if (count == -1)
			return -1;
		else
		{
			return bytesUntilNextBlock(count);
		}
	}

	/*Take our BMP and convert to binary for loading into ISO*/
	int WriteImgToBinary(char* filename, char* newFileName)
	{
		std::string byte;
		std::ifstream myfile;
		myfile.open(filename, std::ios::binary);
		//open output
		FILE* out;
		fopen_s(&out, newFileName, "wb");
		if (out == NULL)
		{
			return 1;
		}
		int count = 0;
		char buffer[512];
		// Read and write the input file in 512-byte chunks
		while (myfile.read(buffer, sizeof(buffer))) {
			if (count == 0)
				count = 1;
			fwrite(buffer, sizeof(buffer), 1, out);
		}

		// Write any remaining bytes from the input file
		if (myfile.gcount() > 0) {
			fwrite(buffer, myfile.gcount(), 1, out);
		}
		if (count == 0)
			return 1;

		myfile.close();
		fclose(out);

	}

	/*Take our BMP binary string and write to file for loading into ISO*/
	int WriteBytesToFile(std::vector<BYTE> bytearray, std::string output)
	{

		std::ofstream output_file(output, std::ios::out | std::ios::binary);

		for (int i = 0; i < bytearray.size(); i++)
		{
			output_file.write((const char*)&bytearray[0], bytearray.size());
		}
		output_file.close();
		return 0;
	}

	//read a file to a byteArray (unused)
	std::vector<BYTE> readFile(const char* filename)
	{
		// open the file:
		std::streampos fileSize;
		std::ifstream file(filename, std::ios::binary);

		// get its size:
		file.seekg(0, std::ios::end);
		fileSize = file.tellg();
		file.seekg(0, std::ios::beg);

		// read the data:
		std::vector<BYTE> fileData(fileSize);
		file.read((char*)&fileData[0], fileSize);
		file.close();
		return fileData;
	}

};

//
//Endian Functions//
// 

//RedSea integer endianness palindromes
uint16_t EndianU16(uint16_t d)
{
	uint16_t res = _byteswap_ushort(d);
	return res;
}
uint32_t EndianU32(uint32_t d)
{
	uint32_t res = _byteswap_ulong(d);
	return res;
}

//Fill the palindrome functions
void FillU16Palindrome(CPalindromeU16* dst, uint16_t w)
{
	dst->big = EndianU16(w);
	dst->little = w;
}
void FillU32Palindrome(CPalindromeU32* dst, uint32_t d)
{
	dst->big = EndianU32(d);
	dst->little = d;
}
uint64_t EndianU364(uint64_t d)
{
	uint64_t res = _byteswap_uint64(d);
	return res;
}


//write the primary descriptor and TDrv to file
static int writePrimaryDescriptor(const char* outfileName)
{
	//Allocate memory for the object we are serializing the data into
	uint8_t* zero_buf0 = (uint8_t*)calloc(1, myDVD_BLK_SIZE);//system area & reserved fields
	CISOPriDesc* iso_pri = (CISOPriDesc*)calloc(1, DVD_BLK_SIZE),//param: 1 element of size 'harddrive'
		* iso_boot = (CISOPriDesc*)calloc(1, DVD_BLK_SIZE),//This is also known as contiguous allocation. allocates the memory along with initializing the bits to zero.
		* iso_sup = (CISOPriDesc*)calloc(1, DVD_BLK_SIZE),
		* iso_term = (CISOPriDesc*)calloc(1, DVD_BLK_SIZE);

	uint64_t iso_size = 0;
	CElTorito* et = (CElTorito*)calloc(1, DVD_BLK_SIZE);


	//Get the size of the data we will be writing
	//get output size
	FILE* file = NULL; // name: *string* isoFile 
	FILE* outfile = NULL; // name: *string* isoFile 
	fopen_s(&file, "TDRV.bin", "r");
	if (file == NULL)
		return 1;
	fseek(file, 0L, SEEK_END); //0L start //2560
	long int ans = ftell(file) / DVD_BLK_SIZE;
	iso_size = ans; //ans;
	fclose(file);

	//type
	iso_pri->type = ISOT_PRI_VOL_DESC;

	//id
	std::string str = "CD001";
	for (int i = 0; i < str.length(); i++)
	{
		iso_pri->id[i] = str[i];
	}
	//std::memcpy(iso_pri->id, c.toBinary(str), sizeof str);

	//version
	iso_pri->version = 1;

	//Boot System Identifier, must be EL TORITO SPECIFICATION padded
	FillU16Palindrome(&iso_pri->vol_set_size, 1);
	FillU16Palindrome(&iso_pri->vol_seq_num, 1);
	FillU16Palindrome(&iso_pri->log_block_size, DVD_BLK_SIZE);
	FillU32Palindrome(&iso_pri->vol_space_size, iso_size);//iso_size set to 88
	FillU32Palindrome(&iso_pri->root_dir_record.loc, 90);//start root cluster set to 90
	//version
	iso_pri->file_structure_version = FSt_REDSEA;
	//publisher
	str = "TempleOS RedSea";
	for (int i = 0; i < str.length(); i++)
	{
		iso_pri->publisher_id[i] = str[i];
	}

	std::memcpy(iso_sup, iso_pri, DVD_BLK_SIZE);
	iso_sup->type = ISOT_SUPPLEMENTARY_DESC;
	iso_boot->type = ISOT_BOOT_RECORD;
	//id
	str = "CD001";
	for (int i = 0; i < str.length(); i++)
	{
		iso_boot->id[i] = str[i];
	}
	iso_boot->version = 1;
	str = "EL TORITO SPECIFICATION";
	int offset = 0;
	for (int i = 0; i < str.length(); i++)
	{
		iso_boot->pad1[i] = str[i];
	}
	for (int i = str.length(); i < str.length() + 42; i++)
	{
		iso_boot->pad1[i] = 0x00;
		offset = i;
	}
	iso_boot->pad1[offset] = 20;//cant verify

	//FillU32Palindrome(&iso_boot->vol_space_size, 14);
	//don't use palindrome functions

	fopen_s(&outfile, outfileName, "wb");
	if (outfile == NULL)
	{
		printf("Can't write to file, are you sure you don't have it open? %s\n", strerror(errno));
		return 1;

	}

	for (int i = 0; i < myDVD_BLK_SIZE; i++)
		fwrite(zero_buf0, sizeof(zero_buf0), 1, outfile);
	fwrite(iso_pri, sizeof(CISOPriDesc), 1, outfile);//Fileend -4
	iso_term->type = 0xFF;//oxFF but doesnt write correctly
	str = "CD001";
	for (int i = 0; i < str.length(); i++)
	{
		iso_term->id[i] = str[i];// bitset<8>(byte[i]);
	}

	iso_term->version = 1;

	//El Torito Drive Validation Entry
	et->w[0] = 1;
	et->w[1] = 0;
	et->w[2] = 0;
	et->w[3] = 0;
	str = "TempleOS";
	int j = 4;
	for (int i = 0; i < str.length(); i++)
	{
		et->w[j] = str[i];
		j++;
	}

	et->w2[15] = 0xAA;
	et->w2[14] = 0x55;//terminator
	et->w2[13] = 0xC7;
	et->w2[12] = 0x2E;
	//j = 0;//to un hardcode the checksum
	//for (int i = 0; i < 16; i++)
	//{
	//	j += et->w[i];//checksum
	//}
	//et->w2[13] = -j;
	et->bootable = 0x88;
	et->media_type = 0;
	et->sect_cnt = 4;
	et->load_rba = 21;//sector 21, eltorito is sector 20



	/*
	//Part 3: time to write to the ISO
	*/

	fwrite(iso_boot, sizeof(CISOPriDesc) , 1, outfile);//FBlkWrite????17<<2
	fwrite(iso_sup, sizeof(CISOPriDesc) , 1, outfile);//FBlkWrite????18<<2
	fwrite(iso_term, sizeof(CISOPriDesc) , 1, outfile);
	fwrite(et, sizeof(CISOPriDesc) , 1, outfile);//FBlkWrite???? 20<<2
	fclose(outfile);

	std::ofstream outFinalFile(outfileName, std::ios_base::app | std::ios_base::binary | std::ios_base::out);

	std::ifstream inFile("TDRV.bin", std::ios_base::binary | std::ios_base::in);
	inFile >> std::noskipws;
	char c;
	while (inFile >> c) {
		outFinalFile << c;
	}


	free(et);
	free(iso_pri);
	free(iso_boot);
	free(iso_sup);
	free(iso_term);
	free(zero_buf0);
	return 0;
}

//Program starts here
int ISOBOOTER(int argc, char** argv)
{
	//Part 1 Local Variables
	std::string kernelImage, prefix, isoFile, directory;
	const char* outfileName;
	Convertor c;

	//Part 2 Input from user
	if (argc == 0)
	{
		std::cout << "Redsea Converter\n";

		std::cout << "Enter the Kernel Image Directory Path (all files in directory must be bmp) :";
		std::cin >> kernelImage;
		std::cout << "Enter the Img Prefix Text (Ex: if your files are cat.0.bmp, cat.1.bmp etc, enter 'cat'):";
		std::cin >> prefix;
		std::cout << "Enter the CD ISO Image Output File Name:";
		std::cin >> isoFile;
		outfileName = isoFile.data();
	}
	else
	{
		if (argc == 4)
		{
			kernelImage = argv[1];
			prefix = argv[2];

			std::string myfileName(argv[3]);
			std::string ext = ".ISO.C";
			int length = myfileName.length();
			// declaring character array (+1 for null terminator)
			char* char_array = new char[length + 7];
			strcpy(char_array, myfileName.append(ext).c_str());

			outfileName = char_array;

		}
		else
		{
			std::cout << "Incorrect number of arguments";
			return 1;
		}
	}

	//Part 3 Convert Images to binary
	c.convertImageDirectoryToBinaryArray(kernelImage, prefix);

	//Part 4 Make our drive from the binary data
	c.makeTDrv(kernelImage,prefix);

	//Part 5 Write ISO9660 Primary Descriptor
	writePrimaryDescriptor(outfileName);


}


#pragma pack(pop)

//
//Windows GUI
//

std::wstring ExePath() {
	TCHAR buffer[MAX_PATH] = { 0 };
	GetModuleFileName(NULL, buffer, MAX_PATH);
	std::wstring::size_type pos = std::wstring(buffer).find_last_of(L"\\/");
	std::wstring returnMe = std::wstring(buffer).substr(0, pos);
	returnMe = returnMe + L"\\moses.bmp";
	return returnMe;
}

int wmain()
{

	return 0;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	/// TODO: Place code here.
	//MessageBox(hInstance, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
	//CreateWindow(" Kernel Image Directory Path ", 0, WS_BORDER | WS_CHILD | WS_VISIBLE, 56, 10, 50, 18, g_hWnd, 0, hInst, 0);
	//CreateWindow("Img Prefix ", 0, WS_BORDER | WS_CHILD | WS_VISIBLE, 56, 10, 50, 18, g_hWnd, 0, hInst, 0);
	//CreateWindow(" Kernel Image Directory Path ", 0, WS_BORDER | WS_CHILD | WS_VISIBLE, 56, 10, 50, 18, g_hWnd, 0, hInst, 0);
	//CreateWindow("CD ISO Image Output File Name", 0, WS_CHILD | WS_VISIBLE, 70, 70, 80, 25, g_hWnd, 0, hInst, 0);
	// Initialize global strings
	LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadStringW(hInstance, IDC_MOSESSTAFFV1, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MOSESSTAFFV1));

	MSG msg;

	// Main message loop:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int)msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEXW wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MOSESSTAFFV1));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_MOSESSTAFFV1);
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Store instance handle in our global variable

	HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return FALSE;
	}


	ShowWindow(hWnd, nCmdShow);
	//wmain();
	UpdateWindow(hWnd);

	return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
char* encode(const wchar_t* wstr, unsigned int codePage)
{
	int sizeNeeded = WideCharToMultiByte(codePage, 0, wstr, -1, NULL, 0, NULL, NULL);
	char* encodedStr = new char[sizeNeeded];
	WideCharToMultiByte(codePage, 0, wstr, -1, encodedStr, sizeNeeded, NULL, NULL);
	return encodedStr;
}

//Call ISOBooter
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{


	switch (message)
	{
	case WM_COMMAND:
	{
		int wmId = LOWORD(wParam);
		// Parse the menu selections:


		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		case 1:

			//GetWindowText(GetDlgItem(hWnd, 1002), reinterpret_cast <char*> ((char*)input3.c_str()), 255);
			input1 = GetWindowTextLength(directory);
			input2 = GetWindowTextLength(directory);
			input3 = GetWindowTextLength(directory);
			//char* t = &text1[0];
			//GetWindowText(directory, t, 255);
			//GetWindowText(prefix, &text2[0], 255);
			//GetWindowText(output, &text3[0], 255);
			//input1 = tstring(text1);
			//input2 = tstring(text2);
			//input3 = tstring(text3);
			GetWindowText(GetDlgItem(hWnd, 1000), text1, 255);
			GetWindowText(GetDlgItem(hWnd, 1001), text2, 255);
			GetWindowText(GetDlgItem(hWnd, 1002), text3, 255);
			//MessageBoxW(0, text1, text1, 0);
			//USES_CONVERSION;
			//GetWindowText(GetDlgItem(hWnd, 1001), reinterpret_cast <char*> ((char*)input2.c_str()), 255);
			//arg1c = T2A(input1);
			//::MessageBox(hWnd, text1, text1, MB_OK);

			for (int i = 0; i < input1; i++)
			{
				arg1c[i] = text1[i];
			}
			for (int i = 0; i < input2; i++)
			{
				arg2c[i] = text2[i];
			}
			for (int i = 0; i < input3; i++)
			{
				arg3c[i] = text3[i];
			}
			
			char* argv[4]; char arg0c[1]; arg0c[0] = '4';
			argv[0] = &arg0c[0];
			argv[1] = arg1c;//encode((wchar_t*)input1.c_str(), CP_UTF8);//arg1c;
			argv[2] = arg2c;//encode((wchar_t*)input2.c_str(), CP_UTF8);//arg2c;
			argv[3] = arg3c;//encode((wchar_t*)input3.c_str(), CP_UTF8);// arg3c;

			ISOBOOTER(4, argv);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
	}
	break;
	case WM_CREATE:
	{
		hwndButton = CreateWindow(
			L"BUTTON",  // Predefined class; Unicode assumed 
			L"CONVERT",      // Button text 
			WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
			10,         // x position 
			10,         // y position 
			100,        // Button width
			100,        // Button height
			hWnd,     // Parent window
			(HMENU)1,       // No menu.
			NULL,
			NULL);      // Pointer not needed.
		directory = CreateWindow(
			L"EDIT",  // Predefined class; Unicode assumed 
			L"DIRECTORY",      // Button text 
			WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
			300,         // x position 
			10,         // y position 
			300,        // Button width
			20,        // Button height
			hWnd,     // Parent window
			(HMENU) 1000,       //1000
			NULL,
			NULL);      // Pointer not needed.
		prefix = CreateWindow(
			L"EDIT",  // Predefined class; Unicode assumed 
			L"PREFIX",      // Button text 
			WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
			300,         // x position 
			40,         // y position 
			300,        // Button width
			20,        // Button height
			hWnd,     // Parent window
			(HMENU)1001,       // 1001
			NULL,
			NULL);      // Pointer not needed.
		output = CreateWindow(
			L"EDIT",  // Predefined class; Unicode assumed 
			L"OUTPUT",      // Button text 
			WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,  // Styles 
			300,         // x position 
			70,         // y position 
			300,        // Button width
			20,        // Button height
			hWnd,     // Parent window
			(HMENU)1002,       // 1002
			NULL,
			NULL);      // Pointer not needed.
		hBitmap = (HBITMAP)LoadImage(hInst, ExePath().c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);
		PlaySound(TEXT("speed.wav"), 0, SND_FILENAME | SND_ASYNC);
		//CreateWindow("EDIT", "", WS_BORDER | WS_CHILD | WS_VISIBLE, 10, 10, 400, 20, hWnd,NULL,NULL,NULL);
		//CreateWindow("Img Prefix ", 0, WS_BORDER | WS_CHILD | WS_VISIBLE, 56, 10, 50, 18, hWnd, NULL, NULL, NULL);
		//CreateWindow(" Kernel Image Directory Path ", 0, WS_BORDER | WS_CHILD | WS_VISIBLE, 56, 10, 50, 18, hWnd, NULL, NULL, NULL);
		//CreateWindow("CD ISO Image Output File Name", 0, WS_CHILD | WS_VISIBLE, 70, 70, 80, 25, hWnd, NULL, NULL, NULL);

		if (hWnd != NULL) { MoveWindow(hWnd, 300, 300, 640, 380, TRUE); }
		break;
	}
	case WM_PAINT:
	{
		PAINTSTRUCT     ps;
		HDC             hdc;
		BITMAP          bitmap;
		HDC             hdcMem;
		HGDIOBJ         oldBitmap;

		hdc = BeginPaint(hWnd, &ps);

		hdcMem = CreateCompatibleDC(hdc);
		oldBitmap = SelectObject(hdcMem, hBitmap);

		GetObject(hBitmap, sizeof(bitmap), &bitmap);
		BitBlt(hdc, 0, 0, bitmap.bmWidth, bitmap.bmHeight, hdcMem, 0, 0, SRCCOPY);

		SelectObject(hdcMem, oldBitmap);
		DeleteDC(hdcMem);

		EndPaint(hWnd, &ps);
	}
	break;
	case WM_DESTROY:
		DeleteObject(hBitmap);
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}
