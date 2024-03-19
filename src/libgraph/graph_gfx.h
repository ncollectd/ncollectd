/* SPDX-License-Identifier: GPL-2.0-only */
#pragma once

typedef struct {
    double    red;
    double    green;
    double    blue;
    double    alpha;
} gfx_color_t;

typedef enum {
    GFX_H_NULL = 0,
    GFX_H_LEFT,
    GFX_H_RIGHT,
    GFX_H_CENTER
} gfx_h_align_t;

typedef enum {
    GFX_V_NULL = 0,
    GFX_V_TOP,
    GFX_V_BOTTOM,
    GFX_V_CENTER
} gfx_v_align_t;

typedef int (*graph_gfx_setup_t)(void *ctx, size_t width, size_t height);
typedef int (*graph_gfx_finish_t)(void *ctx);
typedef void (*graph_gfx_line_t)(void *ctx, double x0, double y0,
                                            double x1, double y1,
                                            double width, gfx_color_t color);
typedef void (*graph_gfx_dashed_line_t)(void *ctx, double x0, double y0,
                                                   double x1, double y1,
                                                   double width, gfx_color_t color,
                                                   double *dash, size_t ndash, double dash_offset);
typedef void (*graph_gfx_rectangle_t)(void *ctx, double x0, double y0,
                                                 double x1, double y1,
                                                 double width, char *style);
typedef void (*graph_gfx_add_rect_fadey_t)(void *ctx, double x0,double y0,
                                                      double x1,double y1,
                                                      double py,
                                                      gfx_color_t color1,
                                                      gfx_color_t color2, double height);
typedef void (*graph_gfx_new_path_t)(void *ctx, double width, gfx_color_t color);
typedef void (*graph_gfx_new_dashed_path_t)(void *ctx, double width,
                                                       gfx_color_t color,
                                                       double *dash, size_t ndash, double dash_offset);
typedef void (*graph_gfx_new_area_t)(void *ctx, double x0, double y0,
                                                double x1, double y1,
                                                double x2, double y2, gfx_color_t color);
typedef void (*graph_gfx_close_path_t)(void *ctx);
typedef void (*graph_gfx_move_to_t)(void *ctx, double x, double y);
typedef void (*graph_gfx_line_to_t)(void *ctx, double x, double y);
typedef void (*graph_gfx_add_point_t)(void *ctx, double x, double y);
typedef void (*graph_gfx_text_t)(void *ctx, double x, double y,
                                            gfx_color_t color,
                                            char *font_family, double font_size,
                                            double tabwidth, double angle,
                                            gfx_h_align_t h_align,
                                            gfx_v_align_t v_align,
                                            const char *text);
typedef double (*graph_gfx_get_text_width_t)(void *ctx, double start,
                                                        char *font_family, double font_size,
                                                        double tabwidth, char *text);

typedef struct {
    graph_gfx_setup_t           setup;
    graph_gfx_finish_t          finish;
    graph_gfx_line_t            line;
    graph_gfx_dashed_line_t     dashed_line;
    graph_gfx_rectangle_t       rectangle;
    graph_gfx_add_rect_fadey_t  add_rect_fadey;
    graph_gfx_new_path_t        new_path;
    graph_gfx_new_dashed_path_t new_dashed_path;
    graph_gfx_new_area_t        new_area;
    graph_gfx_close_path_t      close_path;
    graph_gfx_move_to_t         move_to;
    graph_gfx_line_to_t         line_to;
    graph_gfx_add_point_t       add_point;
    graph_gfx_text_t            text;
    graph_gfx_get_text_width_t  get_text_width;
} graph_gfx_t;

typedef struct {
    FILE *fp;
} graph_gfx_svg_ctx_t;

