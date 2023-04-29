#ifndef PICOPOST_BITMAP_HPP
#define PICOPOST_BITMAP_HPP

#include <cstdint>

static const size_t c_maxBmpPayload { 4096 };

struct Bitmap {
    const uint width;
    const uint height;
    const uint8_t image[c_maxBmpPayload];
};

static const Bitmap bmp_arrowUp = {
    8, 8,
    { 0x08, 0x1C, 0x1C, 0x1C, 0x3E, 0x3E, 0x3E, 0x7F }
};

static const Bitmap bmp_arrowDown = {
    8, 8,
    { 0x7F, 0x3E, 0x3E, 0x3E, 0x1C, 0x1C, 0x1C, 0x08 }
};

static const Bitmap bmp_select = {
    8, 8,
    { 0x00, 0x40, 0x40, 0x44, 0x46, 0x7F, 0x06, 0x04 }
};

static const Bitmap bmp_back = {
    8, 8,
    { 0x00, 0x0E, 0x01, 0x21, 0x61, 0xFE, 0x60, 0x20 }
};

#endif // PICOPOST_BITMAP_HPP