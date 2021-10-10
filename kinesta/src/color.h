#ifndef COLOR_H
#define COLOR_H

#include <stdint.h>

typedef uint32_t color_t;

#define COLOR_CHAN_BITS 10
#define COLOR_CHAN_MAX ((1 << COLOR_CHAN_BITS) - 1)

#define COLOR_RGB(r, g, b) ((r) << (2 * COLOR_CHAN_BITS)) | ((g) << (COLOR_CHAN_BITS)) | (b)

#define COLOR_GET_R(color) (((color) >> (2 * COLOR_CHAN_BITS)) & COLOR_CHAN_MAX)
#define COLOR_GET_G(color) (((color) >> COLOR_CHAN_BITS) & COLOR_CHAN_MAX)
#define COLOR_GET_B(color) ((color) & COLOR_CHAN_MAX)

#endif
