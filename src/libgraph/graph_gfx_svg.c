// SPDX-License-Identifier: GPL-2.0-only

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "graph_gfx.h"

static int graph_gfx_svg_setup(void *ctx, size_t width, size_t height)
{
    graph_gfx_svg_ctx_t *svg_ctx = ctx;
    if (svg_ctx == NULL)
        return -1;

    fprintf(svg_ctx->fp, "<?xml version=\"1.0\"?>\n"
                         "<svg width=\"%dpx\" height=\"%dpx\""
                         " viewBox=\"0 0 %d %d\""
                         " version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\">\n",
                         (int)width, (int)height, (int)width,  (int)height);
    return 0;
}

static int graph_gfx_svg_finish(void *ctx)
{
    graph_gfx_svg_ctx_t *svg_ctx = ctx;
    if(svg_ctx == NULL)
        return -1;

    fprintf(svg_ctx->fp, "</svg>");
    return 0;
}

static void graph_gfx_svg_line(void *ctx, double x0, double y0,
                                          double x1, double y1,
                                          double width, gfx_color_t color)
{
    graph_gfx_svg_ctx_t *svg_ctx = ctx;
    if (svg_ctx == NULL)
        return;

    x0 = round(x0)+0.5;
    y0 = round(y0)+0.5;
    x1 = round(x1)+0.5;
    y1 = round(y1)+0.5;

    fprintf(svg_ctx->fp, "<line x1=\"%g\" y1=\"%g\""
                           " x2=\"%g\" y2=\"%g\""
                           " stroke-width=\"%g\""
                           " stroke=\"rgba(%d,%d,%d,%g)\"/>\n",
                            x0, y0, x1, y1, width,
                            (int)(color.red*255),
                            (int)(color.green*255),
                            (int)(color.blue*255), color.alpha);
}

static void graph_gfx_svg_dashed_line(void *ctx, double x0, double y0,
                                                 double x1, double y1,
                                                 double width, gfx_color_t color,
                                                 double *dash, size_t ndash, double dash_offset)
{
    graph_gfx_svg_ctx_t *svg_ctx = ctx;
    if(svg_ctx == NULL)
        return;

    x0 = round(x0)+0.5;
    y0 = round(y0)+0.5;
    x1 = round(x1)+0.5;
    y1 = round(y1)+0.5;

    fprintf(svg_ctx->fp, "<line x1=\"%g\" y1=\"%g\""
                           " x2=\"%g\" y2=\"%g\""
                           " stroke-width=\"%g\""
                           " stroke=\"rgba(%d,%d,%d,%g)\"",
                            x0, y0, x1, y1, width,
                            (int)(color.red*255),
                            (int)(color.green*255),
                            (int)(color.blue*255), color.alpha);

    if(ndash > 0) {
        fprintf(svg_ctx->fp, " stroke-dasharray=\"");
        for (size_t i=0; i < ndash; i++) {
            if (i != 0)
                 fprintf(svg_ctx->fp, ",");
            fprintf(svg_ctx->fp, "%g", dash[i]);
        }
        fprintf(svg_ctx->fp, "\"");
        if (dash_offset > 0)
            fprintf(svg_ctx->fp, " stroke-dashoffset=\"%g\"", dash_offset);
    }
    fprintf(svg_ctx->fp, "/>\n");
}

static void graph_gfx_svg_rectangle(void *ctx, double x0, double y0,
                                               double x1, double y1,
                                               double width, __attribute__((unused)) char *style)
{
    graph_gfx_svg_ctx_t *svg_ctx = ctx;
    if(svg_ctx == NULL)
        return;

    double rwidth = fabs(x1-x0);
    double rheight = fabs(y1-y0);

    fprintf(svg_ctx->fp, "<rect x=\"%g\" y=\"%g\" "
                              " width=\"%g\" height=\"%g\" "
                              " stroke-width=\"%g\" "
                              " fill=\"none\"/>\n",
                           round(x0)+0.5, round(y0-rheight)+0.5,
                           rwidth, rheight,
                           width);
}

