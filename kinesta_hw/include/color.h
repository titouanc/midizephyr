#ifndef COLOR_H
#define COLOR_H

#include <stdint.h>
#include <sys/util.h>

typedef uint32_t color_t;

typedef enum {
    CHANNEL_RED   = 0,
    CHANNEL_GREEN = 1,
    CHANNEL_BLUE  = 2,
} color_channel_t;

#define COLOR_N_CHANS 3
#define COLOR_CHAN_BITS 10
#define COLOR_CHAN_HIGHEST_BIT_MASK (1 << (COLOR_CHAN_BITS - 1))
#define COLOR_CHAN_MAX ((1 << COLOR_CHAN_BITS) - 1)

/* Constructors */
static inline color_t color_rgb(unsigned r, unsigned g, unsigned b)
{
    r = CLAMP(r, 0, COLOR_CHAN_MAX);
    g = CLAMP(g, 0, COLOR_CHAN_MAX);
    b = CLAMP(b, 0, COLOR_CHAN_MAX);
    return (r << (2*COLOR_CHAN_BITS)) | (g << COLOR_CHAN_BITS) | b;
}

static inline color_t color_rgbf(float r, float g, float b)
{
    return color_rgb(r*COLOR_CHAN_MAX, g*COLOR_CHAN_MAX, b*COLOR_CHAN_MAX);
}

/* Accessors */
static inline unsigned color_get_r(color_t color)
{
    return (color >> (2*COLOR_CHAN_BITS)) & COLOR_CHAN_MAX;
}

static inline unsigned color_get_g(color_t color)
{
    return (color >> COLOR_CHAN_BITS) & COLOR_CHAN_MAX;
}

static inline unsigned color_get_b(color_t color)
{
    return color & COLOR_CHAN_MAX;
}

static inline void color_get_u8(color_t color, uint8_t *r, uint8_t *g, uint8_t *b)
{
    *r = color_get_r(color) >> (COLOR_CHAN_BITS - 8);
    *g = color_get_g(color) >> (COLOR_CHAN_BITS - 8);
    *b = color_get_b(color) >> (COLOR_CHAN_BITS - 8);
}

/* Predefined palette of colors */
#define COLOR_RED     color_rgbf(1, 0, 0)
#define COLOR_GREEN   color_rgbf(0, 1, 0)
#define COLOR_BLUE    color_rgbf(0, 0, 1)
#define COLOR_YELLOW  color_rgbf(1, 1, 0)
#define COLOR_MAGENTA color_rgbf(1, 0, 1)
#define COLOR_CYAN    color_rgbf(0, 1, 1)
#define COLOR_WHITE   color_rgbf(1, 1, 1)

/**
 * @brief      Map a value onto a bicolor gradient
 * @param[in]  c1    The first color
 * @param[in]  c2    The second color
 * @param[in]  t     The value to map (in range 0...1)
 * @return     The resulting color
 */
static inline color_t color_map(color_t c1, color_t c2, float t)
{
    t = CLAMP(t, 0, 1);

    int gradient_r = color_get_r(c2) - color_get_r(c1);
    int gradient_g = color_get_g(c2) - color_get_g(c1);
    int gradient_b = color_get_b(c2) - color_get_b(c1);

    return color_rgb(
        color_get_r(c1) + t * gradient_r,
        color_get_g(c1) + t * gradient_g,
        color_get_b(c1) + t * gradient_b
    );
}


/**
 * @brief      Multiply a color by a scalar
 * @param[in]  c           The color
 * @param[in]  multiplier  The scalar multiplier. >1 to ligthen, <1 to darken
 * @return     The scaled color
 */
static inline color_t color_mul(color_t c, float multiplier)
{
    return color_rgb(
        color_get_r(c) * multiplier,
        color_get_g(c) * multiplier,
        color_get_b(c) * multiplier
    );
}

#endif
