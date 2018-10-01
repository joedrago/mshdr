#define _CRT_SECURE_NO_WARNINGS

#include "image.h"

#include <stdint.h>
#include <string.h>

#include <vector>

// ---------------------------------------------------------------------------
// Adapted from public MSDN docs

typedef struct BITMAPFILEHEADER
{
    // uint16_t bfType; // Commented out because the alignment of this struct is tragic. I'll just deal with this manually.
    uint32_t bfSize;
    uint16_t bfReserved1;
    uint16_t bfReserved2;
    uint32_t bfOffBits;
} BITMAPFILEHEADER;

typedef int32_t FXPT2DOT30;
typedef struct CIEXYZ
{
    FXPT2DOT30 ciexyzX;
    FXPT2DOT30 ciexyzY;
    FXPT2DOT30 ciexyzZ;
} CIEXYZ;

typedef struct CIEXYZTRIPLE
{
    CIEXYZ ciexyzRed;
    CIEXYZ ciexyzGreen;
    CIEXYZ ciexyzBlue;
} CIEXYZTRIPLE;

typedef struct BITMAPV5HEADER
{
    uint32_t bV5Size;
    int32_t bV5Width;
    int32_t bV5Height;
    uint16_t bV5Planes;
    uint16_t bV5BitCount;
    uint32_t bV5Compression;
    uint32_t bV5SizeImage;
    int32_t bV5XPelsPerMeter;
    int32_t bV5YPelsPerMeter;
    uint32_t bV5ClrUsed;
    uint32_t bV5ClrImportant;
    uint32_t bV5RedMask;
    uint32_t bV5GreenMask;
    uint32_t bV5BlueMask;
    uint32_t bV5AlphaMask;
    uint32_t bV5CSType;
    CIEXYZTRIPLE bV5Endpoints;
    uint32_t bV5GammaRed;
    uint32_t bV5GammaGreen;
    uint32_t bV5GammaBlue;
    uint32_t bV5Intent;
    uint32_t bV5ProfileData;
    uint32_t bV5ProfileSize;
    uint32_t bV5Reserved;
} BITMAPV5HEADER;

#define BI_RGB        0
#define BI_BITFIELDS  3

#define LCS_sRGB                'sRGB'
#define LCS_WINDOWS_COLOR_SPACE 'Win ' // Windows default color space
#define PROFILE_EMBEDDED        'MBED'

#define LCS_GM_ABS_COLORIMETRIC 8

Image::Image()
    : width_(0)
    , height_(0)
    , depth_(0)
    , pixels_(nullptr)
{
}

Image::~Image()
{
    if (pixels_)
        delete [] pixels_;
}

// figures out the depth of the channel mask, then returns max(channelDepth, currentDepth) and the right shift
static int maskDepth(uint32_t mask, int currentDepth, int * channelDepth, int * rightShift)
{
    int depth = 0;
    *channelDepth = 0;
    *rightShift = 0;

    if (!mask)
        return currentDepth;

    // Shift out all trailing 0s
    while (!(mask & 1)) {
        mask >>= 1;
        ++(*rightShift);
    }

    // Find the first unset bit
    while (mask & (1 << (depth))) {
        ++depth;
    }
    *channelDepth = depth;
    return (depth > currentDepth) ? depth : currentDepth;
}

