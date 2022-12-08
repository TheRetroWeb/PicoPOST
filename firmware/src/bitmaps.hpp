#ifndef PICOPOST_BITMAP_HPP
#define PICOPOST_BITMAP_HPP

#include <stdint.h>

#define MAX_BMP_IMAGE 4096

typedef struct __picopost_bitmap {
    uint width;
    uint height;
    uint8_t image[MAX_BMP_IMAGE];
} Bitmap;

static Bitmap bmp_arrowUp = {
    8, 8,
    { 0x08, 0x1C, 0x1C, 0x1C, 0x3E, 0x3E, 0x3E, 0x7F }
};

static Bitmap bmp_arrowDown = {
    8, 8,
    { 0x7F, 0x3E, 0x3E, 0x3E, 0x1C, 0x1C, 0x1C, 0x08 }
};

#endif // PICOPOST_BITMAP_HPP