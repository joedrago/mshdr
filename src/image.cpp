#define _CRT_SECURE_NO_WARNINGS

#include "image.h"

#include "png.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <vector>

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

struct gfx_read_png_info
{
    unsigned char * curr;
    png_size_t remaining;
};
static void gfx_read_png(png_structp read, png_bytep data, png_size_t length)
{
    gfx_read_png_info * info = (gfx_read_png_info *)png_get_io_ptr(read);
    assert(info->remaining >= length);
    memcpy(data, info->curr, length);
    info->curr += length;
    info->remaining -= length;
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

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (png == NULL) {
        return false;
    }

    png_infop info = png_create_info_struct(png);
    if (info == NULL) {
        png_destroy_read_struct(&png, NULL, NULL);
        return false;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        return false;
    }

    gfx_read_png_info readInfo;
    readInfo.curr = &fileContents[0];
    readInfo.remaining = fileContents.size();
    png_set_read_fn(png, &readInfo, gfx_read_png);
    png_read_info(png, info);

    int width      = png_get_image_width(png, info);
    int height     = png_get_image_height(png, info);
    int color_type = png_get_color_type(png, info);
    int bit_depth  = png_get_bit_depth(png, info);

    png_set_swap(png);

    if (color_type == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(png);

    // PNG_COLOR_TYPE_GRAY_ALPHA is always 8 or 16bit depth.
    if (( color_type == PNG_COLOR_TYPE_GRAY) && ( bit_depth < 8))
        png_set_expand_gray_1_2_4_to_8(png);

    if (png_get_valid(png, info, PNG_INFO_tRNS))
        png_set_tRNS_to_alpha(png);

    // These color_type don't have an alpha channel then fill it with 0xff.
    if (( color_type == PNG_COLOR_TYPE_RGB) ||
        ( color_type == PNG_COLOR_TYPE_GRAY) ||
        ( color_type == PNG_COLOR_TYPE_PALETTE))
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);

    if (( color_type == PNG_COLOR_TYPE_GRAY) ||
        ( color_type == PNG_COLOR_TYPE_GRAY_ALPHA))
        png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    png_bytep * row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * height);
    int rowBytes = (int)png_get_rowbytes(png, info);
    for (int y = 0; y < height; y++) {
        row_pointers[y] = (png_byte *)malloc(rowBytes);
    }

    png_read_image(png, row_pointers);

    width_ = width;
    height_= height;
    depth_ = bit_depth;
    pixels_ = new unsigned char[bytes()];
    int pitch = width_ * bpp();
    for (int y = 0; y < height; y++) {
        memcpy(pixels_ + (y * pitch), row_pointers[y], rowBytes);
    }

    png_destroy_read_struct(&png, &info, NULL);
    for (int y = 0; y < height; y++) {
        free(row_pointers[y]);
    }
    free(row_pointers);
    return true;
}
