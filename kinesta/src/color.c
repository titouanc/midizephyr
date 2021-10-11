#include "color.h"

color_t color_map(color_t c1, color_t c2, float t)
{
    if (t < 0){
        t = 0;
    }
    if (t > 1){
        t = 1;
    }

    int gradient_r = COLOR_GET_R(c2) - COLOR_GET_R(c1);
    int gradient_g = COLOR_GET_G(c2) - COLOR_GET_G(c1);
    int gradient_b = COLOR_GET_B(c2) - COLOR_GET_B(c1);

    return COLOR_RGB(
        (int)(COLOR_GET_R(c1) + t * gradient_r),
        (int)(COLOR_GET_G(c1) + t * gradient_g),
        (int)(COLOR_GET_B(c1) + t * gradient_b)
    );
}
