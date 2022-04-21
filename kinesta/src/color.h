#ifndef COLOR_H
#define COLOR_H

#include <stdint.h>

typedef uint32_t color_t;

#define COLOR_CHAN_BITS 10
#define COLOR_CHAN_HIGHEST_BIT_MASK (1 << (COLOR_CHAN_BITS - 1))
#define COLOR_CHAN_MAX ((1 << COLOR_CHAN_BITS) - 1)

/* Constructor */
#define COLOR_RGB(r, g, b) ((r) << (2 * COLOR_CHAN_BITS)) | ((g) << (COLOR_CHAN_BITS)) | (b)

/* Accessors */
#define COLOR_GET_R(color) (((color) >> (2 * COLOR_CHAN_BITS)) & COLOR_CHAN_MAX)
#define COLOR_GET_G(color) (((color) >> COLOR_CHAN_BITS) & COLOR_CHAN_MAX)
#define COLOR_GET_B(color) ((color) & COLOR_CHAN_MAX)

/* Predefined palette of colors */
#define COLOR_RED   COLOR_RGB(COLOR_CHAN_MAX, 0, 0)
#define COLOR_GREEN COLOR_RGB(0, COLOR_CHAN_MAX, 0)
#define COLOR_BLUE  COLOR_RGB(0, 0, COLOR_CHAN_MAX)
#define COLOR_WHITE (COLOR_RED | COLOR_GREEN | COLOR_BLUE)

/**
 * @brief      Map value between 2 colors
 * @param[in]  c1    The first color
 * @param[in]  c2    The second color
 * @param[in]  t     The value to map (must be between 0 and 1)
 * @return     The resulting color
 */
color_t color_map(color_t c1, color_t c2, float t);

#endif