static void graph_gfx_svg_add_rect_fadey(void *ctx, double x0,double y0,
                                                    double x1,double y1,
                                                    double py,
                                                    gfx_color_t color1,
                                                    gfx_color_t color2,
                                                    double height)
{
    graph_gfx_svg_ctx_t *svg_ctx = ctx;
    if(svg_ctx == NULL)
        return;

    x0 = round(x0)+0.5;
    y0 = round(y0)+0.5;
    x1 = round(x1)+0.5;
    y1 = round(y1)+0.5;
    // FIXME area fit

    static int num = 0;
    num++;

    double gx0 = x0;
    double gy0 = y0;
    double gx1 = x1;
    double gy1 = y1;

    if (height < 0) {
        gy1 = round(y1+height)+0.5;
    } else if (height > 0) {
        gy0 = round((y1+py)/2+height)+0.5;
        gy1 = round((y1+py)/2)+0.5;
    } else {
        gy1 = round((y1+py)/2) +0.5;
    }

    fprintf(svg_ctx->fp, "<defs>\n"
                     "<linearGradient id=\"rectfadey%d\" x1=\"%g\" y1=\"%g\" x2=\"%g\" y2=\"%g\">\n"
                     "<stop offset=\"0%%\" style=\"stop-color:rgba(%d,%d,%d,%g);stop-opacity:1\"/>\n"
                     "<stop offset=\"100%%\" style=\"stop-color:rgba(%d,%d,%d,%g);stop-opacity:1\"/>\n"
                     "</linearGradient>\n"
                     "</defs>\n",
                     num, gx0, gy0, gx1, gy1,
                     (int)(color1.red*255),
                     (int)(color1.green*255),
                     (int)(color1.blue*255), color1.alpha,
                     (int)(color2.red*255),
                     (int)(color2.green*255),
                     (int)(color2.blue*255), color2.alpha);

    fprintf(svg_ctx->fp, "<path "
                     " fill=\"url(#rectfadey%d)\" "
                     " stroke-linecap=\"round\" "
                     " stroke-linejoin=\"round\" "
                     " d=\"L%g,%g L%g,%g L%g,%g L%g,%g Z\"/>\n",
                     num,
                     x0, y0, x0, y1, x1, y1, x1, y0);
}

static void graph_gfx_svg_new_path(void *ctx, double width, gfx_color_t color)
{
    graph_gfx_svg_ctx_t *svg_ctx = ctx;
    if (svg_ctx == NULL)
        return;

    fprintf(svg_ctx->fp, "<path "
            " fill=\"none\" "
            " stroke\"rgba(%d,%d,%d,%g)\""
            " stroke-width=\"%g\""
            " stroke-linecap=\"round\""
            " stroke-linejoin=\"round\""
            " d=\"",
            (int)(color.red*255), (int)(color.green*255), (int)(color.blue*255), color.alpha, width);
}

static void graph_gfx_svg_new_dashed_path(void *ctx, double width, gfx_color_t color,
                                                     double *dash, size_t ndash, double dash_offset)
{
    graph_gfx_svg_ctx_t *svg_ctx = ctx;
    if (svg_ctx == NULL)
        return;

    fprintf(svg_ctx->fp, "<path fill=\"none\""
            " stroke\"rgba(%d,%d,%d,%g)\""
            " stroke-width=\"%g\""
            " stroke-linecap=\"round\""
            " stroke-linejoin=\"round\"",
            (int)(color.red*255), (int)(color.green*255), (int)(color.blue*255), color.alpha, width);

    if (ndash > 0) {
        fprintf(svg_ctx->fp, " stroke-dasharray=\"");
        for (size_t i=0; i < ndash; i++) {
            if (i != 0)
                 fprintf(svg_ctx->fp, ",");
            fprintf(svg_ctx->fp, "%g", dash[i]);
        }
        fprintf(svg_ctx->fp, "\"");
        if (dash_offset > 0)
            fprintf(svg_ctx->fp, " stroke-dashoffset=\"%g\"", dash_offset);
    }

    fprintf(svg_ctx->fp, " d=\"");
}

static void graph_gfx_svg_new_area(void *ctx, double x0, double y0,
                                              double x1, double y1,
                                              double x2, double y2, gfx_color_t color)
{
    graph_gfx_svg_ctx_t *svg_ctx = ctx;
    if (svg_ctx == NULL)
        return;

    x0 = round(x0)+0.5;
    y0 = round(y0)+0.5;
    x1 = round(x1)+0.5;
    y1 = round(y1)+0.5;
    x2 = round(x2)+0.5;
    y2 = round(y2)+0.5;

// " stroke-width=\"%g\"", this.path_width);
    fprintf(svg_ctx->fp, "<path fill=\"rgba(%d,%d,%d,%g)\""
                     " stroke=\"none\""
                     " d=\"M%g,%g L%g,%g L%g,%g",
                      (int)(color.red*255),
                      (int)(color.green*255),
                      (int)(color.blue*255), color.alpha,
                      x0, y0, x1, y1, x2, y2);
}