bool Image::load(const char * filename)
{
    std::vector<unsigned char> fileContents;
    {
        FILE * f = fopen(filename, "rb");
        if (!f) {
            return false;
        }
        fseek(f, 0, SEEK_END);
        unsigned int fileSize = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (fileSize == 0) {
            fclose(f);
            return false;
        }

        fileContents.resize(fileSize);
        fread(&fileContents[0], fileSize, 1, f);
        fclose(f);
    }

    const uint16_t expectedMagic = 0x4D42; // 'BM'
    uint16_t magic = 0;
    BITMAPFILEHEADER fileHeader;
    BITMAPV5HEADER info;
    int rDepth, gDepth, bDepth, aDepth;
    int rShift, gShift, bShift, aShift;
    int depth = 8;
    int packedPixelBytes = 0;
    uint32_t * packedPixels;
    int pixelCount;

    if (fileContents.size() < (sizeof(magic) + sizeof(fileHeader))) {
        return false;
    }

    memcpy(&magic, &fileContents[0], sizeof(magic));
    if (magic != expectedMagic) {
        return false;
    }

    memcpy(&fileHeader, &fileContents[2], sizeof(fileHeader));
    if (fileHeader.bfSize != fileContents.size()) {
        return false;
    }

    memset(&info, 0, sizeof(info));
    memcpy(&info, &fileContents[sizeof(magic) + sizeof(fileHeader)], 4); // read bV5Size
    if ((info.bV5Size >= fileContents.size()) || (info.bV5Size > sizeof(info))) {
        return false;
    }
    memcpy(&info, &fileContents[2 + sizeof(fileHeader)], info.bV5Size); // read the whole header
    // TODO: Make decisions based on the size? (autodetect V4 or previous)

    if (info.bV5BitCount != 32) {
        return false;
    }

    if (info.bV5Compression == BI_BITFIELDS) {
        depth = maskDepth(info.bV5RedMask, depth, &rDepth, &rShift);
        depth = maskDepth(info.bV5GreenMask, depth, &gDepth, &gShift);
        depth = maskDepth(info.bV5BlueMask, depth, &bDepth, &bShift);
        depth = maskDepth(info.bV5AlphaMask, depth, &aDepth, &aShift);
    } else {
        if (info.bV5Compression != BI_RGB) {
            return false;
        }

        // Assume these masks/depths for BI_RGB
        info.bV5BlueMask = 255 << 0;
        info.bV5GreenMask = 255 << 8;
        info.bV5RedMask = 255 << 16;
        info.bV5AlphaMask = 255 << 24;
        rDepth = gDepth = bDepth = aDepth = 8;
    }

    if ((depth != 8) && (depth != 10)) {
        return false;
    }

    pixelCount = info.bV5Width * info.bV5Height;
    packedPixelBytes = sizeof(uint32_t) * pixelCount;
    if ((fileHeader.bfOffBits + packedPixelBytes) > fileContents.size()) {
        return false;
    }
    packedPixels = new uint32_t[pixelCount];
    memcpy(packedPixels, &fileContents[fileHeader.bfOffBits], packedPixelBytes);

    width_ = info.bV5Width;
    height_= info.bV5Height;
    depth_ = (depth > 8) ? 16 : 8;
    pixels_ = new unsigned char[bytes()];

    if (depth == 10) {
        // 10 bit
        int i;
        int maxSrcChannel = (1 << 10) - 1;
        int maxDstChannel = (1 << 16) - 1;
        for (i = 0; i < pixelCount; ++i) {
            uint8_t * srcPixel = (uint8_t *)&packedPixels[i];
            uint16_t * pixels = (uint16_t *)pixels_;
            uint16_t * dstPixel = &pixels[i * 4];
            dstPixel[0] = (maxDstChannel * ((packedPixels[i] & info.bV5RedMask) >> rShift)) / maxSrcChannel;
            dstPixel[1] = (maxDstChannel * ((packedPixels[i] & info.bV5GreenMask) >> gShift)) / maxSrcChannel;
            dstPixel[2] = (maxDstChannel * ((packedPixels[i] & info.bV5BlueMask) >> bShift)) / maxSrcChannel;
            if (aDepth > 0)
                dstPixel[3] = (packedPixels[i] & info.bV5AlphaMask) >> aShift;
            else
                dstPixel[3] = (1 << depth_) - 1;
        }
    } else {
        // TODO: Implement?
        memset(pixels_, 255, bytes());
    }

    delete [] packedPixels;
    return true;
}
