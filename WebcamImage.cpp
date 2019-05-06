// WebcamImage.cpp : Defines the entry point for the console application.
//
#include <Windows.h>
#include <tchar.h>
#include "WebcamAccess.h"

void CreateBitmapFile(LPCWSTR fileName, long width, long height, WORD bitsPerPixel, BYTE * bitmapData, DWORD bitmapDataLength)
{
    HANDLE file;
    BITMAPFILEHEADER fileHeader;
    BITMAPINFOHEADER fileInfo;
    DWORD writePosn = 0;

    file = CreateFile(fileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);  //Sets up the new bmp to be written to

    fileHeader.bfType = 19778;                                                                    //Sets our type to BM or bmp
    fileHeader.bfSize = sizeof(fileHeader.bfOffBits) + sizeof(RGBTRIPLE);                         //Sets the size equal to the size of the header struct
    fileHeader.bfReserved1 = 0;                                                                    //sets the reserves to 0
    fileHeader.bfReserved2 = 0;
    fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);                    //Sets offbits equal to the size of file and info header
    fileInfo.biSize = sizeof(BITMAPINFOHEADER);
    fileInfo.biWidth = width;
    fileInfo.biHeight = height;
    fileInfo.biPlanes = 1;
    fileInfo.biBitCount = bitsPerPixel;
    fileInfo.biCompression = BI_RGB;
    fileInfo.biSizeImage = width * height * (bitsPerPixel / 8);
    fileInfo.biXPelsPerMeter = 2400;
    fileInfo.biYPelsPerMeter = 2400;
    fileInfo.biClrImportant = 0;
    fileInfo.biClrUsed = 0;

    WriteFile(file, &fileHeader, sizeof(fileHeader), &writePosn, NULL);

    WriteFile(file, &fileInfo, sizeof(fileInfo), &writePosn, NULL);

    WriteFile(file, bitmapData, bitmapDataLength, &writePosn, NULL);

    CloseHandle(file);
}

int fileExists(TCHAR * file)
{
    WIN32_FIND_DATA FindFileData;
    HANDLE handle = FindFirstFile(file, &FindFileData);
    int found = handle != INVALID_HANDLE_VALUE;
    if(found)
    {
        FindClose(handle);
    }
    return found;
}

int _tmain(int argc, _TCHAR* argv[])
{
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    CWebcamAccess wa;
    wa.Initialize();
    wa.PrepareDevice();
    unsigned int width, height;
    wa.GetImageSizes(width, height);

    int buflen = width * height * 4;
    BYTE *buf = new BYTE[buflen];

    // bmps are stored bottom-up, GetImageData always retrieves top-down, so pass pointer to last line
    // and set negative stride
    wa.GetImageData(buf + (height - 1) * width * 4, width * -4);

    TCHAR filename[100] = L"sample.bmp";
    int i = 0;
    while(fileExists(filename))
    {
        _stprintf(filename, L"sample%d.bmp", i++);
    }

    CreateBitmapFile(filename, width, height, 32, buf, buflen);

    delete[] buf;

	return 0;
}

