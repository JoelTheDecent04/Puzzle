#include <Windows.h>
#include <stdio.h>
#include <stdint.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

int main(int ArgCount, char** Args)
{
    if (ArgCount < 3)
    {
        printf("Usage: %s [Font Path] [Output Path]\n", Args[0]);
    }
    
    char* Path = Args[1];
    HANDLE File = CreateFileA(Path, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
    DWORD FileSize = GetFileSize(File, 0);
    void* Data = malloc(FileSize);
    ReadFile(File, Data, FileSize, 0, 0);
    
    int32_t Width = 256, Height = 256;
    
    uint8_t* STBPixels = malloc(Width * Height);
    
    stbtt_bakedchar BakedChars[128];
    stbtt_BakeFontBitmap(Data, 0, 48.0f, STBPixels, Width, Height, 0, 128, BakedChars);
    
    uint32_t* BMPPixels = malloc(4 * Width * Height);
    
    uint8_t* Src = STBPixels + Width * (Height - 1);
    uint32_t* Dest = BMPPixels;
    for (int Y = 0; Y < Height; Y++)
    {
        for (int X = 0; X < Width; X++)
        {
            uint32_t Val = Src[X];
            Dest[X] = (Val << 24 | Val << 16 | Val << 8 | Val);
        }
        Dest += Width;
        Src -= Width;
    }
    
    char* OutPath = Args[2];
    HANDLE OutFile = CreateFileA(OutPath, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    
    BITMAPFILEHEADER Header = {0};
    BITMAPINFOHEADER Info = {0};
    
    Header.bfType = 0x4d42;
    Header.bfSize = sizeof(Header) + sizeof(Info) + (4 * Width * Height);
    Header.bfOffBits = sizeof(Header) + sizeof(Info);
    
    Info.biSize = sizeof(Info);
    Info.biWidth = Width;
    Info.biHeight = Height;
    Info.biPlanes = 1;
    Info.biBitCount = 32;
    Info.biCompression = BI_RGB;
    Info.biSizeImage = 0;
    Info.biXPelsPerMeter = 1000;
    Info.biXPelsPerMeter = 1000;
    Info.biClrUsed = 0;
    Info.biClrImportant = 0;
    
    WriteFile(OutFile, &Header, sizeof(Header), 0, 0);
    WriteFile(OutFile, &Info, sizeof(Info), 0, 0);
    WriteFile(OutFile, BMPPixels, (4 * Width * Height), 0, 0);
}
