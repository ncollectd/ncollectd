// SPDX-License-Identifier: GPL-2.0-only

#include <stdlib.h>
#include <stdint.h>

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} rgba_t;

#define COLOR_ALPHA_OPAQUE 0
#define COLOR_ALPHA_TRASPARENT 127

typedef struct {
    size_t width;
    size_t height;
    rgba_t **image;
} image_t;

void image_free(image_t *img)
{
    if (img->image != NULL) {
        free(img->image[0]);
        free(img->image);
    }
    free(img);
}

image_t *image_create(size_t width, size_t height)
{
    image_t *img = calloc(1, sizeof(*img));
    if (img == NULL)
        return NULL;

    img->width = width;
    img->height = height;

    img->image = calloc(height, sizeof(*img->image));
    if (img->image == NULL) {
        image_free(img);
        return NULL;
    }

    rgba_t *buffer = calloc(height * width, sizeof(*buffer));
    if (buffer == NULL) {
        image_free(img);
        return NULL;
    }

    for (size_t x = 0; x < height; x++) {
        img->image[x] = buffer + (x * width);
    }

    return 0;
}

static void image_setpixel_alpha(image_t *img, int x, int y, rgba_t color)
{
    rgba_t color_dst = img->image[x][y];
    unsigned int weight_src = 255 - color.a;
    unsigned int weight_dst = (255 - color_dst.a) * color.a / 255;
    unsigned int weight_tot = weight_src + weight_dst;

    color.a = color.a * color_dst.a / 255;
    color.r = (color.r * weight_src + color_dst.r * weight_dst) / weight_tot;
    color.g = (color.g * weight_src + color_dst.g * weight_dst) / weight_tot;
    color.b = (color.b * weight_src + color_dst.b * weight_dst) / weight_tot;

    img->image[x][y] = color;
}

static void image_setpixel(image_t *img, int x, int y, rgba_t color)
{
    if (color.a == 0) {
        img->image[x][y] = color;
        return;
    }

    if (color.a == 255)
        return;

    rgba_t color_dst = img->image[x][y];
    if (color_dst.a == 255) {
        img->image[x][y] = color;
        return;
    }

    image_setpixel_alpha(img, x, y, color);
}

#if 0
int image_clip(image_t *canvas)
{

    return 0;
}
#endif

int image_vline(image_t *img, double x, double y0, double y1, rgba_t color)
{
// thick
    if (y1 < y0) {
        int y = y0;
        y0 = y1;
        y1 = y;
    }

    for (; y0 <= y1; y0++) {
        image_setpixel(img, x, y0, color);
    }

    return 0;
}

int image_hline(image_t *img, double y, double x0, double x1, rgba_t color)
{
// thick
    if (x1 < x0) {
        int x = x0;
        x0 = x1;
        x1 = x;
    }

    for (; x0 <= x1; x0++) {
        image_setpixel(img, x0, y, color);
    }

    return 0;
}

#if 0
int image_line(image_t *canvas, double x0, double y0, double x1, double y1, double width, rgba_t color)
{

    long dx = x2 - x0;
    long dy = y2 - y0;

    if ((dx == 0) && (dy == 0)) {
        image_setpixel(canvas, x0, y0, color);
        return;
    }

    double ag = fabs(abs((int)dy) < abs((int)dx) ? cos(atan2(dy, dx)) : sin(atan2(dy, dx)));


    int wid;
    if (ag != 0) {
        wid = thick / ag;
    } else {
        wid = 1;
    }
    if (wid == 0) {
        wid = 1;
    }

    if (dx == 0) {
        image_vline(canvas, x0, y0, y1, col);
        return;
    }
    if (dy == 0) {
        image_hline(im, y0, x0, x1, col);
        return;
    }


    if (abs((int)dx) > abs((int)dy)) {
        if (dx < 0) {
            tmp = x1;
            x1 = x2;
            x2 = tmp;
            tmp = y1;
            y1 = y2;
            y2 = tmp;
            dx = x2 - x1;
            dy = y2 - y1;
        }
        y = y1;
        inc = (dy * 65536) / dx;
        frac = 0;
        /* TBB: set the last pixel for consistency (<=) */
        for (x = x1 ; x <= x2 ; x++) {
            wstart = y - wid / 2;
            for (w = wstart; w < wstart + wid; w++) {
                gdImageSetAAPixelColor(im, x , w , col , (frac >> 8) & 0xFF);
                gdImageSetAAPixelColor(im, x , w + 1 , col, (~frac >> 8) & 0xFF);
            }
            frac += inc;
            if (frac >= 65536) {
                frac -= 65536;
                y++;
            } else if (frac < 0) {
                frac += 65536;
                y--;
            }
        }
    } else {
        if (dy < 0) {
            tmp = x1;
            x1 = x2;
            x2 = tmp;
            tmp = y1;
            y1 = y2;
            y2 = tmp;
            dx = x2 - x1;
            dy = y2 - y1;
        }
        x = x1;
        inc = (dx * 65536) / dy;
        frac = 0;
        /* TBB: set the last pixel for consistency (<=) */
        for (y = y1 ; y <= y2 ; y++) {
            wstart = x - wid / 2;
            for (w = wstart; w < wstart + wid; w++) {
                gdImageSetAAPixelColor(im, w , y  , col, (frac >> 8) & 0xFF);
                gdImageSetAAPixelColor(im, w + 1, y, col, (~frac >> 8) & 0xFF);
            }
            frac += inc;
            if (frac >= 65536) {
                frac -= 65536;
                x++;
            } else if (frac < 0) {
                frac += 65536;
                x--;
            }
        }
    }

}

