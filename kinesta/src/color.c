#include "color.h"

color_t color_map(color_t c1, color_t c2, float t)
{
    if (t < 0){
        t = 0;
    }
    if (t > 1){
        t = 1;
    }

    int gradient_r = color_get_r(c2) - color_get_r(c1);
    int gradient_g = color_get_g(c2) - color_get_g(c1);
    int gradient_b = color_get_b(c2) - color_get_b(c1);

    return color_rgb(
        color_get_r(c1) + t * gradient_r,
        color_get_g(c1) + t * gradient_g,
        color_get_b(c1) + t * gradient_b
    );
}

color_t color_mul(color_t c, float multiplier)
{
    return color_rgb(
        color_get_r(c) * multiplier,
        color_get_g(c) * multiplier,
        color_get_b(c) * multiplier
    );
}