static void graph_gfx_svg_close_path(void *ctx)
{
    graph_gfx_svg_ctx_t *svg_ctx = ctx;
    if (svg_ctx == NULL)
        return;

    fprintf(svg_ctx->fp, " Z\"/>\n");
}

static void graph_gfx_svg_move_to(void *ctx, double x, double y)
{
    graph_gfx_svg_ctx_t *svg_ctx = ctx;
    if (svg_ctx == NULL)
        return;

    x = round(x)+0.5;
    y = round(y)+0.5;

    fprintf(svg_ctx->fp, " M%g,%g", x, y);
}

static void graph_gfx_svg_line_to(void *ctx, double x, double y)
{
    graph_gfx_svg_ctx_t *svg_ctx = ctx;
    if (svg_ctx == NULL)
        return;

    x = round(x)+0.5;
    y = round(y)+0.5;

    fprintf(svg_ctx->fp, " L%g,%g", x, y);
}

static void graph_gfx_svg_add_point(void *ctx, double x, double y)
{
    graph_gfx_svg_ctx_t *svg_ctx = ctx;
    if (svg_ctx == NULL)
        return;

    x = round(x)+0.5;
    y = round(y)+0.5;

    fprintf(svg_ctx->fp, " L%g,%g", x, y);
}

static void graph_gfx_svg_text(void *ctx, double x, double y,
                                          gfx_color_t color,
                                          char *font_family, double font_size,
                                          __attribute__((unused)) double tabwidth, double angle,
                                          gfx_h_align_t h_align, gfx_v_align_t v_align,
                                          const char *text)
{
    graph_gfx_svg_ctx_t *svg_ctx = ctx;
    if (svg_ctx == NULL)
        return;

    x = round(x)+0.5;

    char *text_h_align;
    switch (h_align) {
        case GFX_H_NULL:
            text_h_align = "middle";
            break;
        case GFX_H_LEFT:
            text_h_align = "start";
            break;
        case GFX_H_RIGHT:
            text_h_align = "end";
            break;
        case GFX_H_CENTER:
            text_h_align = "middle";
            break;
        default:
            text_h_align = "";
            break;
    }
#if 0
    char *text_v_align = "";
    switch (v_align) {
        case GFX_V_NULL:
            text_v_align = "center";
            break;
        case GFX_V_TOP:
            text_v_align = "top";
            break;
        case GFX_V_BOTTOM:
            text_v_align = "bottom";
            break;
        case GFX_V_CENTER:
            text_v_align = "center";
            break;
    }
    alignment-baseline
#endif
    switch (v_align) {
        case GFX_V_NULL:
            y = round(y)+0.5;
            break;
        case GFX_V_TOP:
            y = round(y + font_size/2.0) + 0.5;
            break;
        case GFX_V_BOTTOM:
            y = round(y - font_size/6.0) + 0.5;
            break;
        case GFX_V_CENTER:
            y = round(y + font_size/4.0) + 0.5;
            break;
    }


    fprintf(svg_ctx->fp, "<text x=\"%g\" y=\"%g\""
                         " transform=\"rotate(%g %g %g)\""
                         " fill=\"rgba(%d,%d,%d,%g)\""
                         " stroke=\"none\""
                         " font-family=\"%s\""
                         " font-size=\"%gpx\""
                         " text-anchor=\"%s\">%s</text>\n",
                         x, y, -angle, x, y,
                         (int)(color.red*255),
                         (int)(color.green*255),
                         (int)(color.blue*255), color.alpha,
                         font_family, font_size, text_h_align, text);
}

static double graph_gfx_svg_get_text_width(__attribute__((unused)) void *ctx,
                                           __attribute__((unused)) double start,
                                           __attribute__((unused)) char *font_family, double font_size,
                                           __attribute__((unused)) double tabwidth, char *text)
{
   // FIXME
   return strlen(text)*font_size;
}

graph_gfx_t graph_gfx_svg = {
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
};