int image_rectangle(image_t *canvas, double x0, double y0, double x1, double y1, double width, rgba_t color)
{
    int thick = im->thick;

    if (x1 == x2 && y1 == y2 && thick == 1) {
        gdImageSetPixel(im, x1, y1, color);
        return;
    }

    if (y2 < y1) {
        int t = y1;
        y1 = y2;
        y2 = t;
    }

    if (x2 < x1) {
        int t = x1;
        x1 = x2;
        x2 = t;
    }

    if (thick > 1) {
        int cx, cy, x1ul, y1ul, x2lr, y2lr;
        int half = thick >> 1;
        x1ul = x1 - half;
        y1ul = y1 - half;

        x2lr = x2 + half;
        y2lr = y2 + half;

        cy = y1ul + thick;
        while (cy-- > y1ul) {
            cx = x1ul - 1;
            while (cx++ < x2lr) {
                gdImageSetPixel(im, cx, cy, color);
            }
        }

        cy = y2lr - thick;
        while (cy++ < y2lr) {
            cx = x1ul - 1;
            while (cx++ < x2lr) {
                gdImageSetPixel(im, cx, cy, color);
            }
        }

        cy = y1ul + thick - 1;
        while (cy++ < y2lr -thick) {
            cx = x1ul - 1;
            while (cx++ < x1ul + thick) {
                gdImageSetPixel(im, cx, cy, color);
            }
        }

        cy = y1ul + thick - 1;
        while (cy++ < y2lr -thick) {
            cx = x2lr - thick - 1;
            while (cx++ < x2lr) {
                gdImageSetPixel(im, cx, cy, color);
            }
        }

        return;
    } else {
        if (x1 == x2 || y1 == y2) {
            gdImageLine(im, x1, y1, x2, y2, color);
        } else {
            gdImageLine(im, x1, y1, x2, y1, color);
            gdImageLine(im, x1, y2, x2, y2, color);
            gdImageLine(im, x1, y1 + 1, x1, y2 - 1, color);
            gdImageLine(im, x2, y1 + 1, x2, y2 - 1, color);
        }
    }
}

static void _gdImageFilledHRectangle (gdImagePtr im, int x1, int y1, int x2, int y2,
        int color)
{
    int x, y;

    if (x1 == x2 && y1 == y2) {
        gdImageSetPixel(im, x1, y1, color);
        return;
    }

    if (x1 > x2) {
        x = x1;
        x1 = x2;
        x2 = x;
    }

    if (y1 > y2) {
        y = y1;
        y1 = y2;
        y2 = y;
    }

    if (x1 < 0) {
        x1 = 0;
    }

    if (x2 >= gdImageSX(im)) {
        x2 = gdImageSX(im) - 1;
    }

    if (y1 < 0) {
        y1 = 0;
    }

    if (y2 >= gdImageSY(im)) {
        y2 = gdImageSY(im) - 1;
    }

    for (x = x1; (x <= x2); x++) {
        for (y = y1; (y <= y2); y++) {
            gdImageSetPixel (im, x, y, color);
        }
    }
}

static void _gdImageFilledVRectangle (gdImagePtr im, int x1, int y1, int x2, int y2,
        int color)
{
    int x, y;

    if (x1 == x2 && y1 == y2) {
        gdImageSetPixel(im, x1, y1, color);
        return;
    }

    if (x1 > x2) {
        x = x1;
        x1 = x2;
        x2 = x;
    }

    if (y1 > y2) {
        y = y1;
        y1 = y2;
        y2 = y;
    }

    if (x1 < 0) {
        x1 = 0;
    }

    if (x2 >= gdImageSX(im)) {
        x2 = gdImageSX(im) - 1;
    }

    if (y1 < 0) {
        y1 = 0;
    }

    if (y2 >= gdImageSY(im)) {
        y2 = gdImageSY(im) - 1;
    }

    for (y = y1; (y <= y2); y++) {
        for (x = x1; (x <= x2); x++) {
            gdImageSetPixel (im, x, y, color);
        }
    }
}

/*
    Function: gdImageFilledRectangle
*/
BGD_DECLARE(void) gdImageFilledRectangle (gdImagePtr im, int x1, int y1, int x2, int y2,
        int color)
{
    _gdImageFilledVRectangle(im, x1, y1, x2, y2, color);
}

#endif

#if 0
    .setup           = graph_gfx_svg_setup,
    .finish          = graph_gfx_svg_finish,
    .line            = graph_gfx_svg_line,
    .dashed_line     = graph_gfx_svg_dashed_line,
    .rectangle       = graph_gfx_svg_rectangle,
    .add_rect_fadey  = graph_gfx_svg_add_rect_fadey,
    .new_path        = graph_gfx_svg_new_path,
    .new_dashed_path = graph_gfx_svg_new_dashed_path,
    .new_area        = graph_gfx_svg_new_area,
    .close_path      = graph_gfx_svg_close_path,
    .move_to         = graph_gfx_svg_move_to,
    .line_to         = graph_gfx_svg_line_to,
    .add_point       = graph_gfx_svg_add_point,
    .text            = graph_gfx_svg_text,
    .get_text_width  = graph_gfx_svg_get_text_width,
#endif
