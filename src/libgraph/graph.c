// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright by Tobi Oetiker, 1997-2019
// SPDX-FileContributor: Tobi Oetiker

#include "config.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <sys/stat.h>
#include <time.h>
#include <math.h>

#ifdef KERNEL_SOLARIS
#include <ieeefp.h>
#endif

#define LOCALTIME_R(a,b,c) (c ? gmtime_r(a,b) : localtime_r(a,b))
#ifdef _AIX
// FIXME
#define MKTIME(a,b) mktime(a)
#else
#define MKTIME(a,b) (b ? timegm(a) : mktime(a))
#endif

#ifndef max
#define max(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

#define DIM(x) (sizeof(x)/sizeof(x[0]))

#define RRDGRAPH_YLEGEND_ANGLE -90.0

#include "graph_gfx.h"
#include "graph.h"

/* some constant definitions */
#ifndef DEFAULT_FONT
/* there is special code later to pick Cour.ttf when running on windows */
#define DEFAULT_FONT "'DejaVu Sans Mono','Bitstream Vera Sans Mono','monospace','Courier'"
#endif

#pragma GCC diagnostic ignored "-Wformat-nonliteral"

static text_prop_t text_prop[TEXT_PROP_MAX] = {
    [TEXT_PROP_DEFAULT]   = {8.0, DEFAULT_FONT, NULL},
    [TEXT_PROP_TITLE]     = {9.0, DEFAULT_FONT, NULL},
    [TEXT_PROP_AXIS]      = {7.0, DEFAULT_FONT, NULL},
    [TEXT_PROP_UNIT]      = {8.0, DEFAULT_FONT, NULL},
    [TEXT_PROP_LEGEND]    = {8.0, DEFAULT_FONT, NULL},
    [TEXT_PROP_WATERMARK] = {5.5, DEFAULT_FONT, NULL},
};

static xlab_t xlab[] = {
    {      0.0,       0, TMT_SECOND,  1, TMT_SECOND,  5, TMT_SECOND,  1,           0, "%H:%M:%S"},
    {      0.015,     0, TMT_SECOND,  1, TMT_SECOND,  5, TMT_SECOND,  5,           0, "%H:%M:%S"},
    {      0.08,      0, TMT_SECOND,  1, TMT_SECOND,  5, TMT_SECOND, 10,           0, "%H:%M:%S"},
    {      0.15,      0, TMT_SECOND,  5, TMT_SECOND, 15, TMT_SECOND, 30,           0, "%H:%M:%S"},
    {      0.4,       0, TMT_SECOND, 10, TMT_MINUTE,  1, TMT_MINUTE,  1,           0, "%H:%M"   },
    {      0.7,       0, TMT_SECOND, 20, TMT_MINUTE,  1, TMT_MINUTE,  1,           0, "%H:%M"   },
    {      1.0,       0, TMT_SECOND, 30, TMT_MINUTE,  1, TMT_MINUTE,  2,           0, "%H:%M"   },
    {      2.0,       0, TMT_MINUTE,  1, TMT_MINUTE,  5, TMT_MINUTE,  5,           0, "%H:%M"   },
    {      5.0,       0, TMT_MINUTE,  2, TMT_MINUTE, 10, TMT_MINUTE, 10,           0, "%H:%M"   },
    {     10.0,       0, TMT_MINUTE,  5, TMT_MINUTE, 20, TMT_MINUTE, 20,           0, "%H:%M"   },
    {     30.0,       0, TMT_MINUTE, 10, TMT_MINUTE, 30, TMT_HOUR,    1,           0, "%H:%M"   },
    {     60.0,       0, TMT_MINUTE, 30, TMT_HOUR,    1, TMT_HOUR,    2,           0, "%H:%M"   },
    {     60.0, 24*3600, TMT_MINUTE, 30, TMT_HOUR,    1, TMT_HOUR,    3,           0, "%a %H:%M"},
    {    140.0,       0, TMT_HOUR,    1, TMT_HOUR,    2, TMT_HOUR,    4,           0, "%a %H:%M"},
    {    180.0,       0, TMT_HOUR,    1, TMT_HOUR,    3, TMT_HOUR,    6,           0, "%a %H:%M"},
    {    300.0,       0, TMT_HOUR,    2, TMT_HOUR,    6, TMT_HOUR,   12,           0, "%a %H:%M"},
    {    600.0,       0, TMT_HOUR,    6, TMT_DAY,     1, TMT_DAY,     1,     24*3600, "%a %d %b"},
    {   1200.0,       0, TMT_HOUR,    6, TMT_DAY,     1, TMT_DAY,     1,     24*3600, "%d %b"   },
    {   1800.0,       0, TMT_HOUR,   12, TMT_DAY,     1, TMT_DAY,     2,     24*3600, "%a %d %b"},
    {   2400.0,       0, TMT_HOUR,   12, TMT_DAY,     1, TMT_DAY,     2,     24*3600, "%d %b"   },
    {   3600.0,       0, TMT_DAY,     1, TMT_WEEK,    1, TMT_WEEK,    1,   7*24*3600, "Week %V" },
    {  12000.0,       0, TMT_DAY,     1, TMT_MONTH,   1, TMT_MONTH,   1,  30*24*3600, "%B %Y"   },
    {  18000.0,       0, TMT_DAY,     2, TMT_MONTH,   1, TMT_MONTH,   1,  30*24*3600, "%B %Y"   },
    {  23000.0,       0, TMT_WEEK,    1, TMT_MONTH,   1, TMT_MONTH,   1,  30*24*3600, "%b %Y"   },
    {  32000.0,       0, TMT_WEEK,    1, TMT_MONTH,   1, TMT_MONTH,   1,  30*24*3600, "%b '%g"  },
    {  42000.0,       0, TMT_WEEK,    1, TMT_MONTH,   1, TMT_MONTH,   2,  30*24*3600, "%B %Y"   },
    {  52000.0,       0, TMT_WEEK,    1, TMT_MONTH,   1, TMT_MONTH,   2,  30*24*3600, "%b %Y"   },
    {  78000.0,       0, TMT_WEEK,    1, TMT_MONTH,   1, TMT_MONTH,   2,  30*24*3600, "%b '%g"  },
    {  84000.0,       0, TMT_WEEK,    2, TMT_MONTH,   1, TMT_MONTH,   3,  30*24*3600, "%B %Y"   },
    {  94000.0,       0, TMT_WEEK,    2, TMT_MONTH,   1, TMT_MONTH,   3,  30*24*3600, "%b %Y"   },
    { 120000.0,       0, TMT_WEEK,    2, TMT_MONTH,   1, TMT_MONTH,   3,  30*24*3600, "%b '%g"  },
    { 130000.0,       0, TMT_MONTH,   1, TMT_MONTH,   2, TMT_MONTH,   4,           0, "%Y-%m-%d"},
    { 142000.0,       0, TMT_MONTH,   1, TMT_MONTH,   3, TMT_MONTH,   6,           0, "%Y-%m-%d"},
    { 220000.0,       0, TMT_MONTH,   1, TMT_MONTH,   6, TMT_MONTH,  12,           0, "%Y-%m-%d"},
    { 400000.0,       0, TMT_MONTH,   2, TMT_MONTH,  12, TMT_MONTH,  12, 365*24*3600, "%Y"      },
    { 800000.0,       0, TMT_MONTH,   4, TMT_MONTH,  12, TMT_MONTH,  24, 365*24*3600, "%Y"      },
    {2000000.0,       0, TMT_MONTH,   6, TMT_MONTH,  12, TMT_MONTH,  24, 365*24*3600, "'%g"     },
    {     -1.0,       0, TMT_MONTH,   0, TMT_MONTH,   0, TMT_MONTH,   0,           0, ""        }
};

/* sensible y label intervals ...*/
static ylab_t ylab[] = {
    {  0.1, {1, 2,  5, 10}},
    {  0.2, {1, 5, 10, 20}},
    {  0.5, {1, 2,  4, 10}},
    {  1.0, {1, 2,  5, 10}},
    {  2.0, {1, 5, 10, 20}},
    {  5.0, {1, 2,  4, 10}},
    { 10.0, {1, 2,  5, 10}},
    { 20.0, {1, 5, 10, 20}},
    { 50.0, {1, 2,  4, 10}},
    {100.0, {1, 2,  5, 10}},
    {200.0, {1, 5, 10, 20}},
    {500.0, {1, 2,  4, 10}},
    {  0.0, {0, 0,  0,  0}}
};

static gfx_color_t graph_col[GRC_MAX] = {
    [GRC_CANVAS] = {1.00, 1.00, 1.00, 1.00},
    [GRC_BACK]   = {0.95, 0.95, 0.95, 1.00},
    [GRC_SHADEA] = {0.81, 0.81, 0.81, 1.00},
    [GRC_SHADEB] = {0.62, 0.62, 0.62, 1.00},
    [GRC_GRID]   = {0.56, 0.56, 0.56, 0.75},
    [GRC_MGRID]  = {0.87, 0.31, 0.31, 0.60},
    [GRC_FONT]   = {0.00, 0.00, 0.00, 1.00},
    [GRC_ARROW]  = {0.50, 0.12, 0.12, 1.00},
    [GRC_AXIS]   = {0.12, 0.12, 0.12, 1.00},
    [GRC_FRAME]  = {0.00, 0.00, 0.00, 1.00}
};

static const char default_timestamp_fmt[] = "%Y-%m-%d %H:%M:%S";
static const char default_duration_fmt[] = "%H:%02m:%02s";



static char *sprintf_alloc( char *fmt, ...)
{
    char     *str = NULL;
    va_list   argp;
#ifdef HAVE_VASPRINTF
    va_start( argp, fmt );
    if (vasprintf( &str, fmt, argp ) == -1){
        va_end(argp);
        rrd_set_error ("vasprintf failed.");
        return(NULL);
    }
#else
    int       maxlen = 1024 + strlen(fmt);
    str = (char*)malloc(sizeof(char) * (maxlen + 1));
    if (str != NULL) {
        va_start(argp, fmt);
#ifdef HAVE_VSNPRINTF
        vsnprintf(str, maxlen, fmt, argp);
#else
        vsprintf(str, fmt, argp);
#endif
    }
#endif /* HAVE_VASPRINTF */
    va_end(argp);
    return str;
}

static int xtr(image_desc_t *im, time_t mytime)
{
    if (mytime == 0) {
        im->x_pixie = (double) im->xsize / (double) (im->end - im->start);
        return im->xorigin;
    }
    return (int) ((double) im->xorigin + im->x_pixie * (mytime - im->start));
}

/* translate data values into y coordinates */
static double ytr(image_desc_t *im, double value)
{
    double yval;
    if (isnan(value)) {
        if (!im->logarithmic)
            im->y_pixie = (double) im->ysize / (im->maxval - im->minval);
        else
            im->y_pixie =
                (double) im->ysize / (log10(im->maxval) - log10(im->minval));
        yval = im->yorigin;
    } else if (!im->logarithmic) {
        yval = im->yorigin - im->y_pixie * (value - im->minval);
    } else {
        if (value < im->minval) {
            yval = im->yorigin;
        } else {
            yval = im->yorigin - im->y_pixie * (log10(value) - log10(im->minval));
        }
    }
    return yval;
}

/* find SI magnitude symbol for the given number*/
static void auto_scale(image_desc_t *im, double *value, char **symb_ptr, double *magfact)
{
    static char *symbol[] = {
        "a", /* 1e-18 Atto */
        "f", /* 1e-15 Femto */
        "p", /* 1e-12 Pico */
        "n", /* 1e-9  Nano */
        "u", /* 1e-6  Micro */
        "m", /* 1e-3  Milli */
        " ", /* Base */
        "k", /* 1e3   Kilo */
        "M", /* 1e6   Mega */
        "G", /* 1e9   Giga */
        "T", /* 1e12  Tera */
        "P", /* 1e15  Peta */
        "E"  /* 1e18  Exa */
    };
    static int symbcenter = 6;

    int sindex;
    if (*value == 0.0 || isnan(*value)) {
        sindex = 0;
        *magfact = 1.0;
    } else {
        sindex = floor(log(fabs(*value)) / log((double) im->base));
        *magfact = pow((double) im->base, (double) sindex);
        (*value) /= (*magfact);
    }

    if (sindex <= symbcenter && sindex >= -symbcenter) {
        (*symb_ptr) = symbol[sindex + symbcenter];
    } else {
        (*symb_ptr) = "?";
    }
}

/* power prefixes */
static char si_symbol[] = {
    'y',  /* 1e-24 Yocto */
    'z',  /* 1e-21 Zepto */
    'a',  /* 1e-18 Atto */
    'f',  /* 1e-15 Femto */
    'p',  /* 1e-12 Pico */
    'n',  /* 1e-9  Nano */
    'u',  /* 1e-6  Micro */
    'm',  /* 1e-3  Milli */
    ' ',  /* Base */
    'k',  /* 1e3   Kilo */
    'M',  /* 1e6   Mega */
    'G',  /* 1e9   Giga */
    'T',  /* 1e12  Tera */
    'P',  /* 1e15  Peta */
    'E',  /* 1e18  Exa */
    'Z',  /* 1e21  Zeta */
    'Y'   /* 1e24  Yotta */
};
static const int si_symbcenter = 8;

/* find SI magnitude symbol for the numbers on the y-axis*/
static void si_unit(image_desc_t *im)
{
    double digits = floor(log(fmax(fabs(im->minval), fabs(im->maxval))) /
                    log((double) im->base));

    double viewdigits = 0;
    if (im->unitsexponent != 9999) {
        /* unitsexponent = 9, 6, 3, 0, -3, -6, -9, etc */
        viewdigits = floor((double) (im->unitsexponent / 3));
    } else {
        viewdigits = digits;
    }

    im->magfact = pow((double) im->base, digits);
    im->viewfactor = im->magfact / pow((double) im->base, viewdigits);

    if (((viewdigits + si_symbcenter) < sizeof(si_symbol)) &&
        ((viewdigits + si_symbcenter) >= 0))
        im->symbol = si_symbol[(int) viewdigits + si_symbcenter];
    else
        im->symbol = '?';
}

/*  move min and max values around to become sensible */
static void expand_range(image_desc_t *im)
{
    double sensible_values[] = {
       1000.0, 900.0, 800.0, 750.0, 700.0,
        600.0, 500.0, 400.0, 300.0, 250.0,
        200.0, 125.0, 100.0,  90.0,  80.0,
         75.0,   70.0, 60.0,  50.0,  40.0, 30.0,
         25.0,   20.0, 10.0,   9.0,   8.0,
          7.0,    6.0,  5.0,   4.0,   3.5,  3.0,
          2.5,    2.0,  1.8,   1.5,   1.2,  1.0,
          0.8,    0.7,  0.6,   0.5,   0.4,  0.3, 0.2, 0.1, 0.0, -1
    };

    if (isnan(im->ygridstep)) {
        if (im->extra_flags & ALTAUTOSCALE) {
            /* measure the amplitude of the function. Make sure that
               graph boundaries are slightly higher then max/min vals
               so we can see amplitude on the graph */
            double delt = im->maxval - im->minval;
            double adj = delt * 0.1;
            double fact = 2.0 * pow(10.0, floor(log10(fmax(fabs(im->minval),
                                                fabs(im->maxval)) / im->magfact)) - 2);
            if (delt < fact) {
                adj = (fact - delt) * 0.55;
            }
            im->minval -= adj;
            im->maxval += adj;
        } else if (im->extra_flags & ALTAUTOSCALE_MIN) {
            /* measure the amplitude of the function. Make sure that
               graph boundaries are slightly lower than min vals
               so we can see amplitude on the graph */
            im->minval -= (im->maxval - im->minval) * 0.1;

        } else if (im->extra_flags & ALTAUTOSCALE_MAX) {
            /* measure the amplitude of the function. Make sure that
               graph boundaries are slightly higher than max vals
               so we can see amplitude on the graph */
            im->maxval += (im->maxval - im->minval) * 0.1;
        } else {
            double scaled_min = im->minval / im->magfact;
            double scaled_max = im->maxval / im->magfact;

            for (int i = 1; sensible_values[i] > 0; i++) {
                if (sensible_values[i - 1] >= scaled_min &&
                    sensible_values[i] <= scaled_min)
                    im->minval = sensible_values[i] * (im->magfact);

                if (-sensible_values[i - 1] <= scaled_min &&
                    -sensible_values[i] >= scaled_min)
                    im->minval = -sensible_values[i - 1] * (im->magfact);

                if (sensible_values[i - 1] >= scaled_max &&
                    sensible_values[i] <= scaled_max)
                    im->maxval = sensible_values[i - 1] * (im->magfact);

                if (-sensible_values[i - 1] <= scaled_max &&
                    -sensible_values[i] >= scaled_max)
                    im->maxval = -sensible_values[i] * (im->magfact);
            }
        }
    } else {
        /* adjust min and max to the grid definition if there is one */
        im->minval = (double) im->ylabfact * im->ygridstep *
                     floor(im->minval / ((double) im->ylabfact * im->ygridstep));
        im->maxval = (double) im->ylabfact * im->ygridstep *
                     ceil(im->maxval / ((double) im->ylabfact * im->ygridstep));
    }
}

/* create a grid on the graph. it determines what to do
   from the values of xsize, start and end */
/* the xaxis labels are determined from the number of seconds per pixel
   in the requested graph */
static int calc_horizontal_grid(image_desc_t *im)
{
    int       pixel;
    int       gridind = 0;
    int       decimals, fractionals;

    im->ygrid_scale.labfact = 2;
    double range = im->maxval - im->minval;
    double scaledrange = range / im->magfact;
    /* does the scale of this graph make it impossible to put lines
       on it? If so, give up. */
    if (isnan(scaledrange)) {
        return 0;
    }

    /* find grid spacing */
    pixel = 1;
    if (isnan(im->ygridstep)) {
        if (im->extra_flags & ALTYGRID) {
            /* find the value with max number of digits. Get number of digits */
            decimals = ceil(log10 (fmax(fabs(im->maxval),
                                   fabs(im->minval)) * im->viewfactor / im->magfact));
            if (decimals <= 0)  /* everything is small. make place for zero */
                decimals = 1;
            im->ygrid_scale.gridstep = pow((double) 10,
                                           floor(log10(range * im->viewfactor / im->magfact)))
                                           / im->viewfactor * im->magfact;
            if (im->ygrid_scale.gridstep == 0)  /* range is one -> 0.1 is reasonable scale */
                im->ygrid_scale.gridstep = 0.1;
            /* should have at least 5 lines but no more then 15 */
            if ((range / im->ygrid_scale.gridstep < 5) && im->ygrid_scale.gridstep >= 30)
                im->ygrid_scale.gridstep /= 10;
            if (range / im->ygrid_scale.gridstep > 15)
                im->ygrid_scale.gridstep *= 10;
            if (range / im->ygrid_scale.gridstep > 5) {
                im->ygrid_scale.labfact = 1;
                if ((range / im->ygrid_scale.gridstep > 8) ||
                    (im->ygrid_scale.gridstep < 1.8 * im->text_prop[TEXT_PROP_AXIS].size))
                    im->ygrid_scale.labfact = 2;
            } else {
                im->ygrid_scale.gridstep /= 5;
                im->ygrid_scale.labfact = 5;
            }
            fractionals = floor(log10(im->ygrid_scale.gridstep *
                                      (double) im->ygrid_scale.labfact * im->viewfactor /
                                      im->magfact));
            if (fractionals < 0) {  /* small amplitude. */
                int len = decimals - fractionals + 1;
                if (im->unitslength < len + 2)
                    im->unitslength = len + 2;
                snprintf(im->ygrid_scale.labfmt, sizeof(im->ygrid_scale.labfmt), "%%%d.%df%s",
                         len, -fractionals, (im->symbol != ' ' ? " %c" : ""));
            } else {
                int len = decimals + 1;

                if (im->unitslength < len + 2)
                    im->unitslength = len + 2;
                snprintf(im->ygrid_scale.labfmt, sizeof im->ygrid_scale.labfmt, "%%%d.0f%s",
                         len, (im->symbol != ' ' ? " %c" : ""));
            }
        } else { /* classic rrd grid */
            for (int i = 0; ylab[i].grid > 0; i++) {
                pixel = im->ysize / (scaledrange / ylab[i].grid);
                gridind = i;
                if (pixel >= 5)
                    break;
            }

            for (int i = 0; i < 4; i++) {
                if (pixel * ylab[gridind].lfac[i] >= (1.8 * im->text_prop[TEXT_PROP_AXIS].size)) {
                    im->ygrid_scale.labfact = ylab[gridind].lfac[i];
                    break;
                }
            }

            im->ygrid_scale.gridstep = ylab[gridind].grid * im->magfact;
        }
    } else {
        im->ygrid_scale.gridstep = im->ygridstep;
        im->ygrid_scale.labfact = im->ylabfact;
    }
    return 1;
}


static void apply_gridfit(image_desc_t *im)
{
    if (isnan(im->minval) || isnan(im->maxval))
        return;

    ytr(im, NAN);
    if (im->logarithmic) {
        double log10_range = log10(im->maxval) - log10(im->minval);
        double ya = pow((double) 10, floor(log10(im->minval)));

        while (ya < im->minval)
            ya *= 10;
        if (ya > im->maxval)
            return;     /* don't have y=10^x gridline */
        double yb = ya * 10;
        if (yb <= im->maxval) {
            /* we have at least 2 y=10^x gridlines.
               Make sure distance between them in pixels
               are an integer by expanding im->maxval */
            double y_pixel_delta = ytr(im, ya) - ytr(im, yb);
            double factor = y_pixel_delta / floor(y_pixel_delta);
            double new_log10_range = factor * log10_range;
            double new_ymax_log10 = log10(im->minval) + new_log10_range;

            im->maxval = pow(10, new_ymax_log10);
            ytr(im, NAN);  /* reset precalc */
            log10_range = log10(im->maxval) - log10(im->minval);
        }
        /* make sure first y=10^x gridline is located on
           integer pixel position by moving scale slightly
           downwards (sub-pixel movement) */
        double ypix = ytr(im, ya) + im->ysize; /* add im->ysize so it always is positive */
        double ypixfrac = ypix - floor(ypix);

        if (ypixfrac > 0 && ypixfrac < 1) {
            double yfrac = ypixfrac / im->ysize;
            im->minval = pow(10, log10(im->minval) - yfrac * log10_range);
            im->maxval = pow(10, log10(im->maxval) - yfrac * log10_range);
            ytr(im, NAN);  /* reset precalc */
        }
    } else {
        /* Make sure we have an integer pixel distance between
           each minor gridline */
        double ypos1 = ytr(im, im->minval);
        double ypos2 = ytr(im, im->minval + im->ygrid_scale.gridstep);
        double y_pixel_delta = ypos1 - ypos2;
        double factor = y_pixel_delta / floor(y_pixel_delta);
        double new_range = factor * (im->maxval - im->minval);
        double gridstep = im->ygrid_scale.gridstep;
        double minor_y, minor_y_px, minor_y_px_frac;

        if (im->maxval > 0.0)
            im->maxval = im->minval + new_range;
        else
            im->minval = im->maxval - new_range;
        ytr(im, NAN);  /* reset precalc */
        /* make sure first minor gridline is on integer pixel y coord */
        minor_y = gridstep * floor(im->minval / gridstep);
        while (minor_y < im->minval)
            minor_y += gridstep;
        minor_y_px = ytr(im, minor_y) + im->ysize;  /* ensure > 0 by adding ysize */
        minor_y_px_frac = minor_y_px - floor(minor_y_px);
        if (minor_y_px_frac > 0 && minor_y_px_frac < 1) {
            double    yfrac = minor_y_px_frac / im->ysize;
            double    range = im->maxval - im->minval;

            im->minval = im->minval - yfrac * range;
            im->maxval = im->maxval - yfrac * range;
            ytr(im, NAN);  /* reset precalc */
        }
        calc_horizontal_grid(im);   /* recalc with changed im->maxval */
    }
}

#if 0
/* reduce data reimplementation by Alex */
int rrd_reduce_data(
    enum cf_en cf,      /* which consolidation function ? */
    unsigned long cur_step, /* step the data currently is in */
    time_t *start,      /* start, end and step as requested ... */
    time_t *end,        /* ... by the application will be   ... */
    unsigned long *step,    /* ... adjusted to represent reality    */
    unsigned long *ds_cnt,  /* number of data sources in file */
    rrd_value_t **data)
{                       /* two dimensional array containing the data */
    int       i, reduce_factor = ceil((double) (*step) / (double) cur_step);
    unsigned long col, dst_row, row_cnt, start_offset, end_offset, skiprows =
        0;
    rrd_value_t *srcptr, *dstptr;

    (*step) = cur_step * reduce_factor; /* set new step size for reduced data */
    dstptr = *data;
    srcptr = *data;
    row_cnt = ((*end) - (*start)) / cur_step;

#ifdef DEBUG
#define DEBUG_REDUCE
#endif
#ifdef DEBUG_REDUCE
    printf("Reducing %lu rows with factor %i time %lu to %lu, step %lu\n",
           row_cnt, reduce_factor, *start, *end, cur_step);
    for (col = 0; col < row_cnt; col++) {
        printf("time %10lu: ", *start + (col + 1) * cur_step);
        for (i = 0; i < *ds_cnt; i++)
            printf(" %8.2e", srcptr[*ds_cnt * col + i]);
        printf("\n");
    }
#endif

    /* We have to combine [reduce_factor] rows of the source
     ** into one row for the destination.  Doing this we also
     ** need to take care to combine the correct rows.  First
     ** alter the start and end time so that they are multiples
     ** of the new step time.  We cannot reduce the amount of
     ** time so we have to move the end towards the future and
     ** the start towards the past.
     */
    end_offset = (*end) % (*step);
    start_offset = (*start) % (*step);

    /* If there is a start offset (which cannot be more than
     ** one destination row), skip the appropriate number of
     ** source rows and one destination row.  The appropriate
     ** number is what we do know (start_offset/cur_step) of
     ** the new interval (*step/cur_step aka reduce_factor).
     */
#ifdef DEBUG_REDUCE
    printf("start_offset: %lu  end_offset: %lu\n", start_offset, end_offset);
    printf("row_cnt before:  %lu\n", row_cnt);
#endif
    if (start_offset) {
        (*start) = (*start) - start_offset;
        skiprows = reduce_factor - start_offset / cur_step;
        srcptr += skiprows * *ds_cnt;
        for (col = 0; col < (*ds_cnt); col++)
            *dstptr++ = NAN;
        row_cnt -= skiprows;
    }
#ifdef DEBUG_REDUCE
    printf("row_cnt between: %lu\n", row_cnt);
#endif

    /* At the end we have some rows that are not going to be
     ** used, the amount is end_offset/cur_step
     */
    if (end_offset) {
        (*end) = (*end) - end_offset + (*step);
        skiprows = end_offset / cur_step;
        row_cnt -= skiprows;
    }
#ifdef DEBUG_REDUCE
    printf("row_cnt after:   %lu\n", row_cnt);
#endif

/* Sanity check: row_cnt should be multiple of reduce_factor */
/* if this gets triggered, something is REALLY WRONG ... we die immediately */

    if (row_cnt % reduce_factor) {
        rrd_set_error("SANITY CHECK: %lu rows cannot be reduced by %i \n",
                      row_cnt, reduce_factor);
        return 0;
    }

    /* Now combine reduce_factor intervals at a time
     ** into one interval for the destination.
     */

    for (dst_row = 0; (long int) row_cnt >= reduce_factor; dst_row++) {
        for (col = 0; col < (*ds_cnt); col++) {
            rrd_value_t newval = NAN;
            unsigned long validval = 0;

            for (i = 0; i < reduce_factor; i++) {
                if (isnan(srcptr[i * (*ds_cnt) + col])) {
                    continue;
                }
                validval++;
                if (isnan(newval))
                    newval = srcptr[i * (*ds_cnt) + col];
                else {
                    switch (cf) {
                    case CF_HWPREDICT:
                    case CF_MHWPREDICT:
                    case CF_DEVSEASONAL:
                    case CF_DEVPREDICT:
                    case CF_SEASONAL:
                    case CF_AVERAGE:
                        newval += srcptr[i * (*ds_cnt) + col];
                        break;
                    case CF_MINIMUM:
                        newval = min(newval, srcptr[i * (*ds_cnt) + col]);
                        break;
                    case CF_FAILURES:
                        /* an interval contains a failure if any subintervals contained a failure */
                    case CF_MAXIMUM:
                        newval = max(newval, srcptr[i * (*ds_cnt) + col]);
                        break;
                    case CF_LAST:
                        newval = srcptr[i * (*ds_cnt) + col];
                        break;
                    }
                }
            }
            if (validval == 0) {
                newval = NAN;
            } else {
                switch (cf) {
                case CF_HWPREDICT:
                case CF_MHWPREDICT:
                case CF_DEVSEASONAL:
                case CF_DEVPREDICT:
                case CF_SEASONAL:
                case CF_AVERAGE:
                    newval /= validval;
                    break;
                case CF_MINIMUM:
                case CF_FAILURES:
                case CF_MAXIMUM:
                case CF_LAST:
                    break;
                }
            }
            *dstptr++ = newval;
        }
        srcptr += (*ds_cnt) * reduce_factor;
        row_cnt -= reduce_factor;
    }
    /* If we had to alter the endtime, we didn't have enough
     ** source rows to fill the last row. Fill it with NaN.
     */
    if (end_offset)
        for (col = 0; col < (*ds_cnt); col++)
            *dstptr++ = NAN;
#ifdef DEBUG_REDUCE
    row_cnt = ((*end) - (*start)) / *step;
    srcptr = *data;
    printf("Done reducing. Currently %lu rows, time %lu to %lu, step %lu\n",
           row_cnt, *start, *end, *step);
    for (col = 0; col < row_cnt; col++) {
        printf("time %10lu: ", *start + (col + 1) * (*step));
        for (i = 0; i < *ds_cnt; i++)
            printf(" %8.2e", srcptr[*ds_cnt * col + i]);
        printf("\n");
    }
#endif
    return 1;
}
#endif
#if 0
static int fetch(const char *query, time_t *start, time_t *end, unsigned long *step)
{

    return 0;
}
#endif
#if 0
/* get the data required for the graphs */
static int data_fetch(image_desc_t *im)
{
    /* pull the data ... */
    for (int i = 0; i < (int) im->gdes_c; i++) {
        /* only GF_DEF elements fetch data */
        if ((im->gdes[i].gf != GF_LINE) &&
            (im->gdes[i].gf != GF_AREA) &&
            (im->gdes[i].gf != GF_TICK))
            continue;

        unsigned long ft_step = im->gdes[i].step;   /* ft_step will record what we got from fetch */
        int status;

        int status = fetch(im->gdes[i].query, &im->gdes[i].start, &im->gdes[i].end, &ft_step)


        im->gdes[i].data_first = 1;

        /* must reduce to at least im->step
           otherwise we end up with more data than we can handle in the
           chart and visibility of data will be random */
        im->gdes[i].step = max(im->gdes[i].step, im->step);
        if (ft_step < im->gdes[i].step) {
            if (!rrd_reduce_data(im->gdes[i].cf_reduce_set ? im->gdes[i].cf_reduce : im-> gdes[i].cf,
                                 ft_step, &im->gdes[i].start, &im->gdes[i].end, &im->gdes[i].step, &im->gdes[i].ds_cnt,
                                 &im->gdes[i].data)) {
                return -1;
            }
        } else {
            im->gdes[i].step = ft_step;
        }

        /* lets see if the required data source is really there */
        for (int ii = 0; ii < (int) im->gdes[i].ds_cnt; ii++) {
            if (strcmp(im->gdes[i].ds_namv[ii], im->gdes[i].ds_nam) == 0) {
                im->gdes[i].ds = ii;
            }
        }
        if ((im->gdes[i].ds == -1) && !(im->extra_flags & ALLOW_MISSING_DS)) {
            fprintf(stderr, "No DS called '%s' in '%s'", im->gdes[i].ds_nam, im->gdes[i].rrd);
            return -1;
        }
        // remember that we already got this one
        // g_hash_table_insert(im->rrd_map, gdes_fetch_key(im->gdes[i]), GINT_TO_POINTER(i));
    }
    return 0;
}
#endif

/* evaluate the expressions in the CDEF functions */

/*************************************************************
 * CDEF stuff
 *************************************************************/

#if 0
/* find the greatest common divisor for all the numbers
   in the 0 terminated num array */
static long rrd_lcd(long *num)
{
    long rest;
    int i;
    for (i = 0; num[i + 1] != 0; i++) {
        do {
            rest = num[i] % num[i + 1];
            num[i] = num[i + 1];
            num[i + 1] = rest;
        } while (rest != 0);
        num[i + 1] = num[i];
    }
/*  return i==0?num[i]:num[i-1]; */
    return num[i];
}
#endif
#if 0
/* run the rpn calculator on all the VDEF and CDEF arguments */
static int data_calc(image_desc_t *im)
{
    int       dataidx;
    long     *steparray, rpi;
    long     *steparray_tmp;    /* temp variable for realloc() */
    int       stepcnt;
    time_t    now;
    rpnstack_t rpnstack;
    rpnp_t   *rpnp;

    rpnstack_init(&rpnstack);

    for (int gdi = 0; gdi < im->gdes_c; gdi++) {
        /* Look for GF_VDEF and GF_CDEF in the same loop,
         * so CDEFs can use VDEFs and vice versa
         */
        switch (im->gdes[gdi].gf) {
        case GF_XPORT:
            break;
        case GF_SHIFT:{
            graph_desc_t *vdp = &im->gdes[im->gdes[gdi].vidx];

            /* remove current shift */
            vdp->start -= vdp->shift;
            vdp->end -= vdp->shift;

            /* vdef */
            if (im->gdes[gdi].shidx >= 0)
                vdp->shift = im->gdes[im->gdes[gdi].shidx].vf.val;
            /* constant */
            else
                vdp->shift = im->gdes[gdi].shval;

            /* normalize shift to multiple of consolidated step */
            vdp->shift = (vdp->shift / (long) vdp->step) * (long) vdp->step;

            /* apply shift */
            vdp->start += vdp->shift;
            vdp->end += vdp->shift;
            break;
        }
        case GF_VDEF:
            /* A VDEF has no DS.  This also signals other parts
             * of rrdtool that this is a VDEF value, not a CDEF.
             */
            im->gdes[gdi].ds_cnt = 0;
            if (vdef_calc(im, gdi)) {
                rrd_set_error("Error processing VDEF '%s'",
                              im->gdes[gdi].vname);
                rpnstack_free(&rpnstack);
                return -1;
            }
            break;
        case GF_CDEF:
            im->gdes[gdi].ds_cnt = 1;
            im->gdes[gdi].ds = 0;
            im->gdes[gdi].data_first = 1;
            im->gdes[gdi].start = 0;
            im->gdes[gdi].end = 0;
            steparray = NULL;
            stepcnt = 0;
            dataidx = -1;
            rpnp = im->gdes[gdi].rpnp;

            /* Find the variables in the expression.
             * - VDEF variables are substituted by their values
             *   and the opcode is changed into OP_NUMBER.
             * - CDEF variables are analyzed for their step size,
             *   the lowest common denominator of all the step
             *   sizes of the data sources involved is calculated
             *   and the resulting number is the step size for the
             *   resulting data source.
             */
            for (rpi = 0; im->gdes[gdi].rpnp[rpi].op != OP_END; rpi++) {
                if (im->gdes[gdi].rpnp[rpi].op == OP_VARIABLE ||
                    im->gdes[gdi].rpnp[rpi].op == OP_PREV_OTHER) {
                    long      ptr = im->gdes[gdi].rpnp[rpi].ptr;

                    if (im->gdes[ptr].ds_cnt == 0) {    /* this is a VDEF data source */
#if 0
                        printf
                            ("DEBUG: inside CDEF '%s' processing VDEF '%s'\n",
                             im->gdes[gdi].vname, im->gdes[ptr].vname);
                        printf("DEBUG: value from vdef is %f\n",
                               im->gdes[ptr].vf.val);
#endif
                        im->gdes[gdi].rpnp[rpi].val = im->gdes[ptr].vf.val;
                        im->gdes[gdi].rpnp[rpi].op = OP_NUMBER;
                    } else {    /* normal variables and PREF(variables) */

                        /* add one entry to the array that keeps track of the step sizes of the
                         * data sources going into the CDEF. */
                        if ((steparray_tmp =
                             (long *) rrd_realloc(steparray,
                                                  (++stepcnt +
                                                   1) *
                                                  sizeof(*steparray))) ==
                            NULL) {
                            rrd_set_error("realloc steparray");
                            rpnstack_free(&rpnstack);
                            return -1;
                        };
                        steparray = steparray_tmp;

                        steparray[stepcnt - 1] = im->gdes[ptr].step;

                        /* adjust start and end of cdef (gdi) so
                         * that it runs from the latest start point
                         * to the earliest endpoint of any of the
                         * rras involved (ptr)
                         */

                        if (im->gdes[gdi].start < im->gdes[ptr].start)
                            im->gdes[gdi].start = im->gdes[ptr].start;

                        if (im->gdes[gdi].end == 0 ||
                            im->gdes[gdi].end > im->gdes[ptr].end)
                            im->gdes[gdi].end = im->gdes[ptr].end;

                        /* store pointer to the first element of
                         * the rra providing data for variable,
                         * further save step size and data source
                         * count of this rra
                         */
                        im->gdes[gdi].rpnp[rpi].data =
                            im->gdes[ptr].data + im->gdes[ptr].ds;
                        im->gdes[gdi].rpnp[rpi].step = im->gdes[ptr].step;
                        im->gdes[gdi].rpnp[rpi].ds_cnt = im->gdes[ptr].ds_cnt;

                        /* backoff the *.data ptr; this is done so
                         * rpncalc() function doesn't have to treat
                         * the first case differently
                         */
                    }   /* if ds_cnt != 0 */
                }       /* if OP_VARIABLE */
            }           /* loop through all rpi */

            /* move the data pointers to the correct period */
            for (rpi = 0; im->gdes[gdi].rpnp[rpi].op != OP_END; rpi++) {
                if (im->gdes[gdi].rpnp[rpi].op == OP_VARIABLE ||
                    im->gdes[gdi].rpnp[rpi].op == OP_PREV_OTHER) {
                    long      ptr = im->gdes[gdi].rpnp[rpi].ptr;
                    long      diff =
                        im->gdes[gdi].start - im->gdes[ptr].start;

                    if (diff > 0)
                        im->gdes[gdi].rpnp[rpi].data +=
                            (diff / im->gdes[ptr].step) *
                            im->gdes[ptr].ds_cnt;
                }
            }

            if (steparray == NULL) {
                rrd_set_error("rpn expressions without DEF"
                              " or CDEF variables are not supported");
                rpnstack_free(&rpnstack);
                return -1;
            }
            steparray[stepcnt] = 0;
            /* Now find the resulting step.  All steps in all
             * used RRAs have to be visited
             */
            im->gdes[gdi].step = rrd_lcd(steparray);
            free(steparray);

            if ((im->gdes[gdi].data = (rrd_value_t *)
                 malloc(((im->gdes[gdi].end - im->gdes[gdi].start)
                         / im->gdes[gdi].step)
                        * sizeof(double))) == NULL) {
                rrd_set_error("malloc im->gdes[gdi].data");
                rpnstack_free(&rpnstack);
                return -1;
            }

            /* Step through the new cdef results array and
             * calculate the values
             */
            for (now = im->gdes[gdi].start + im->gdes[gdi].step;
                 now <= im->gdes[gdi].end; now += im->gdes[gdi].step) {

                /* 3rd arg of rpn_calc is for OP_VARIABLE lookups;
                 * in this case we are advancing by timesteps;
                 * we use the fact that time_t is a synonym for long
                 */
                if (rpn_calc(rpnp, &rpnstack, (long) now,
                             im->gdes[gdi].data, ++dataidx,
                             im->gdes[gdi].step) == -1) {
                    /* rpn_calc sets the error string */
                    rpnstack_free(&rpnstack);
                    rpnp_freeextra(rpnp);
                    return -1;
                }
            }           /* enumerate over time steps within a CDEF */
            rpnp_freeextra(rpnp);

            break;
        default:
            continue;
        }
    }                   /* enumerate over CDEFs */
    rpnstack_free(&rpnstack);
    return 0;
}
#endif

/* from http://www.cygnus-software.com/papers/comparingfloats/comparingfloats.htm */
/* yes we are losing precision by doing tos with floats instead of doubles
   but it seems more stable this way. */

static int almostequal2scomplement(float a, float b, int max_ulps)
{
    typedef union {
       float float32;
       int32_t int32;
    } float_int_t;

    float_int_t fia = {.float32 = a};
    float_int_t fib = {.float32 = b};
//  int a_int = fia.int32;
//  int b_int = fib.int32;
//  int a_int = *(int *)&a;
//  int b_int = *(int *)&b;

    /* Make sure maxUlps is non-negative and small enough that the
       default NAN won't compare as equal to anything.  */
    /* assert(maxUlps > 0 && maxUlps < 4 * 1024 * 1024); */
    /* Make aInt lexicographically ordered as a twos-complement int */
    if (fia.int32 < 0)
        fia.int32 = 0x80000000l - fia.int32;
    /* Make bInt lexicographically ordered as a twos-complement int */
    if (fib.int32 < 0)
        fib.int32 = 0x80000000l - fib.int32;

    int int_diff = abs(fia.int32 - fib.int32);

    if (int_diff <= max_ulps)
        return 1;

    return 0;
}

/* massage data so, that we get one value for each x coordinate in the graph */
static int data_proc(image_desc_t *im)
{
    /* memory for the processed data */
    for (int i = 0; i < im->gdes_c; i++) {
        if ((im->gdes[i].gf == GF_LINE) ||
            (im->gdes[i].gf == GF_AREA) ||
            (im->gdes[i].gf == GF_TICK)) {
            im->gdes[i].p_data =  malloc((im->xsize + 1) * sizeof (rrd_value_t));
            if (im->gdes[i].p_data == NULL) {
                fprintf(stderr, "malloc data_proc");
                return -1;
            }
        }
    }

    /* how much time passes in one pixel */
    double pixstep = (double) (im->end - im->start) / (double) im->xsize;
    double minval = NAN;
    double maxval = NAN;
    for (int i = 0; i < im->xsize; i++) { /* for each pixel */
        unsigned long gr_time = im->start + pixstep * i;  /* time of the current step */
        double paintval = 0.0;

        for (int ii = 0; ii < im->gdes_c; ii++) {
            double value;

            switch (im->gdes[ii].gf) {
                case GF_LINE:
                case GF_AREA:
                case GF_TICK:
                    if (!im->gdes[ii].stack)
                        paintval = 0.0;
                    value = im->gdes[ii].yrule;
                    if (isnan(value) || (im->gdes[ii].gf == GF_TICK)) {
                        /* The time of the data doesn't necessarily match
                         ** the time of the graph. Beware.
                         */
                        int vidx = im->gdes[ii].vidx;
                        if (im->gdes[vidx].gf == GF_VDEF) {
                            value = im->gdes[vidx].vf.val;
                        } else if (((long int) gr_time >= (long int) im->gdes[vidx].start) &&
                                   ((long int) gr_time < (long int) im->gdes[vidx].end)) {
                            double didx =  (double)(gr_time - im-> gdes[vidx].start)
                                                   / im->gdes[vidx].step;
                            value = im->gdes[vidx].data[(unsigned long) floor(didx)
                                                        * im->gdes[vidx].ds_cnt + im->gdes[vidx].ds];
                        } else {
                            value = NAN;
                        }
                    };

                    if (!isnan(value)) {
                        paintval += value;
                        im->gdes[ii].p_data[i] = paintval;
                        /* GF_TICK: the data values are not
                         ** relevant for min and max
                         */
                        if (isfinite(paintval) && (im->gdes[ii].gf != GF_TICK) &&
                            !im->gdes[ii].skipscale) {
                            if ((isnan(minval) || (paintval < minval)) &&
                                !(im->logarithmic && paintval <= 0.0)) {
                                minval = paintval;
                            }
                            if (isnan(maxval) || paintval > maxval)
                                maxval = paintval;
                        }
                    } else {
                        im->gdes[ii].p_data[i] = NAN;
                    }
                    break;
                default:
                    break;
            }
        }
    }

    /* if min or max have not been assigned a value this is because
       there was no data in the graph ... this is not good ...
       lets set these to dummy values then ... */

    if (im->logarithmic) {
        if (isnan(minval) || isnan(maxval) || maxval <= 0) {
            minval = 0.0;   /* catching this right away below */
            maxval = 5.1;
        }
        /* in logarithm mode, where minval is smaller or equal
           to 0 make the beast just way smaller than maxval */
        if (minval <= 0)
            minval = maxval / 10e8;
    } else {
        if (isnan(minval) || isnan(maxval)) {
            minval = 0.0;
            maxval = 1.0;
        }
    }

    /* adjust min and max values given by the user */
    /* for logscale we add something on top */
    if (isnan(im->minval) || ((!im->rigid) && im->minval > minval)) {
        if (im->logarithmic)
            im->minval = minval / 2.0;
        else
            im->minval = minval;
    }

    if (isnan(im->maxval) || ((!im->rigid) && im->maxval < maxval)) {
        if (im->logarithmic)
            im->maxval = maxval * 2.0;
        else
            im->maxval = maxval;
    }

    if (!isnan(im->minval) && (im->rigid && im->allow_shrink && im->minval < minval)) {
        if (im->logarithmic)
            im->minval = minval / 2.0;
        else
            im->minval = minval;
    }
    if (!isnan(im->maxval) && (im->rigid && im->allow_shrink && im->maxval > maxval)) {
        if (im->logarithmic)
            im->maxval = maxval * 2.0;
        else
            im->maxval = maxval;
    }

    /* make sure min is smaller than max */
    if (im->minval > im->maxval) {
        if (im->minval > 0)
            im->minval = 0.99 * im->maxval;
        else
            im->minval = 1.01 * im->maxval;
    }

    /* make sure min and max are not equal */
    if (almostequal2scomplement(im->minval, im->maxval, 4)) {
        if (im->maxval > 0)
            im->maxval *= 1.01;
        else
            im->maxval *= 0.99;
        /* make sure min and max are not both zero */
        if (almostequal2scomplement(im->maxval, 0, 4))
            im->maxval = 1.0;
    }

    return 0;
}

static int find_first_weekday(void)
{
    static int first_weekday = -1;

    if (first_weekday == -1) {
#ifdef HAVE__NL_TIME_WEEK_1STDAY
        /* according to http://sourceware.org/ml/libc-locales/2009-q1/msg00011.html */
        /* See correct way here http://pasky.or.cz/dev/glibc/first_weekday.c */
        first_weekday = nl_langinfo(_NL_TIME_FIRST_WEEKDAY)[0];
        int       week_1stday;
        long      week_1stday_l = (long) nl_langinfo(_NL_TIME_WEEK_1STDAY);

        if (week_1stday_l == 19971130
#if SIZEOF_LONG_INT > 4
            || week_1stday_l >> 32 == 19971130
#endif
            )
            week_1stday = 0;    /* Sun */
        else if (week_1stday_l == 19971201
#if SIZEOF_LONG_INT > 4
                 || week_1stday_l >> 32 == 19971201
#endif
            )
            week_1stday = 1;    /* Mon */
        else {
            first_weekday = 1;
            return first_weekday;   /* we go for a Monday default */
        }
        first_weekday = (week_1stday + first_weekday - 1) % 7;
        first_weekday = 0;
#endif
    }
    return first_weekday;
}

/* identify the point where the first gridline, label ... gets placed */
static time_t find_first_time(
    time_t start,       /* what is the initial time */
    tmt_t baseint,    /* what is the basic interval */
    long basestep,      /* how many if these do we jump a time */
    int utc             /* set to 1 if we force the UTC timezone */
    )
{
    struct tm tm;

    LOCALTIME_R(&start, &tm, utc);
    switch (baseint) {
        case TMT_SECOND:
            tm.tm_sec -= tm.tm_sec % basestep;
            break;
        case TMT_MINUTE:
            tm.tm_sec = 0;
            tm.tm_min -= tm.tm_min % basestep;
            break;
        case TMT_HOUR:
            tm.tm_sec = 0;
            tm.tm_min = 0;
            tm.tm_hour -= tm.tm_hour % basestep;
            break;
        case TMT_DAY:
            /* we do NOT look at the basestep for this ... */
            tm.tm_sec = 0;
            tm.tm_min = 0;
            tm.tm_hour = 0;
            break;
        case TMT_WEEK:
            /* we do NOT look at the basestep for this ... */
            tm.tm_sec = 0;
            tm.tm_min = 0;
            tm.tm_hour = 0;
            tm.tm_mday -= tm.tm_wday - find_first_weekday();
            if (tm.tm_wday == 0 && find_first_weekday() > 0)
                tm.tm_mday -= 7; /* we want the *previous* week */
            break;
        case TMT_MONTH:
            tm.tm_sec = 0;
            tm.tm_min = 0;
            tm.tm_hour = 0;
            tm.tm_mday = 1;
            tm.tm_mon -= tm.tm_mon % basestep;
            break;
        case TMT_YEAR:
            tm.tm_sec = 0;
            tm.tm_min = 0;
            tm.tm_hour = 0;
            tm.tm_mday = 1;
            tm.tm_mon = 0;
            tm.tm_year -= (tm.tm_year + 1900) %basestep;
            break;
    }
    return MKTIME(&tm, utc);
}

/* identify the point where the next gridline, label ... gets placed */
static time_t find_next_time(
    time_t current,     /* what is the initial time */
    tmt_t baseint,    /* what is the basic interval */
    long basestep,      /* how many if these do we jump a time */
    int utc             /* set to 1 if we force the UTC timezone */
    )
{
    struct tm tm;
    time_t    madetime;

    LOCALTIME_R(&current, &tm, utc);

    /* let mktime figure this dst on its own */
    //tm.tm_isdst = -1;

    int       limit = 2;

    switch (baseint) {
        case TMT_SECOND:
            limit = 7200;
            break;
        case TMT_MINUTE:
            limit = 120;
            break;
        case TMT_HOUR:
            limit = 2;
            break;
        default:
            limit = 2;
            break;
    }

    do {
        switch (baseint) {
            case TMT_SECOND:
                tm.tm_sec += basestep;
                break;
            case TMT_MINUTE:
                tm.tm_min += basestep;
                break;
            case TMT_HOUR:
                tm.tm_hour += basestep;
                break;
            case TMT_DAY:
                tm.tm_mday += basestep;
                break;
            case TMT_WEEK:
                tm.tm_mday += 7 * basestep;
                break;
            case TMT_MONTH:
                tm.tm_mon += basestep;
                break;
            case TMT_YEAR:
                tm.tm_year += basestep;
        }
        madetime = MKTIME(&tm, utc);
    /* this is necessary to skip impossible times like the daylight saving time skips */
    } while ((madetime == -1) && (limit-- >= 0));

    return madetime;
}

static int strfduration(char *const dest, const size_t destlen,
                        const char *const fmt, const double duration)
{
    char     *d = dest, *const dbound = dest + destlen;
    const char *f;
    int       wlen = 0;
    double    seconds = fabs(duration) / 1000.0,
        minutes = seconds / 60.0,
        hours = minutes / 60.0, days = hours / 24.0, weeks = days / 7.0;

#define STORC(chr) do {             \
    if (wlen == INT_MAX)            \
        return -1;                  \
    wlen++;                         \
    if (d < dbound)                 \
        *d++ = (chr);               \
} while(0);

#define STORPF(valArg) do {                                                         \
    double pval = trunc((valArg) * pow(10.0, precision)) / pow(10.0, precision);    \
    ptrdiff_t avail = dbound - d;                                                   \
    if (avail < 0 || avail > INT_MAX)                                               \
        return -1;                                                                  \
    char *tmpfmt = sprintf_alloc("%%%s%d.%df", zpad ? "0" : "", width, precision);  \
    if (!tmpfmt)                                                                    \
        return -1;                                                                  \
    int r = snprintf(d, avail, tmpfmt, pval);                                       \
    free(tmpfmt);                                                                   \
    if (r < 0)                                                                      \
        return -1;                                                                  \
    d += min(avail, r);                                                             \
    if (INT_MAX-r < wlen)                                                           \
        return -1;                                                                  \
    wlen += r;                                                                      \
} while(0);

    if (duration < 0)
        STORC('-')
            for (f = fmt; *f; f++) {
            if (*f != '%') {
                STORC(*f)
            } else {
                int       zpad, width = 0, precision = 0;

                f++;

                if ((zpad = *f == '0'))
                    f++;

                if (isdigit((int)*f)) {
                    int       nread;

                    sscanf(f, "%d%n", &width, &nread);
                    f += nread;
                }

                if (*f == '.') {
                    int       nread;

                    f++;
                    if (1 == sscanf(f, "%d%n", &precision, &nread)) {
                        if (precision < 0) {
                            fprintf(stderr, "Wrong duration format");
                            return -1;
                        }
                        f += nread;
                    }
                }

                switch (*f) {
                case '%':
                    STORC('%')
                    break;
                case 'W':
                    STORPF(weeks)
                    break;
                case 'd':
                    STORPF(days - trunc(weeks) * 7.0)
                    break;
                case 'D':
                    STORPF(days)
                    break;
                case 'h':
                    STORPF(hours - trunc(days) * 24.0)
                    break;
                case 'H':
                    STORPF(hours)
                    break;
                case 'm':
                    STORPF(minutes - trunc(hours) * 60.0)
                    break;
                case 'M':
                    STORPF(minutes)
                    break;
                case 's':
                    STORPF(seconds - trunc(minutes) * 60.0)
                    break;
                case 'S':
                    STORPF(seconds)
                    break;
                case 'f':
                    STORPF(fabs(duration) - trunc(seconds) * 1000.0)
                    break;
                default:
                    fprintf(stderr, "Wrong duration format");
                    return -1;
                }
            }
        }

    STORC('\0')
        if (destlen > 0)
        *(dbound - 1) = '\0';

    return wlen - 1;

#undef STORC
#undef STORPF
}

static int timestamp_to_tm(struct tm *tm, double timestamp)
{
    if (timestamp < LLONG_MIN || timestamp > LLONG_MAX)
        return 1;

    time_t ts = (long long int) timestamp;
    if (ts != (long long int) timestamp)
        return 1;
    gmtime_r(&ts, tm);
    return 0;
}

static void time_clean(char *result, char *format)
{
    int       j, jj;

/*     Handling based on
       - ANSI C99 Specifications                         http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1124.pdf
       - Single UNIX Specification version 2             http://www.opengroup.org/onlinepubs/007908799/xsh/strftime.html
       - POSIX:2001/Single UNIX Specification version 3  http://www.opengroup.org/onlinepubs/009695399/functions/strftime.html
       - POSIX:2008 Specifications                       http://www.opengroup.org/onlinepubs/9699919799/functions/strftime.html
       Specifications tells
       "If a conversion specifier is not one of the above, the behavior is undefined."

      C99 tells
       "A conversion specifier consists of a % character, possibly followed by an E or O modifier character (described below), followed by a character that determines the behavior of the conversion specifier.

      POSIX:2001 tells
      "A conversion specification consists of a '%' character, possibly followed by an E or O modifier, and a terminating conversion specifier character that determines the conversion specification's behavior."

      POSIX:2008 introduce more complex behavior that are not handled here.

      According to this, this code will replace:
      - % followed by @ by a %@
      - % followed by   by a %SPACE
      - % followed by . by a %.
      - % followed by % by a %
      - % followed by t by a TAB
      - % followed by E then anything by '-'
      - % followed by O then anything by '-'
      - % followed by anything else by at least one '-'. More characters may be added to better fit expected output length
*/

    jj = 0;
    for (j = 0; (j < FMT_LEG_LEN - 1) && (jj < FMT_LEG_LEN); j++) { /* we don't need to parse the last char */
        if (format[j] == '%') {
            if ((format[j + 1] == 'E') || (format[j + 1] == 'O')) {
                result[jj++] = '-';
                j += 2; /* We skip next 2 following char */
            } else if ((format[j + 1] == 'C') || (format[j + 1] == 'd') ||
                       (format[j + 1] == 'g') || (format[j + 1] == 'H') ||
                       (format[j + 1] == 'I') || (format[j + 1] == 'm') ||
                       (format[j + 1] == 'M') || (format[j + 1] == 'S') ||
                       (format[j + 1] == 'U') || (format[j + 1] == 'V') ||
                       (format[j + 1] == 'W') || (format[j + 1] == 'y')) {
                result[jj++] = '-';
                if (jj < FMT_LEG_LEN) {
                    result[jj++] = '-';
                }
                j++;    /* We skip the following char */
            } else if (format[j + 1] == 'j') {
                result[jj++] = '-';
                if (jj < FMT_LEG_LEN - 1) {
                    result[jj++] = '-';
                    result[jj++] = '-';
                }
                j++;    /* We skip the following char */
            } else if ((format[j + 1] == 'G') || (format[j + 1] == 'Y')) {
                /* Assuming Year on 4 digit */
                result[jj++] = '-';
                if (jj < FMT_LEG_LEN - 2) {
                    result[jj++] = '-';
                    result[jj++] = '-';
                    result[jj++] = '-';
                }
                j++;    /* We skip the following char */
            } else if (format[j + 1] == 'R') {
                result[jj++] = '-';
                if (jj < FMT_LEG_LEN - 3) {
                    result[jj++] = '-';
                    result[jj++] = ':';
                    result[jj++] = '-';
                    result[jj++] = '-';
                }
                j++;    /* We skip the following char */
            } else if (format[j + 1] == 'T') {
                result[jj++] = '-';
                if (jj < FMT_LEG_LEN - 6) {
                    result[jj++] = '-';
                    result[jj++] = ':';
                    result[jj++] = '-';
                    result[jj++] = '-';
                    result[jj++] = ':';
                    result[jj++] = '-';
                    result[jj++] = '-';
                }
                j++;    /* We skip the following char */
            } else if (format[j + 1] == 'F') {
                result[jj++] = '-';
                if (jj < FMT_LEG_LEN - 8) {
                    result[jj++] = '-';
                    result[jj++] = '-';
                    result[jj++] = '-';
                    result[jj++] = '-';
                    result[jj++] = '-';
                    result[jj++] = '-';
                    result[jj++] = '-';
                    result[jj++] = '-';
                    result[jj++] = '-';
                }
                j++;    /* We skip the following char */
            } else if (format[j + 1] == 'D') {
                result[jj++] = '-';
                if (jj < FMT_LEG_LEN - 6) {
                    result[jj++] = '-';
                    result[jj++] = '/';
                    result[jj++] = '-';
                    result[jj++] = '-';
                    result[jj++] = '/';
                    result[jj++] = '-';
                    result[jj++] = '-';
                }
                j++;    /* We skip the following char */
            } else if (format[j + 1] == 'n') {
                result[jj++] = '\r';
                result[jj++] = '\n';
                j++;    /* We skip the following char */
            } else if (format[j + 1] == 't') {
                result[jj++] = '\t';
                j++;    /* We skip the following char */
            } else if (format[j + 1] == '%') {
                result[jj++] = '%';
                j++;    /* We skip the following char */
            } else if (format[j + 1] == ' ') {
                if (jj < FMT_LEG_LEN - 1) {
                    result[jj++] = '%';
                    result[jj++] = ' ';
                }
                j++;    /* We skip the following char */
            } else if (format[j + 1] == '.') {
                if (jj < FMT_LEG_LEN - 1) {
                    result[jj++] = '%';
                    result[jj++] = '.';
                }
                j++;    /* We skip the following char */
            } else if (format[j + 1] == '@') {
                if (jj < FMT_LEG_LEN - 1) {
                    result[jj++] = '%';
                    result[jj++] = '@';
                }
                j++;    /* We skip the following char */
            } else {
                result[jj++] = '-';
                j++;    /* We skip the following char */
            }
        } else {
            result[jj++] = format[j];
        }
    }
    result[jj] = '\0';  /* We must force the end of the string */
}

static int bad_format_print(__attribute__((unused)) char *fmt)
{
    return 0;
}

/* calculate values required for PRINT and GPRINT functions */
static int print_calc(image_desc_t *im)
{
    long      ii, validsteps;
    double    printval;
    struct tm tmvdef;
    int       graphelement = 0;
    int       max_ii;
    double    magfact = -1;
    char     *si_symb = "";
    char     *percent_s;
//    int       prline_cnt = 0;

    /* wow initializing tmvdef is quite a task :-) */
    time_t    now = time(NULL);

    LOCALTIME_R(&now, &tmvdef, im->extra_flags & FORCE_UTC_TIME);
    for (int i = 0; i < im->gdes_c; i++) {
        int vidx = im->gdes[i].vidx;
        switch (im->gdes[i].gf) {
        case GF_PRINT:
        case GF_GPRINT:
            /* PRINT and GPRINT can now print VDEF generated values.
             * There's no need to do any calculations on them as these
             * calculations were already made.
             */
            if (im->gdes[vidx].gf == GF_VDEF) { /* simply use vals */
                printval = im->gdes[vidx].vf.val;
                LOCALTIME_R(&im->gdes[vidx].vf.when, &tmvdef, im->extra_flags & FORCE_UTC_TIME);
            } else {    /* need to calculate max,min,avg etcetera */
                max_ii = ((im->gdes[vidx].end - im->gdes[vidx].start)
                          / im->gdes[vidx].step * im->gdes[vidx].ds_cnt);
                printval = NAN;
                validsteps = 0;
                for (ii = im->gdes[vidx].ds;
                     ii < max_ii; ii += im->gdes[vidx].ds_cnt) {
                    if (!isfinite(im->gdes[vidx].data[ii]))
                        continue;
                    if (isnan(printval)) {
                        printval = im->gdes[vidx].data[ii];
                        validsteps++;
                        continue;
                    }

                    switch (im->gdes[i].cf) {
                    case CF_HWPREDICT:
                    case CF_MHWPREDICT:
                    case CF_DEVPREDICT:
                    case CF_DEVSEASONAL:
                    case CF_SEASONAL:
                    case CF_AVERAGE:
                        validsteps++;
                        printval += im->gdes[vidx].data[ii];
                        break;
                    case CF_MINIMUM:
                        printval = min(printval, im->gdes[vidx].data[ii]);
                        break;
                    case CF_FAILURES:
                    case CF_MAXIMUM:
                        printval = max(printval, im->gdes[vidx].data[ii]);
                        break;
                    case CF_LAST:
                        printval = im->gdes[vidx].data[ii];
                    }
                }
                if (im->gdes[i].cf == CF_AVERAGE || im->gdes[i].cf > CF_LAST) {
                    if (validsteps > 1) {
                        printval = (printval / validsteps);
                    }
                }
            }           /* prepare printval */

            if (!im->gdes[i].strftm && im->gdes[i].vformatter == VALUE_FORMATTER_NUMERIC) {
                if ((percent_s = strstr(im->gdes[i].format, "%S")) != NULL) {
                    /* Magfact is set to -1 upon entry to print_calc.  If it
                     * is still less than 0, then we need to run auto_scale.
                     * Otherwise, put the value into the correct units.  If
                     * the value is 0, then do not set the symbol or magnification
                     * so next the calculation will be performed again. */
                    if (magfact < 0.0) {
                        auto_scale(im, &printval, &si_symb, &magfact);
                        if (printval == 0.0)
                            magfact = -1.0;
                    } else {
                        printval /= magfact;
                    }
                    *(++percent_s) = 's';
                } else if (strstr(im->gdes[i].format, "%s") != NULL) {
                    auto_scale(im, &printval, &si_symb, &magfact);
                }
            }

            if (im->gdes[i].gf == GF_PRINT) {
                rrd_infoval_t prline;

                if (im->gdes[i].strftm) {
                    prline.u_str = (char *) malloc((FMT_LEG_LEN + 2) * sizeof(char));
                    if (im->gdes[vidx].vf.never == 1) {
                        time_clean(prline.u_str, im->gdes[i].format);
                    } else {
                        strftime(prline.u_str, FMT_LEG_LEN, im->gdes[i].format, &tmvdef);
                    }
                } else {
                    struct tm tmval;

                    switch (im->gdes[i].vformatter) {
                    case VALUE_FORMATTER_NUMERIC:
                        if (bad_format_print(im->gdes[i].format)) {
                            return -1;
                        } else {
                            prline.u_str = sprintf_alloc(im->gdes[i].format, printval, si_symb);
                        }
                        break;
                    case VALUE_FORMATTER_TIMESTAMP:
                        if (!isfinite(printval) || timestamp_to_tm(&tmval, printval)) {
                            prline.u_str = sprintf_alloc("%.0f", printval);
                        } else {
                            const char *fmt;

                            if (im->gdes[i].format[0] == '\0')
                                fmt = default_timestamp_fmt;
                            else
                                fmt = im->gdes[i].format;
                            prline.u_str = (char *) malloc(FMT_LEG_LEN * sizeof(char));
                            if (!prline.u_str)
                                return -1;
                            if (0 ==
                                strftime(prline.u_str, FMT_LEG_LEN, fmt, &tmval)) {
                                free(prline.u_str);
                                return -1;
                            }
                        }
                        break;
                    case VALUE_FORMATTER_DURATION:
                        if (!isfinite(printval)) {
                            prline.u_str = sprintf_alloc("%f", printval);
                        } else {
                            const char *fmt;

                            if (im->gdes[i].format[0] == '\0')
                                fmt = default_duration_fmt;
                            else
                                fmt = im->gdes[i].format;
                            prline.u_str = (char *) malloc(FMT_LEG_LEN * sizeof(char));
                            if (!prline.u_str)
                                return -1;
                            if (0 >
                                strfduration(prline.u_str, FMT_LEG_LEN, fmt, printval)) {
                                free(prline.u_str);
                                return -1;
                            }
                        }
                        break;
                    default:
                        fprintf(stderr, "Unsupported print value formatter");
                        return -1;
                    }
                }

                free(prline.u_str);
            } else {
                /* GF_GPRINT */

                if (im->gdes[i].strftm) {
                    if (im->gdes[vidx].vf.never == 1) {
                        time_clean(im->gdes[i].legend, im->gdes[i].format);
                    } else {
                        strftime(im->gdes[i].legend, FMT_LEG_LEN, im->gdes[i].format, &tmvdef);
                    }
                } else {
                    struct tm tmval;

                    switch (im->gdes[i].vformatter) {
                    case VALUE_FORMATTER_NUMERIC:
                        if (bad_format_print(im->gdes[i].format)) {
                            return -1;
                        }
                        snprintf(im->gdes[i].legend, FMT_LEG_LEN - 2,
                                 im->gdes[i].format, printval, si_symb);
                        break;
                    case VALUE_FORMATTER_TIMESTAMP:
                        if (!isfinite(printval)
                            || timestamp_to_tm(&tmval, printval)) {
                            snprintf(im->gdes[i].legend, FMT_LEG_LEN, "%.0f", printval);
                        } else {
                            const char *fmt;

                            if (im->gdes[i].format[0] == '\0')
                                fmt = default_timestamp_fmt;
                            else
                                fmt = im->gdes[i].format;
                            if (0 == strftime(im->gdes[i].legend, FMT_LEG_LEN, fmt, &tmval))
                                return -1;
                        }
                        break;
                    case VALUE_FORMATTER_DURATION:
                        if (!isfinite(printval)) {
                            snprintf(im->gdes[i].legend, FMT_LEG_LEN, "%f",
                                     printval);
                        } else {
                            const char *fmt;
                            if (im->gdes[i].format[0] == '\0')
                                fmt = default_duration_fmt;
                            else
                                fmt = im->gdes[i].format;
                            if (0 > strfduration(im->gdes[i].legend, FMT_LEG_LEN, fmt, printval))
                                return -1;
                        }
                        break;
                    default:
                        fprintf(stderr, "Unsupported gprint value formatter");
                        return -1;
                    }
                }
                graphelement = 1;
            }
            break;
        case GF_LINE:
        case GF_AREA:
        case GF_TICK:
            graphelement = 1;
            break;
        case GF_HRULE:
            if (isnan(im->gdes[i].yrule)) { /* we must set this here or the legend printer can not decide to print the legend */
                im->gdes[i].yrule = im->gdes[vidx].vf.val;
            };
            graphelement = 1;
            break;
        case GF_VRULE:
            if (im->gdes[i].xrule == 0) {   /* again ... the legend printer needs it */
                im->gdes[i].xrule = im->gdes[vidx].vf.when;
            };
            graphelement = 1;
            break;
        case GF_COMMENT:
        case GF_TEXTALIGN:
        case GF_DEF:
        case GF_CDEF:
        case GF_VDEF:
        case GF_SHIFT:
        case GF_XPORT:
            break;
        case GF_XAXIS:
        case GF_YAXIS:
            break;
        }
    }
    return graphelement;
}

/* place legends with color spots */
static int leg_place(image_desc_t *im, graph_gfx_t *gfx, void *gfx_ctx, int calc_width)
{
    /* graph labels */
    int       interleg = im->text_prop[TEXT_PROP_LEGEND].size * 2.0;
    int       border = im->text_prop[TEXT_PROP_LEGEND].size * 2.0;
    int       fill = 0, fill_last;
    double    legendwidth;  // = im->ximg - 2 * border;
    int       leg_c = 0;
    int       leg_y = 0;    //im->yimg;
    int       leg_cc;
    double    glue = 0;
    int       i, ii, mark = 0;
    char      default_txtalign = TXA_JUSTIFIED; /*default line orientation */
    int      *legspace;
    char     *tab;
    char      saved_legend[FMT_LEG_LEN + 5];

    if (calc_width) {
        legendwidth = 0;
    } else {
        legendwidth = im->legendwidth - 2 * border;
    }


    if (!(im->extra_flags & NOLEGEND) && !(im->extra_flags & ONLY_GRAPH)) {
        if ((legspace = (int *) malloc(im->gdes_c * sizeof(int))) == NULL) {
            fprintf(stderr, "malloc for legspace");
            return -1;
        }

        for (i = 0; i < im->gdes_c; i++) {
            char      prt_fctn; /*special printfunctions */

            if (calc_width) {
                strncpy(saved_legend, im->gdes[i].legend, sizeof saved_legend - 1);
                saved_legend[sizeof saved_legend - 1] = '\0';
            }

            fill_last = fill;
            /* hide legends for rules which are not displayed */
            if (im->gdes[i].gf == GF_TEXTALIGN) {
                default_txtalign = im->gdes[i].txtalign;
            }

            if (!(im->extra_flags & FORCE_RULES_LEGEND)) {
                if (im->gdes[i].gf == GF_HRULE
                    && (im->gdes[i].yrule < im->minval || im->gdes[i].yrule > im->maxval))
                    im->gdes[i].legend[0] = '\0';
                if (im->gdes[i].gf == GF_VRULE
                    && (im->gdes[i].xrule < im->start || im->gdes[i].xrule > im->end))
                    im->gdes[i].legend[0] = '\0';
            }

            /* turn \\t into tab */
            while ((tab = strstr(im->gdes[i].legend, "\\t"))) {
                memmove(tab, tab + 1, strlen(tab));
                tab[0] = (char) 9;
            }

            leg_cc = strlen(im->gdes[i].legend);
            /* is there a control code at the end of the legend string ? */
            if (leg_cc >= 2 && im->gdes[i].legend[leg_cc - 2] == '\\') {
                prt_fctn = im->gdes[i].legend[leg_cc - 1];
                leg_cc -= 2;
                im->gdes[i].legend[leg_cc] = '\0';
            } else {
                prt_fctn = '\0';
            }
            /* only valid control codes */
            if (prt_fctn != 'l' && prt_fctn != 'n' &&   /* a synonym for l */
                prt_fctn != 'r' &&
                prt_fctn != 'j' &&
                prt_fctn != 'c' &&
                prt_fctn != 'u' &&
                prt_fctn != '.' &&
                prt_fctn != 's' && prt_fctn != '\0' && prt_fctn != 'g') {
                free(legspace);
                fprintf(stderr, "Unknown control code at the end of '%s\\%c'",
                                 im->gdes[i].legend, prt_fctn);
                return -1;
            }
            /* \n -> \l */
            if (prt_fctn == 'n') {
                prt_fctn = 'l';
            }
            /* \. is a null operation to allow strings ending in \x */
            if (prt_fctn == '.') {
                prt_fctn = '\0';
            }

            /* remove excess space from the end of the legend for \g */
            while (prt_fctn == 'g' &&
                   leg_cc > 0 && im->gdes[i].legend[leg_cc - 1] == ' ') {
                leg_cc--;
                im->gdes[i].legend[leg_cc] = '\0';
            }

            if (leg_cc != 0) {

                /* no interleg space if string ends in \g */
                legspace[i] = (prt_fctn == 'g' ? 0 : interleg);
                if (fill > 0) {
                    fill += legspace[i];
                }
                fill += gfx->get_text_width(gfx_ctx,
                                       fill + border,
                                       im->text_prop[TEXT_PROP_LEGEND].font,
                                       im->text_prop[TEXT_PROP_LEGEND].size,
                                       im->tabwidth, im->gdes[i].legend);
                leg_c++;
            } else {
                legspace[i] = 0;
            }
            /* who said there was a special tag ... ? */
            if (prt_fctn == 'g') {
                prt_fctn = '\0';
            }

            if (prt_fctn == '\0') {
                if (calc_width && (fill > legendwidth)) {
                    legendwidth = fill;
                }
                if (i == im->gdes_c - 1 || fill > legendwidth) {
                    /* just one legend item is left right or center */
                    switch (default_txtalign) {
                    case TXA_RIGHT:
                        prt_fctn = 'r';
                        break;
                    case TXA_CENTER:
                        prt_fctn = 'c';
                        break;
                    case TXA_JUSTIFIED:
                        prt_fctn = 'j';
                        break;
                    default:
                        prt_fctn = 'l';
                        break;
                    }
                }
                /* is it time to place the legends ? */
                if (fill > legendwidth) {
                    if (leg_c > 1) {
                        /* go back one */
                        i--;
                        fill = fill_last;
                        leg_c--;
                    }
                }
                if (leg_c == 1 && prt_fctn == 'j') {
                    prt_fctn = 'l';
                }
            }

            if (prt_fctn != '\0') {
                double leg_x = border;
                if (leg_c >= 2 && prt_fctn == 'j') {
                    glue =
                        (double) (legendwidth - fill) / (double) (leg_c - 1);
                } else {
                    glue = 0;
                }
                if (prt_fctn == 'c')
                    leg_x = border + (double) (legendwidth - fill) / 2.0;
                if (prt_fctn == 'r')
                    leg_x = legendwidth - fill + border;
                for (ii = mark; ii <= i; ii++) {
                    if (im->gdes[ii].legend[0] == '\0')
                        continue;   /* skip empty legends */
                    im->gdes[ii].leg_x = leg_x;
                    im->gdes[ii].leg_y = leg_y + border;
                    leg_x += (double) gfx->get_text_width(gfx_ctx, leg_x,
                                                    im->text_prop[TEXT_PROP_LEGEND].font,
                                                    im->text_prop[TEXT_PROP_LEGEND].size,
                                                    im->tabwidth,
                                                    im->gdes[ii].legend)
                        + (double) legspace[ii]
                        + glue;
                }
                if (leg_x > border || prt_fctn == 's')
                    leg_y += im->text_prop[TEXT_PROP_LEGEND].size * 1.8;
                if (prt_fctn == 's')
                    leg_y -= im->text_prop[TEXT_PROP_LEGEND].size;
                if (prt_fctn == 'u')
                    leg_y -= im->text_prop[TEXT_PROP_LEGEND].size * 1.8;

                if (calc_width && (fill > legendwidth)) {
                    legendwidth = fill;
                }
                fill = 0;
                leg_c = 0;
                mark = ii;
            }

            if (calc_width) {
                strncpy(im->gdes[i].legend, saved_legend, sizeof im->gdes[0].legend);
                im->gdes[i].legend[sizeof im->gdes[0].legend - 1] = '\0';
            }
        }

        if (calc_width) {
            im->legendwidth = legendwidth + 2 * border;
        } else {
            im->legendheight = leg_y + border * 0.6;
        }
        free(legspace);
    }

    return 0;
}

static int draw_horizontal_grid(image_desc_t *im, graph_gfx_t *gfx, void *gfx_ctx)
{
    char      graph_label[512];
    int       nlabels = 0;
    double    x0 = im->xorigin;
    double    x1 = im->xorigin + im->xsize;
    int       sgrid = (int) (im->minval / im->ygrid_scale.gridstep - 1);
    int       egrid = (int) (im->maxval / im->ygrid_scale.gridstep + 1);
    double    second_axis_magfact = 0;
    char     *second_axis_symb = "";

    double scaledstep = im->ygrid_scale.gridstep / (double) im->magfact * (double) im->viewfactor;
    double MaxY = scaledstep * (double) egrid;
    for (int i = sgrid; i <= egrid; i++) {
        double y0 = ytr(im, im->ygrid_scale.gridstep * i);
        double yn = ytr(im, im->ygrid_scale.gridstep * (i + 1));

        if ((floor(y0 + 0.5) >= (im->yorigin - im->ysize)) && (floor(y0 + 0.5) <= im->yorigin)) {
            /* Make sure at least 2 grid labels are shown, even if it doesn't agree
               with the chosen settings. Add a label if required by settings, or if
               there is only one label so far and the next grid line is out of bounds. */
            if ((i % im->ygrid_scale.labfact == 0) ||
                ((nlabels == 1) && (yn < im->yorigin - im->ysize || yn > im->yorigin))) {
                switch (im->primary_axis_formatter) {
                case VALUE_FORMATTER_NUMERIC:
                    if (im->symbol == ' ') {
                        if (im->primary_axis_format == NULL || im->primary_axis_format[0] == '\0') {
                            if (im->extra_flags & ALTYGRID) {
                                snprintf(graph_label, sizeof graph_label, im->ygrid_scale.labfmt,
                                         scaledstep * (double) i);
                            } else {
                                if (MaxY < 10) {
                                    snprintf(graph_label, sizeof graph_label, "%4.1f",
                                             scaledstep * (double) i);
                                } else {
                                    snprintf(graph_label, sizeof graph_label, "%4.0f",
                                             scaledstep * (double) i);
                                }
                            }
                        } else {
                            snprintf(graph_label, sizeof graph_label, im->primary_axis_format,
                                     scaledstep * (double) i);
                        }
                    } else {
                        char sisym = (i == 0 ? ' ' : im->symbol);

                        if ((im->primary_axis_format == NULL) || (im->primary_axis_format[0] == '\0')) {
                            if (im->extra_flags & ALTYGRID) {
                                snprintf(graph_label, sizeof graph_label, im->ygrid_scale.labfmt,
                                         scaledstep * (double) i, sisym);
                            } else {
                                if (MaxY < 10) {
                                    snprintf(graph_label, sizeof graph_label, "%4.1f %c",
                                             scaledstep * (double) i, sisym);
                                } else {
                                    snprintf(graph_label, sizeof graph_label, "%4.0f %c",
                                             scaledstep * (double) i, sisym);
                                }
                            }
                        } else {
                            sprintf(graph_label, im->primary_axis_format,
                                    scaledstep * (double) i, sisym);
                        }
                    }
                    break;
                case VALUE_FORMATTER_TIMESTAMP:
                {
                    struct tm tm;
                    const char *yfmt;

                    if (im->primary_axis_format == NULL
                        || im->primary_axis_format[0] == '\0')
                        yfmt = default_timestamp_fmt;
                    else
                        yfmt = im->primary_axis_format;
                    if (timestamp_to_tm(&tm, im->ygrid_scale.gridstep * i))
                        snprintf( graph_label, sizeof graph_label, "%f", im->ygrid_scale.gridstep * i);
                    else if (0 == strftime(graph_label, sizeof graph_label, yfmt, &tm))
                        graph_label[0] = '\0';
                }
                    break;
                case VALUE_FORMATTER_DURATION:
                {
                    const char *yfmt;

                    if (im->primary_axis_format == NULL || im->primary_axis_format[0] == '\0')
                        yfmt = default_duration_fmt;
                    else
                        yfmt = im->primary_axis_format;
                    if (0 > strfduration(graph_label, sizeof graph_label, yfmt, im->ygrid_scale.gridstep * i))
                        graph_label[0] = '\0';
                }
                    break;
                default:
                    fprintf(stderr, "Unsupported left axis value formatter");
                    return -1;
                }
                nlabels++;
                if (im->second_axis_scale != 0) {
                    char graph_label_right[512];
                    double sval = im->ygrid_scale.gridstep * (double) i *
                                  im->second_axis_scale + im->second_axis_shift;
                    switch (im->second_axis_formatter) {
                    case VALUE_FORMATTER_NUMERIC:
                        if ((im->second_axis_format == NULL) || (im->second_axis_format[0] == '\0')) {
                            if (!second_axis_magfact) {
                                double dummy = im->ygrid_scale.gridstep *
                                               (double) (sgrid + egrid) / 2.0 *
                                               im->second_axis_scale +
                                               im->second_axis_shift;
                                auto_scale(im, &dummy, &second_axis_symb, &second_axis_magfact);
                            }
                            sval /= second_axis_magfact;

                            if (MaxY < 10) {
                                snprintf(graph_label_right,
                                         sizeof graph_label_right, "%5.1f %s",
                                         sval, second_axis_symb);
                            } else {
                                snprintf(graph_label_right,
                                         sizeof graph_label_right, "%5.0f %s",
                                         sval, second_axis_symb);
                            }
                        } else {
                            snprintf(graph_label_right,
                                     sizeof graph_label_right,
                                     im->second_axis_format, sval, "");
                        }
                        break;
                    case VALUE_FORMATTER_TIMESTAMP:
                    {
                        struct tm tm;
                        const char *yfmt;
                        if (im->second_axis_format == NULL || im->second_axis_format[0] == '\0')
                            yfmt = default_timestamp_fmt;
                        else
                            yfmt = im->second_axis_format;
                        if (timestamp_to_tm(&tm, sval))
                            snprintf(graph_label_right, sizeof graph_label_right, "%f", sval);
                        else if (0 == strftime(graph_label_right, sizeof graph_label, yfmt, &tm))
                            graph_label_right[0] = '\0';
                    }
                        break;
                    case VALUE_FORMATTER_DURATION:
                    {
                        const char *yfmt;
                        if (im->second_axis_format == NULL || im->second_axis_format[0] == '\0')
                            yfmt = default_duration_fmt;
                        else
                            yfmt = im->second_axis_format;
                        if (0 > strfduration(graph_label_right, sizeof graph_label_right, yfmt, sval))
                            graph_label_right[0] = '\0';
                    }
                        break;
                    default:
                        fprintf(stderr, "Unsupported right axis value formatter");
                        return -1;
                    }
                    gfx->text(gfx_ctx,
                             x1 + 7, y0,
                             im->graph_col[GRC_FONT],
                             im->text_prop[TEXT_PROP_AXIS].font,
                             im->text_prop[TEXT_PROP_AXIS].size,
                             im->tabwidth, 0.0, GFX_H_LEFT, GFX_V_CENTER,
                             graph_label_right);
                }

                gfx->text(gfx_ctx,
                         x0 - im->text_prop[TEXT_PROP_AXIS].size, y0,
                         im->graph_col[GRC_FONT],
                         im->text_prop[TEXT_PROP_AXIS].font,
                         im->text_prop[TEXT_PROP_AXIS].size,
                         im->tabwidth, 0.0,
                         GFX_H_RIGHT, GFX_V_CENTER, graph_label);
                gfx->line(gfx_ctx, x0 - 2, y0, x0, y0,
                         MGRIDWIDTH, im->graph_col[GRC_MGRID]);
                gfx->line(gfx_ctx, x1, y0, x1 + 2, y0,
                         MGRIDWIDTH, im->graph_col[GRC_MGRID]);
                gfx->dashed_line(gfx_ctx, x0 - 2, y0,
                                x1 + 2, y0,
                                MGRIDWIDTH,
                                im->graph_col
                                [GRC_MGRID],
                                im->grid_dash, 2, 0);
            } else if (!(im->extra_flags & NOMINOR)) {
                gfx->line(gfx_ctx,
                         x0 - 2, y0,
                         x0, y0, GRIDWIDTH, im->graph_col[GRC_GRID]);
                gfx->line(gfx_ctx, x1, y0, x1 + 2, y0,
                         GRIDWIDTH, im->graph_col[GRC_GRID]);
                gfx->dashed_line(gfx_ctx, x0 - 1, y0,
                                x1 + 1, y0,
                                GRIDWIDTH,
                                im->graph_col[GRC_GRID],
                                im->grid_dash, 2, 0);
            }
        }
    }
    return 1;
}

/* this is frexp for base 10 */
static double frexp10(double x, double *e)
{
    int iexp = floor(log((double) fabs(x)) / log((double) 10));
    double mnt = x / pow(10.0, iexp);
    if (mnt >= 10.0) {
        iexp++;
        mnt = x / pow(10.0, iexp);
    }
    *e = iexp;
    return mnt;
}

/* logarithmic horizontal grid */
static int horizontal_log_grid(image_desc_t *im, graph_gfx_t *gfx, void *gfx_ctx)
{
    double yloglab[][10] = {
        { 1.0, 10.0,  0.0, 0.0,  0.0,  0.0, 0.0, 0.0, 0.0,  0.0},
        { 1.0,  5.0, 10.0, 0.0,  0.0,  0.0, 0.0, 0.0, 0.0,  0.0},
        { 1.0,  2.0,  5.0, 7.0, 10.0,  0.0, 0.0, 0.0, 0.0,  0.0},
        { 1.0,  2.0,  4.0, 6.0,  8.0, 10.0, 0.0, 0.0, 0.0,  0.0},
        { 1.0,  2.0,  3.0, 4.0,  5.0,  6.0, 7.0, 8.0, 9.0, 10.0},
        { 0.0,  0.0,  0.0, 0.0,  0.0,  0.0, 0.0, 0.0, 0.0,  0.0}  /* last line */
    };
//    int i, j;
    double y0;
    char graph_label[512];

    /* decade spacing */
    int exfrac = 1;
    /* number of decades in data */
    double nex = log10(im->maxval / im->minval);
    /* scale in logarithmic space */
    double logscale = im->ysize / nex;
    /* major spacing for data with high dynamic range */
    while (logscale * exfrac < 3 * im->text_prop[TEXT_PROP_LEGEND].size) {
        if (exfrac == 1)
            exfrac = 3;
        else
            exfrac += 3;
    }
    /* smallest major grid spacing (pixels) */
    double mspac = 0;
    /* row in yloglab for major grid */
    int mid = -1;
    /* major spacing for less dynamic data */
    do {
        /* search best row in yloglab */
        mid++;
        int i;
        for (i = 0; yloglab[mid][i + 1] < 10.0; i++);
        mspac = logscale * log10(10.0 / yloglab[mid][i]);
    } while (mspac > 2 * im->text_prop[TEXT_PROP_LEGEND].size && yloglab[mid][0] > 0);

    if (mid)
        mid--;
    /* find first value in yloglab */
    int flab = 0;
    double tmp = 0.0;
    while (yloglab[mid][flab] < 10 && frexp10(im->minval, &tmp) > yloglab[mid][flab])
        flab++;

    if (yloglab[mid][flab] == 10.0) {
        tmp += 1.0;
        flab = 0;
    }

    int val_exp = tmp;
    if (val_exp % exfrac)
        val_exp += abs(-val_exp % exfrac);
    double x0 = im->xorigin;
    double x1 = im->xorigin + im->xsize;
    /* draw grid */
    double pre_value = NAN;
    while (1) {
        double value = yloglab[mid][flab] * pow(10.0, val_exp);
        if (almostequal2scomplement(value, pre_value, 4))
            break;      /* it seems we are not converging */
        pre_value = value;
        y0 = ytr(im, value);
        if (floor(y0 + 0.5) <= (im->yorigin - im->ysize))
            break;
        /* major grid line */
        gfx->line(gfx_ctx, x0 - 2, y0, x0, y0, MGRIDWIDTH, im->graph_col[GRC_MGRID]);
        gfx->line(gfx_ctx, x1, y0, x1 + 2, y0, MGRIDWIDTH, im->graph_col[GRC_MGRID]);
        gfx->dashed_line(gfx_ctx, x0 - 2, y0, x1 + 2, y0, MGRIDWIDTH,
                         im->graph_col[GRC_MGRID], im->grid_dash, 2, 0);
        /* label */
        if (im->extra_flags & FORCE_UNITS_SI) {
            double    pvalue;
            char      symbol;

            int scale = floor(val_exp / 3.0);
            if (value >= 1.0)
                pvalue = pow(10.0, val_exp % 3);
            else
                pvalue = pow(10.0, ((val_exp + 1) % 3) + 2);
            pvalue *= yloglab[mid][flab];
            if (((scale + si_symbcenter) < (int) sizeof(si_symbol))
                && ((scale + si_symbcenter) >= 0))
                symbol = si_symbol[scale + si_symbcenter];
            else
                symbol = '?';
            snprintf(graph_label, sizeof graph_label, "%3.0f %c", pvalue, symbol);
        } else {
            snprintf(graph_label, sizeof graph_label, "%3.0e", value);
        }
        if (im->second_axis_scale != 0) {
            char graph_label_right[512];
            double sval = value * im->second_axis_scale + im->second_axis_shift;
            if ((im->second_axis_format == NULL) || (im->second_axis_format[0] == '\0')) {
                if (im->extra_flags & FORCE_UNITS_SI) {
                    double    mfac = 1;
                    char     *symb = "";

                    auto_scale(im, &sval, &symb, &mfac);
                    snprintf(graph_label_right, sizeof graph_label_right, "%4.0f %s", sval, symb);
                } else {
                    snprintf(graph_label_right, sizeof graph_label_right, "%3.0e", sval);
                }
            } else {
                snprintf(graph_label_right, sizeof graph_label_right,
                         im->second_axis_format, sval, "");
            }

            gfx->text(gfx_ctx,
                      x1 + 7, y0,
                      im->graph_col[GRC_FONT],
                      im->text_prop[TEXT_PROP_AXIS].font,
                      im->text_prop[TEXT_PROP_AXIS].size,
                      im->tabwidth, 0.0, GFX_H_LEFT, GFX_V_CENTER,
                      graph_label_right);
        }

        gfx->text(gfx_ctx,
                  x0 - im->text_prop[TEXT_PROP_AXIS].size, y0,
                  im->graph_col[GRC_FONT],
                  im->text_prop[TEXT_PROP_AXIS].font,
                  im->text_prop[TEXT_PROP_AXIS].size,
                  im->tabwidth, 0.0, GFX_H_RIGHT, GFX_V_CENTER, graph_label);
        /* minor grid */
        if (mid < 4 && exfrac == 1) {
            /* find first and last minor line behind current major line
             * i is the first line and j the last */
            int i, j, min_exp;
            if (flab == 0) {
                min_exp = val_exp - 1;
                for (i = 1; yloglab[mid][i] < 10.0; i++);
                i = yloglab[mid][i - 1] + 1;
                j = 10;
            } else {
                min_exp = val_exp;
                i = yloglab[mid][flab - 1] + 1;
                j = yloglab[mid][flab];
            }

            /* draw minor lines below current major line */
            for (; i < j; i++) {

                value = i * pow(10.0, min_exp);
                if (value < im->minval)
                    continue;
                y0 = ytr(im, value);
                if (floor(y0 + 0.5) <= im->yorigin - im->ysize)
                    break;
                /* draw lines */
                gfx->line(gfx_ctx, x0 - 2, y0, x0, y0, GRIDWIDTH, im->graph_col[GRC_GRID]);
                gfx->line(gfx_ctx, x1, y0, x1 + 2, y0, GRIDWIDTH, im->graph_col[GRC_GRID]);
                gfx->dashed_line(gfx_ctx, x0 - 1, y0, x1 + 1, y0, GRIDWIDTH, im->graph_col[GRC_GRID], im->grid_dash, 2, 0);
            }
        } else if (exfrac > 1) {
            for (int i = val_exp - exfrac / 3 * 2; i < val_exp; i += exfrac / 3) {
                value = pow(10.0, i);
                if (value < im->minval)
                    continue;
                y0 = ytr(im, value);
                if (floor(y0 + 0.5) <= im->yorigin - im->ysize)
                    break;
                /* draw lines */
                gfx->line(gfx_ctx, x0 - 2, y0, x0, y0, GRIDWIDTH, im->graph_col[GRC_GRID]);
                gfx->line(gfx_ctx, x1, y0, x1 + 2, y0, GRIDWIDTH, im->graph_col[GRC_GRID]);
                gfx->dashed_line(gfx_ctx, x0 - 1, y0, x1 + 1, y0, GRIDWIDTH, im->graph_col[GRC_GRID], im->grid_dash, 2, 0);
            }
        }

        /* next decade */
        if (yloglab[mid][++flab] == 10.0) {
            flab = 0;
            val_exp += exfrac;
        }
    }

    /* draw minor lines after highest major line */
    if (mid < 4 && exfrac == 1) {
        /* find first and last minor line below current major line
         * i is the first line and j the last */
        int i, j, min_exp;
        if (flab == 0) {
            min_exp = val_exp - 1;
            for (i = 1; yloglab[mid][i] < 10.0; i++);
            i = yloglab[mid][i - 1] + 1;
            j = 10;
        } else {
            min_exp = val_exp;
            i = yloglab[mid][flab - 1] + 1;
            j = yloglab[mid][flab];
        }

        /* draw minor lines below current major line */
        for (; i < j; i++) {

            double value = i * pow(10.0, min_exp);
            if (value < im->minval)
                continue;
            y0 = ytr(im, value);
            if (floor(y0 + 0.5) <= im->yorigin - im->ysize)
                break;
            /* draw lines */
            gfx->line(gfx_ctx, x0 - 2, y0, x0, y0, GRIDWIDTH, im->graph_col[GRC_GRID]);
            gfx->line(gfx_ctx, x1, y0, x1 + 2, y0, GRIDWIDTH, im->graph_col[GRC_GRID]);
            gfx->dashed_line(gfx_ctx, x0 - 1, y0, x1 + 1, y0, GRIDWIDTH,
                            im->graph_col[GRC_GRID], im->grid_dash, 2, 0);
        }
    }
    /* fancy minor gridlines */
    else if (exfrac > 1) {
        for (int i = val_exp - exfrac / 3 * 2; i < val_exp; i += exfrac / 3) {
            double value = pow(10.0, i);
            if (value < im->minval)
                continue;
            y0 = ytr(im, value);
            if (floor(y0 + 0.5) <= im->yorigin - im->ysize)
                break;
            /* draw lines */
            gfx->line(gfx_ctx, x0 - 2, y0, x0, y0, GRIDWIDTH, im->graph_col[GRC_GRID]);
            gfx->line(gfx_ctx, x1, y0,x1 + 2, y0, GRIDWIDTH, im->graph_col[GRC_GRID]);
            gfx->dashed_line(gfx_ctx, x0 - 1, y0, x1 + 1, y0, GRIDWIDTH,
                            im->graph_col[GRC_GRID], im->grid_dash, 2, 0);
        }
    }

    return 1;
}

static void vertical_grid(image_desc_t *im, graph_gfx_t *gfx, void *gfx_ctx)
{
    int       xlab_sel; /* which sort of label and grid ? */
    time_t    ti, tilab, timajor;
    double    factor;
    char      graph_label[100];
    struct tm tm;

    /* the type of time grid is determined by finding
       the number of seconds per pixel in the graph */
    if (im->xlab_user.minsec == -1.0) {
        factor = (double) (im->end - im->start) / (double) im->xsize;
        xlab_sel = 0;
        while (xlab[xlab_sel + 1].minsec != -1.0 && xlab[xlab_sel + 1].minsec <= factor) {
            xlab_sel++;
        }               /* pick the last one */
        while ((xlab_sel > 0 && xlab[xlab_sel - 1].minsec == xlab[xlab_sel].minsec) &&
               (xlab[xlab_sel].length > (im->end - im->start))) {
            xlab_sel--;
        } /* go back to the smallest size */
        im->xlab_user.gridtm = xlab[xlab_sel].gridtm;
        im->xlab_user.gridst = xlab[xlab_sel].gridst;
        im->xlab_user.mgridtm = xlab[xlab_sel].mgridtm;
        im->xlab_user.mgridst = xlab[xlab_sel].mgridst;
        im->xlab_user.labtm = xlab[xlab_sel].labtm;
        im->xlab_user.labst = xlab[xlab_sel].labst;
        im->xlab_user.precis = xlab[xlab_sel].precis;
        im->xlab_user.stst = xlab[xlab_sel].stst;
    }

    /* y coords are the same for every line ... */
    double y0 = im->yorigin;
    double y1 = im->yorigin - im->ysize;
    /* paint the minor grid */
    if (!(im->extra_flags & NOMINOR)) {
        for (ti = find_first_time(im->start,
                                  im->xlab_user.gridtm,
                                  im->xlab_user.gridst,
                                  im->extra_flags & FORCE_UTC_TIME),
             timajor = find_first_time(im->start,
                             im->xlab_user.mgridtm,
                             im->xlab_user.mgridst,
                             im->extra_flags & FORCE_UTC_TIME);
             ti < im->end && ti != -1;
             ti = find_next_time(ti,
                            im->xlab_user.gridtm,
                            im->xlab_user.gridst,
                            im->extra_flags & FORCE_UTC_TIME)
            ) {
            /* are we inside the graph ? */
            if (ti < im->start || ti > im->end)
                continue;
            while (timajor < ti && timajor != -1) {
                timajor = find_next_time(timajor,
                                         im->xlab_user.mgridtm,
                                         im->xlab_user.mgridst,
                                         im->extra_flags & FORCE_UTC_TIME);
            }
            if (timajor == -1)
                break;  /* fail in case of problems with time increments */
            if (ti == timajor)
                continue;   /* skip as falls on major grid line */
            double x0 = xtr(im, ti);
            gfx->line(gfx_ctx, x0, y1 - 2, x0, y1, GRIDWIDTH, im->graph_col[GRC_GRID]);
            gfx->line(gfx_ctx, x0, y0, x0, y0 + 2, GRIDWIDTH, im->graph_col[GRC_GRID]);
            gfx->dashed_line(gfx_ctx, x0, y0 + 1, x0,
                             y1 - 1, GRIDWIDTH,
                             im->graph_col[GRC_GRID],
                             im->grid_dash, 2, 0);
        }
    }

    /* paint the major grid */
    for (ti = find_first_time(im->start,
                              im->xlab_user.mgridtm,
                              im->xlab_user.mgridst,
                              im->extra_flags & FORCE_UTC_TIME);
         ti < im->end && ti != -1;
         ti = find_next_time(ti,
                             im->xlab_user.mgridtm,
                             im->xlab_user.mgridst,
                             im->extra_flags & FORCE_UTC_TIME)
        ) {
        /* are we inside the graph ? */
        if (ti < im->start || ti > im->end)
            continue;
        double x0 = xtr(im, ti);
        gfx->line(gfx_ctx, x0, y1 - 2, x0, y1, MGRIDWIDTH, im->graph_col[GRC_MGRID]);
        gfx->line(gfx_ctx, x0, y0, x0, y0 + 3, MGRIDWIDTH, im->graph_col[GRC_MGRID]);
        gfx->dashed_line(gfx_ctx, x0, y0 + 3, x0,
                         y1 - 2, MGRIDWIDTH,
                         im->graph_col
                         [GRC_MGRID], im->grid_dash, 2 , 0);
    }
    /* paint the labels below the graph */
    for (ti = find_first_time(im->start -
                         im->xlab_user.precis / 2,
                         im->xlab_user.labtm,
                         im->xlab_user.labst,
                         im->extra_flags & FORCE_UTC_TIME);
         (ti <= (im->end - im->xlab_user.precis / 2) && ti != -1);
         ti = find_next_time(ti,
                             im->xlab_user.labtm,
                             im->xlab_user.labst,
                             im->extra_flags & FORCE_UTC_TIME)
        ) {
        tilab = ti + im->xlab_user.precis / 2;  /* correct time for the label */
        /* are we inside the graph ? */
        if (tilab < im->start || tilab > im->end)
            continue;

        LOCALTIME_R(&tilab, &tm, im->extra_flags & FORCE_UTC_TIME);
        strftime(graph_label, 99, im->xlab_user.stst, &tm);

        gfx->text(gfx_ctx, xtr(im, tilab), y0 + 3,
                 im->graph_col[GRC_FONT],
                 im->text_prop[TEXT_PROP_AXIS].font,
                 im->text_prop[TEXT_PROP_AXIS].size,
                 im->tabwidth, 0.0, GFX_H_CENTER, GFX_V_TOP, graph_label);
    }

}


static void axis_paint(image_desc_t *im, graph_gfx_t *gfx, void *gfx_ctx)
{
    /* draw x and y axis */
    /* gfx_line ( im->canvas, im->xorigin+im->xsize,im->yorigin, im->xorigin+im->xsize,im->yorigin-im->ysize, GRIDWIDTH, im->graph_col[GRC_AXIS]);
       gfx_line ( im->canvas, im->xorigin,im->yorigin-im->ysize, im->xorigin+im->xsize,im->yorigin-im->ysize, GRIDWIDTH, im->graph_col[GRC_AXIS]); */

    gfx->line(gfx_ctx, im->xorigin - 4, im->yorigin,
                 im->xorigin + im->xsize + 4, im->yorigin,
                 MGRIDWIDTH, im->graph_col[GRC_AXIS]);
    gfx->line(gfx_ctx, im->xorigin, im->yorigin + 4,
                 im->xorigin, im->yorigin - im->ysize - 4,
                 MGRIDWIDTH, im->graph_col[GRC_AXIS]);

    /* arrow for X and Y axis direction */
    gfx->new_area(gfx_ctx, im->xorigin + im->xsize + 2, im->yorigin - 3,
                     im->xorigin + im->xsize + 2, im->yorigin + 3,
                     im->xorigin + im->xsize + 7, im->yorigin,  /* horizontal */
                     im->graph_col[GRC_ARROW]);
    gfx->close_path(gfx_ctx);

    gfx->new_area(gfx_ctx, im->xorigin - 3, im->yorigin - im->ysize - 2,
                     im->xorigin + 3, im->yorigin - im->ysize - 2,
                     im->xorigin, im->yorigin - im->ysize - 7,  /* vertical */
                     im->graph_col[GRC_ARROW]);
    gfx->close_path(gfx_ctx);

    if (im->second_axis_scale != 0) {
        gfx->line(gfx_ctx, im->xorigin + im->xsize, im->yorigin + 4,
                     im->xorigin + im->xsize, im->yorigin - im->ysize - 4,
                      MGRIDWIDTH, im->graph_col[GRC_AXIS]);
        gfx->new_area(gfx_ctx, im->xorigin + im->xsize - 2, im->yorigin - im->ysize - 2,
                         im->xorigin + im->xsize + 3, im->yorigin - im->ysize - 2,
                         im->xorigin + im->xsize, im->yorigin - im->ysize - 7,  /* LINEOFFSET */
                         im->graph_col[GRC_ARROW]);
        gfx->close_path(gfx_ctx);
    }

}

static image_title_t graph_title_split(const char *title)
{
    image_title_t retval;
    int       count = 0;    /* line count */
    char     *found_pos;
    int       found_size;


    retval.lines = malloc((MAX_IMAGE_TITLE_LINES + 1) * sizeof(char *));
    char     *delims[] = { "\n", "\\n", "<br>", "<br/>" };

    // printf("unsplit title: %s\n", title);

    char     *consumed = strdup(title); /* allocates copy */
    // FIXME free alloc
    do {
        /*
           search for next delimiter occurrence in the title,
           if found, save the string before the delimiter for the next line & search the remainder
         */
        found_pos = 0;
        found_size = 0;
        for (unsigned int i = 0; i < sizeof(delims) / sizeof(delims[0]); i++) {
            // get size of this delimiter
            int       delim_size = strlen(delims[i]);

            // find position of delimiter (0 = not found, otherwise ptr)
            char     *delim_pos = strstr(consumed, delims[i]);

            // do we have a delimiter pointer?
            if (delim_pos)
                // have we not found anything yet? or is this position lower than any
                // any previous ptr?
                if ((!found_pos) || (delim_pos < found_pos)) {
                    found_pos = delim_pos;
                    found_size = delim_size;
                }
        }

        // We previous found a delimiter so lets null terminate it
        if (found_pos)
            *found_pos = '\0';

        // ignore an empty line caused by a leading delimiter (or two in a row)
        if (found_pos != consumed) {
            // strdup allocated space for us, reuse that
            retval.lines[count] = consumed;
            ++count;

            consumed = found_pos;
        }

        // move the consumed pointer past any delimiter, so we can loop around again
        consumed = consumed + found_size;
    }
    // must not create more than MAX lines, so must stop splitting
    // either when we hit the max, or have no other delimiter
    while (found_pos && count < MAX_IMAGE_TITLE_LINES);

    retval.lines[count] = NULL;
    retval.count = count;

    return retval;
}


static int grid_paint(image_desc_t *im, graph_gfx_t *gfx, void *gfx_ctx)
{
    long i;
    int res = 0;
    int j = 0;
    double X0, Y0;   /* points for filled graph and more */
    struct image_title_t image_title;
    gfx_color_t water_color;

    if (im->draw_3d_border > 0) {
        /* draw 3d border */
        i = im->draw_3d_border;
        gfx->new_area(gfx_ctx, 0, im->yimg,
                      i, im->yimg - i, i, i, im->graph_col[GRC_SHADEA]);
        gfx->add_point(gfx_ctx, im->ximg - i, i);
        gfx->add_point(gfx_ctx, im->ximg, 0);
        gfx->add_point(gfx_ctx, 0, 0);
        gfx->close_path(gfx_ctx);
        gfx->new_area(gfx_ctx, i, im->yimg - i,
                      im->ximg - i,
                      im->yimg - i, im->ximg - i, i,
                      im->graph_col[GRC_SHADEB]);
        gfx->add_point(gfx_ctx, im->ximg, 0);
        gfx->add_point(gfx_ctx, im->ximg, im->yimg);
        gfx->add_point(gfx_ctx, 0, im->yimg);
        gfx->close_path(gfx_ctx);
    }

    if (im->draw_x_grid == 1)
        vertical_grid(im, gfx, gfx_ctx);

    if (im->draw_y_grid == 1) {
        if (im->logarithmic) {
            res = horizontal_log_grid(im, gfx, gfx_ctx);
        } else {
            res = draw_horizontal_grid(im, gfx, gfx_ctx);
            if (res < 0)
                return -1;
        }

        /* don't draw horizontal grid if there is no min and max val */
        if (!res) {
            char     *nodata = "No Data found";

            gfx->text(gfx_ctx, im->ximg / 2,
                     (2 * im->yorigin -
                      im->ysize) / 2,
                     im->graph_col[GRC_FONT],
                     im->text_prop[TEXT_PROP_AXIS].font,
                     im->text_prop[TEXT_PROP_AXIS].size,
                     im->tabwidth, 0.0, GFX_H_CENTER, GFX_V_CENTER, nodata);
        }
    }

    /* yaxis unit description */
    if (im->ylegend && im->ylegend[0] != '\0') {
        gfx->text(gfx_ctx,
                 im->xOriginLegendY + 10,
                 im->yOriginLegendY,
                 im->graph_col[GRC_FONT],
                 im->text_prop[TEXT_PROP_UNIT].font,
                 im->text_prop[TEXT_PROP_UNIT].size,
                 im->tabwidth,
                 RRDGRAPH_YLEGEND_ANGLE, GFX_H_CENTER, GFX_V_CENTER,
                 im->ylegend);

    }
    if (im->second_axis_legend && im->second_axis_legend[0] != '\0') {
        gfx->text(gfx_ctx,
                 im->xOriginLegendY2 + 10,
                 im->yOriginLegendY2,
                 im->graph_col[GRC_FONT],
                 im->text_prop[TEXT_PROP_UNIT].font,
                 im->text_prop[TEXT_PROP_UNIT].size,
                 im->tabwidth,
                 RRDGRAPH_YLEGEND_ANGLE,
                 GFX_H_CENTER, GFX_V_CENTER, im->second_axis_legend);
    }

    /* graph title */
    image_title = graph_title_split(im->title ? im->title : "");
    while (image_title.lines[j] != NULL) {
        gfx->text(gfx_ctx,
                 im->ximg / 2,
                 (im->text_prop[TEXT_PROP_TITLE].size * 1.3) +
                 (im->text_prop[TEXT_PROP_TITLE].size * 1.6 * j),
                 im->graph_col[GRC_FONT],
                 im->text_prop[TEXT_PROP_TITLE].font,
                 im->text_prop[TEXT_PROP_TITLE].size,
                 im->tabwidth, 0.0,
                 GFX_H_CENTER, GFX_V_TOP,
                 image_title.lines[j] ? image_title.lines[j] : "");
        j++;
    }
    // FIXME free lines

    /* graph watermark */
    if (im->watermark && im->watermark[0] != '\0') {
        water_color = im->graph_col[GRC_FONT];
        water_color.alpha = 0.3;
        gfx->text(gfx_ctx,
                 im->ximg / 2, im->yimg - 6,
                 water_color,
                 im->text_prop[TEXT_PROP_WATERMARK].font,
                 im->text_prop[TEXT_PROP_WATERMARK].size,
                 im->tabwidth,
                 0, GFX_H_CENTER, GFX_V_BOTTOM, im->watermark);
    }

    /* graph labels */
    if (!(im->extra_flags & NOLEGEND) && !(im->extra_flags & ONLY_GRAPH)) {
        long first_noncomment = im->gdes_c, last_noncomment = 0;

        /* get smallest and biggest leg_y values. Assumes
         * im->gdes[i].leg_y is in order. */
        double min = 0, max = 0;
        int gotcha = 0;

        for (i = 0; i < im->gdes_c; i++) {
            if (im->gdes[i].legend[0] == '\0')
                continue;
            if (!gotcha) {
                min = im->gdes[i].leg_y;
                gotcha = 1;
            }
            if (im->gdes[i].gf != GF_COMMENT) {
                if (im->legenddirection == BOTTOM_UP2)
                    min = im->gdes[i].leg_y;
                first_noncomment = i;
                break;
            }
        }
        gotcha = 0;
        for (i = im->gdes_c - 1; i >= 0; i--) {
            if (im->gdes[i].legend[0] == '\0')
                continue;
            if (!gotcha) {
                max = im->gdes[i].leg_y;
                gotcha = 1;
            }
            if (im->gdes[i].gf != GF_COMMENT) {
                if (im->legenddirection == BOTTOM_UP2)
                    max = im->gdes[i].leg_y;
                last_noncomment = i;
                break;
            }
        }
        for (i = 0; i < im->gdes_c; i++) {
            if (im->gdes[i].legend[0] == '\0')
                continue;
            /* im->gdes[i].leg_y is the bottom of the legend */
            X0 = im->xOriginLegend + im->gdes[i].leg_x;
            int       reverse = 0;

            switch (im->legenddirection) {
            case TOP_DOWN:
                reverse = 0;
                break;
            case BOTTOM_UP:
                reverse = 1;
                break;
            case BOTTOM_UP2:
                reverse = i >= first_noncomment && i <= last_noncomment;
                break;
            }
            Y0 = reverse ?
                im->yOriginLegend + max + min - im->gdes[i].leg_y :
                im->yOriginLegend + im->gdes[i].leg_y;
            gfx->text(gfx_ctx, X0, Y0,
                     im->graph_col[GRC_FONT],
                     im->text_prop[TEXT_PROP_LEGEND].font,
                     im->text_prop[TEXT_PROP_LEGEND].size,
                     im->tabwidth, 0.0,
                     GFX_H_LEFT, GFX_V_BOTTOM, im->gdes[i].legend);
            /* The legend for GRAPH items starts with "M " to have
               enough space for the box */
            if (im->gdes[i].gf != GF_PRINT &&
                im->gdes[i].gf != GF_GPRINT && im->gdes[i].gf != GF_COMMENT) {
                double    boxH, boxV;
                double    X1, Y1;

                boxH = gfx->get_text_width(gfx_ctx, 0,
                                          im->text_prop[TEXT_PROP_LEGEND].font,
                                          im->text_prop[TEXT_PROP_LEGEND].size,
                                          im->tabwidth, "o") * 1.2;
                boxV = boxH;
                /* shift the box up a bit */
                Y0 -= boxV * 0.4;


                if (im->dynamic_labels && im->gdes[i].gf == GF_HRULE) { /* [-] */
                    gfx->line(gfx_ctx,
                              X0, Y0 - boxV / 2,
                              X0 + boxH, Y0 - boxV / 2, 1.0, im->gdes[i].col);
                } else if (im->dynamic_labels && im->gdes[i].gf == GF_VRULE) {  /* [|] */
                    gfx->line(gfx_ctx,
                              X0 + boxH / 2, Y0,
                              X0 + boxH / 2, Y0 - boxV, 1.0, im->gdes[i].col);
                } else if (im->dynamic_labels && im->gdes[i].gf == GF_LINE) {   /* [/] */
                    gfx->line(gfx_ctx,
                              X0, Y0,
                              X0 + boxH, Y0 - boxV,
                              im->gdes[i].linewidth, im->gdes[i].col);
                } else {
                    /* make sure transparent colors show up the same way as in the graph */
                    gfx->new_area(gfx_ctx,
                                  X0, Y0 - boxV,
                                  X0, Y0, X0 + boxH, Y0,
                                  im->graph_col[GRC_BACK]);
                    gfx->add_point(gfx_ctx, X0 + boxH, Y0 - boxV);
                    gfx->close_path(gfx_ctx);

                    gfx->new_area(gfx_ctx, X0, Y0 - boxV, X0,
                                  Y0, X0 + boxH, Y0, im->gdes[i].col);
                    gfx->add_point(gfx_ctx, X0 + boxH, Y0 - boxV);
                    gfx->close_path(gfx_ctx);

                    if (im->gdes[i].dash) {
                        /* make box borders in legend dashed if the graph is dashed */
                        double dashes[] = { 3.0 };
                        gfx->new_dashed_path(gfx_ctx, 1.0, im->graph_col[GRC_FRAME], dashes, 1, 0.0);
                    } else {
                        gfx->new_path(gfx_ctx, 1.0, im->graph_col[GRC_FRAME]);
                    }
                    X1 = X0 + boxH;
                    Y1 = Y0 - boxV;
                    //gfx->line_fit(gfx_ctx, &X0, &Y0);
                    //gfx->line_fit(gfx_ctx, &X1, &Y1);
                    gfx->move_to(gfx_ctx, X0, Y0);
                    gfx->line_to(gfx_ctx, X1, Y0);
                    gfx->line_to(gfx_ctx, X1, Y1);
                    gfx->line_to(gfx_ctx, X0, Y1);
                    gfx->close_path(gfx_ctx);
                }
            }
        }
    }
    return 0;
}

static int graph_size_location(image_desc_t *im, graph_gfx_t *gfx, void *gfx_ctx, int elements)
{
    /* The actual size of the image to draw is determined from
     ** several sources.  The size given on the command line is
     ** the graph area but we need more as we have to draw labels
     ** and other things outside the graph area. If the option
     ** --full-size-mode is selected the size defines the total
     ** image size and the size available for the graph is
     ** calculated.
     */

    /** +---+-----------------------------------+
     ** | y |...............graph title.........|
     ** |   +---+-------------------------------+
     ** | a | y |                               |
     ** | x |   |                               |
     ** | i | a |                               |
     ** | s | x |       main graph area         |
     ** |   | i |                               |
     ** | t | s |                               |
     ** | i |   |                               |
     ** | t | l |                               |
     ** | l | b +-------------------------------+
     ** | e | l |       x axis labels           |
     ** +---+---+-------------------------------+
     ** |....................legends............|
     ** +---------------------------------------+
     ** |                   watermark           |
     ** +---------------------------------------+
     */

    int xvertical = 0;
    int xvertical2 = 0;
    int ytitle = 0;
    int Xylabel = 0;
    int Xmain = 0;
    int Ymain = 0;
    int Yxlabel = 0;
    int Xspacing = 15;
    int Yspacing = 15;
    int Ywatermark = 4;

    // no legends and no the shall be plotted it's easy
    if (im->extra_flags & ONLY_GRAPH) {
        im->xorigin = 0;
        im->ximg = im->xsize;
        im->yimg = im->ysize;
        im->yorigin = im->ysize;
        xtr(im, 0);
        ytr(im, NAN);
        return 0;
    }

    if (im->watermark && im->watermark[0] != '\0') {
        Ywatermark = im->text_prop[TEXT_PROP_WATERMARK].size * 2;
    }

    // calculate the width of the left vertical legend
    if (im->ylegend && im->ylegend[0] != '\0') {
        xvertical = im->text_prop[TEXT_PROP_UNIT].size * 2;
    }

    // calculate the width of the right vertical legend
    if (im->second_axis_legend && im->second_axis_legend[0] != '\0') {
        xvertical2 = im->text_prop[TEXT_PROP_UNIT].size * 2;
    } else {
        xvertical2 = Xspacing;
    }

    if (im->title && im->title[0] != '\0') {
        /* The title is placed "in between" two text lines so it
         ** automatically has some vertical spacing.  The horizontal
         ** spacing is added here, on each side.
         */
        /* if necessary, reduce the font size of the title until it fits the image width */
        image_title_t image_title = graph_title_split(im->title);

        ytitle = im->text_prop[TEXT_PROP_TITLE].size * (image_title.count + 1) * 1.6;
        // FIXME free image_title.lines
    } else {
        // we have no title; get a little clearing from the top
        ytitle = Yspacing;
    }

    if (elements) {
        if (im->draw_x_grid) {
            // calculate the height of the horizontal labelling
            Yxlabel = im->text_prop[TEXT_PROP_AXIS].size * 2.5;
        }
        if (im->draw_y_grid || im->forceleftspace) {
            // calculate the width of the vertical labelling
            Xylabel = gfx->get_text_width(gfx_ctx, 0,
                                   im->text_prop[TEXT_PROP_AXIS].font,
                                   im->text_prop[TEXT_PROP_AXIS].size,
                                   im->tabwidth, "0") * im->unitslength;
        }
    }

    // add some space to the labelling
    Xylabel += Xspacing;

    /* If the legend is printed besides the graph the width has to be
     ** calculated first. Placing the legend north or south of the
     ** graph requires the width calculation first, so the legend is
     ** skipped for the moment.
     */
    im->legendheight = 0;
    im->legendwidth = 0;
    if (!(im->extra_flags & NOLEGEND)) {
        if (im->legendposition == WEST || im->legendposition == EAST) {
            if (leg_place(im, gfx, gfx_ctx, 1) == -1) {
                return -1;
            }
        }
    }

    if (im->extra_flags & FULL_SIZE_MODE) {

        /* The actual size of the image to draw has been determined by the user.
         ** The graph area is the space remaining after accounting for the legend,
         ** the watermark, the axis labels, and the title.
         */
        im->ximg = im->xsize;
        im->yimg = im->ysize;
        Xmain = im->ximg;
        Ymain = im->yimg;

        /* Now calculate the total size.  Insert some spacing where
           desired.  im->xorigin and im->yorigin need to correspond
           with the lower left corner of the main graph area or, if
           this one is not set, the imaginary box surrounding the
           pie chart area. */
        /* Initial size calculation for the main graph area */

        Xmain -= Xylabel;   // + Xspacing;
        if ((im->legendposition == WEST || im->legendposition == EAST)
            && !(im->extra_flags & NOLEGEND)) {
            Xmain -= im->legendwidth;   // + Xspacing;
        }
        if (im->second_axis_scale != 0) {
            Xmain -= Xylabel;
        }

        Xmain -= xvertical + xvertical2;

        /* limit the remaining space to 0 */
        if (Xmain < 1) {
            Xmain = 1;
        }
        im->xsize = Xmain;

        /* Putting the legend north or south, the height can now be calculated */
        if (!(im->extra_flags & NOLEGEND)) {
            if (im->legendposition == NORTH || im->legendposition == SOUTH) {
                im->legendwidth = im->ximg;
                if (leg_place(im, gfx, gfx_ctx, 0) == -1) {
                    return -1;
                }
            }
        }

        if ((im->legendposition == NORTH || im->legendposition == SOUTH)
            && !(im->extra_flags & NOLEGEND)) {
            Ymain -= Yxlabel + im->legendheight;
        } else {
            Ymain -= Yxlabel;
        }

        /* reserve space for the title *or* some padding above the graph */
        Ymain -= ytitle;

        /* reserve space for padding below the graph */
        if (im->extra_flags & NOLEGEND) {
            Ymain -= 0.5 * Yspacing;
        }

        if (im->watermark && im->watermark[0] != '\0') {
            Ymain -= Ywatermark;
        }
        /* limit the remaining height to 0 */
        if (Ymain < 1) {
            Ymain = 1;
        }
        im->ysize = Ymain;
    } else {
        /* dimension options -width and -height refer to the dimensions of the main graph area */

        /* The actual size of the image to draw is determined from
         ** several sources.  The size given on the command line is
         ** the graph area but we need more as we have to draw labels
         ** and other things outside the graph area.
         */

        if (elements) {
            Xmain = im->xsize;  // + Xspacing;
            Ymain = im->ysize;
        }

        im->ximg = Xmain + Xylabel;

        if ((im->legendposition == WEST || im->legendposition == EAST)
            && !(im->extra_flags & NOLEGEND)) {
            im->ximg += im->legendwidth;    // + Xspacing;
        }
        if (im->second_axis_scale != 0) {
            im->ximg += Xylabel;
        }

        im->ximg += xvertical + xvertical2;

        if (!(im->extra_flags & NOLEGEND)) {
            if (im->legendposition == NORTH || im->legendposition == SOUTH) {
                im->legendwidth = im->ximg;
                if (leg_place(im, gfx, gfx_ctx, 0) == -1) {
                    return -1;
                }
            }
        }

        im->yimg = Ymain + Yxlabel;
        if ((im->legendposition == NORTH || im->legendposition == SOUTH)
            && !(im->extra_flags & NOLEGEND)) {
            im->yimg += im->legendheight;
        }

        /* reserve space for the title *or* some padding above the graph */
        if (ytitle) {
            im->yimg += ytitle;
        } else {
            im->yimg += 1.5 * Yspacing;
        }
        /* reserve space for padding below the graph */
        if (im->extra_flags & NOLEGEND) {
            im->yimg += 0.5 * Yspacing;
        }

        if (im->watermark && im->watermark[0] != '\0') {
            im->yimg += Ywatermark;
        }
    }


    /* In case of putting the legend in west or east position the first
     ** legend calculation might lead to wrong positions if some items
     ** are not aligned on the left hand side (e.g. centered) as the
     ** legendwidth might have been increased after the item was placed.
     ** In this case the positions have to be recalculated.
     */
    if (!(im->extra_flags & NOLEGEND)) {
        if (im->legendposition == WEST || im->legendposition == EAST) {
            if (leg_place(im, gfx, gfx_ctx, 0) == -1) {
                return -1;
            }
        }
    }

    /* After calculating all dimensions
     ** it is now possible to calculate
     ** all offsets.
     */
    switch (im->legendposition) {
        case NORTH:
            im->xOriginTitle = (im->ximg / 2);
            im->yOriginTitle = 0;

            im->xOriginLegend = 0;
            im->yOriginLegend = ytitle;

            im->xOriginLegendY = 0;
            im->yOriginLegendY = ytitle + im->legendheight + (Ymain / 2) + Yxlabel;

            im->xorigin = xvertical + Xylabel;
            im->yorigin = ytitle + im->legendheight + Ymain;

            im->xOriginLegendY2 = xvertical + Xylabel + Xmain;
            if (im->second_axis_scale != 0)
                im->xOriginLegendY2 += Xylabel;
            im->yOriginLegendY2 = ytitle + im->legendheight + (Ymain / 2) + Yxlabel;
            break;
        case WEST:
            im->xOriginTitle = im->legendwidth + im->xsize / 2;
            im->yOriginTitle = 0;

            im->xOriginLegend = 0;
            im->yOriginLegend = ytitle;

            im->xOriginLegendY = im->legendwidth;
            im->yOriginLegendY = ytitle + (Ymain / 2);

            im->xorigin = im->legendwidth + xvertical + Xylabel;
            im->yorigin = ytitle + Ymain;

            im->xOriginLegendY2 = im->legendwidth + xvertical + Xylabel + Xmain;
            if (im->second_axis_scale != 0)
                im->xOriginLegendY2 += Xylabel;
            im->yOriginLegendY2 = ytitle + (Ymain / 2);
            break;
        case SOUTH:
            im->xOriginTitle = im->ximg / 2;
            im->yOriginTitle = 0;

            im->xOriginLegend = 0;
            im->yOriginLegend = ytitle + Ymain + Yxlabel;

            im->xOriginLegendY = 0;
            im->yOriginLegendY = ytitle + (Ymain / 2);

            im->xorigin = xvertical + Xylabel;
            im->yorigin = ytitle + Ymain;

            im->xOriginLegendY2 = xvertical + Xylabel + Xmain;
            if (im->second_axis_scale != 0)
                im->xOriginLegendY2 += Xylabel;
            im->yOriginLegendY2 = ytitle + (Ymain / 2);
            break;
        case EAST:
            im->xOriginTitle = im->xsize / 2;
            im->yOriginTitle = 0;

            im->xOriginLegend = xvertical + Xylabel + Xmain + xvertical2;
            if (im->second_axis_scale != 0)
                im->xOriginLegend += Xylabel;
            im->yOriginLegend = ytitle;

            im->xOriginLegendY = 0;
            im->yOriginLegendY = ytitle + (Ymain / 2);

            im->xorigin = xvertical + Xylabel;
            im->yorigin = ytitle + Ymain;

            im->xOriginLegendY2 = xvertical + Xylabel + Xmain;
            if (im->second_axis_scale != 0)
                im->xOriginLegendY2 += Xylabel;
            im->yOriginLegendY2 = ytitle + (Ymain / 2);
            break;
    }

    xtr(im, 0);
    ytr(im, NAN);
    return 0;
}

static int graph_paint_hrule(image_desc_t *im, graph_desc_t *gdes, graph_gfx_t *gfx, void *gfx_ctx)
{
    if ((gdes->yrule >= im->minval) && (gdes->yrule <= im->maxval)) {
        if (gdes->dash) {
            gfx->dashed_line(gfx_ctx, im->xorigin,
                             ytr(im, gdes->yrule),
                             im->xorigin + im->xsize,
                             ytr(im, gdes->yrule), 1.0, gdes->col,
                             gdes->p_dashes, gdes->ndash, gdes->offset);
        } else {
            gfx->line(gfx_ctx, im->xorigin,
                     ytr(im, gdes->yrule),
                     im->xorigin + im->xsize,
                     ytr(im, gdes->yrule), 1.0, gdes->col);
        }
    }
    return 0;
}

static int graph_paint_vrule(image_desc_t *im, graph_desc_t *gdes, graph_gfx_t *gfx, void *gfx_ctx)
{
    if ((gdes->xrule >= im->start) && (gdes->xrule <= im->end)) {
        if (gdes->dash) {
            gfx->dashed_line(gfx_ctx,
                             xtr(im, gdes->xrule),
                             im->yorigin, xtr(im, gdes->xrule),
                             im->yorigin - im->ysize, 1.0, gdes->col,
                             gdes->p_dashes, gdes->ndash, gdes->offset);
        } else {
            gfx->line(gfx_ctx,
                      xtr(im, gdes->xrule),
                      im->yorigin, xtr(im, gdes->xrule),
                      im->yorigin - im->ysize, 1.0, gdes->col);
        }
    }
    return 0;
}

static int graph_paint_tick(image_desc_t *im, graph_desc_t *gdes, graph_gfx_t *gfx, void *gfx_ctx)
{
    for (int i = 0; i < im->xsize; i++) {
        if (!isnan(gdes->p_data[i]) && (gdes->p_data[i] != 0.0)) {
            if (gdes->yrule > 0) {
                gfx->line(gfx_ctx,
                         im->xorigin + i,
                         im->yorigin + 1.0,
                         im->xorigin + i,
                         im->yorigin -
                         gdes->yrule *
                         im->ysize, 1.0, gdes->col);
            } else if (gdes->yrule < 0) {
                gfx->line(gfx_ctx,
                         im->xorigin + i,
                         im->yorigin - im->ysize - 1.0,
                         im->xorigin + i,
                         im->yorigin - im->ysize -
                         gdes->yrule *
                         im->ysize, 1.0, gdes->col);
            }
        }
    }
    return 0;
}

static int graph_paint_line(image_desc_t *im, graph_desc_t *gdes, graph_gfx_t *gfx, void *gfx_ctx, graph_desc_t **lastgdes)
{
    rrd_value_t diffval = im->maxval - im->minval;
    rrd_value_t maxlimit = im->maxval + 9 * diffval;
    rrd_value_t minlimit = im->minval - 9 * diffval;

    for (int ii = 0; ii < im->xsize; ii++) {
        /* fix data points at oo and -oo */
        if (isinf(gdes->p_data[ii])) {
            if (gdes->p_data[ii] > 0) {
                gdes->p_data[ii] = im->maxval;
            } else {
                gdes->p_data[ii] = im->minval;
            }
        }
        /* some versions of cairo go unstable when trying
           to draw way out of the canvas ... lets not even try */
        if (gdes->p_data[ii] > maxlimit) {
            gdes->p_data[ii] = maxlimit;
        }
        if (gdes->p_data[ii] < minlimit) {
            gdes->p_data[ii] = minlimit;
        }
    }

    double areazero = 0.0;
    if (im->minval > 0.0)
        areazero = im->minval;
    if (im->maxval < 0.0)
        areazero = im->maxval;

    /* *******************************************************
       a           ___. (a,t)
       |   |    ___
       ____|   |   |   |
       |       |___|
       -------|--t-1--t--------------------------------

       if we know the value at time t was a then
       we draw a square from t-1 to t with the value a.

       ********************************************************* */
    if (gdes->col.alpha != 0.0) {
        double last_y = 0.0;
        int draw_on = 0;

        if (gdes->dash) {
            gfx->new_dashed_path(gfx_ctx, gdes->linewidth, gdes->col, gdes->p_dashes, gdes->ndash, gdes->offset);
        } else {
            gfx->new_path(gfx_ctx, gdes->linewidth, gdes->col);
        }

        for (int ii = 1; ii < im->xsize; ii++) {
            if (isnan(gdes->p_data[ii]) || ((im->slopemode == 1) && isnan(gdes->p_data[ii - 1]))) {
                draw_on = 0;
                continue;
            }
            if (draw_on == 0) {
                last_y = ytr(im, gdes->p_data[ii]);
                if (im->slopemode == 0) {
                    double x = ii - 1 + im->xorigin;
                    double y = last_y;

                    //gfx->line_fit(gfx_ctx, &x, &y);
                    gfx->move_to(gfx_ctx, x, y);
                    x = ii + im->xorigin;
                    y = last_y;
                    //gfx->line_fit(gfx_ctx, &x, &y);
                    gfx->line_to(gfx_ctx, x, y);
                } else {
                    double x = ii - 1 + im->xorigin;
                    double y = ytr(im, gdes->p_data[ii - 1]);
                    //gfx->line_fit(gfx_ctx, &x, &y);
                    gfx->move_to(gfx_ctx, x, y);
                    x = ii + im->xorigin;
                    y = last_y;
                    //gfx->line_fit(gfx_ctx, &x, &y);
                    gfx->line_to(gfx_ctx, x, y);
                }
                draw_on = 1;
            } else {
                double x1 = ii + im->xorigin;
                double y1 = ytr(im, gdes->p_data[ii]);

                if ((im->slopemode) == 0 && !almostequal2scomplement(y1, last_y, 4)) {
                    double x = ii - 1 + im->xorigin;
                    double y = y1;

                    //gfx->line_fit(gfx_ctx, &x, &y);
                    gfx->line_to(gfx_ctx, x, y);
                };
                last_y = y1;
                //gfx->line_fit(gfx_ctx, &x1, &y1);
                gfx->line_to(gfx_ctx, x1, y1);
            };
        }
        gfx->close_path(gfx_ctx);
//        gfx->set_line_cap(gfx_ctx, CAIRO_LINE_CAP_ROUND); // FIXME
//        gfx->set_line_join(gfx_ctx, CAIRO_LINE_JOIN_ROUND); // FIXME
    }

    /* if color != 0x0 */
    /* make sure we do not run into trouble when stacking on NaN */
    for (int ii = 0; ii < im->xsize; ii++) {
        if (isnan(gdes->p_data[ii])) {
            if ((lastgdes != NULL) && (*lastgdes != NULL) && (gdes->stack)) {
                gdes->p_data[ii] = (*lastgdes)->p_data[ii];
            } else {
                gdes->p_data[ii] = areazero;
            }
        }
    }

    if (lastgdes != NULL)
        *lastgdes = gdes;

    return 0;
}

static int graph_paint_area(image_desc_t *im, graph_desc_t *gdes, graph_gfx_t *gfx, void *gfx_ctx, graph_desc_t **lastgdes)
{
    rrd_value_t diffval = im->maxval - im->minval;
    rrd_value_t maxlimit = im->maxval + 9 * diffval;
    rrd_value_t minlimit = im->minval - 9 * diffval;

    for (int ii = 0; ii < im->xsize; ii++) {
        /* fix data points at oo and -oo */
        if (isinf(gdes->p_data[ii])) {
            if (gdes->p_data[ii] > 0) {
                gdes->p_data[ii] = im->maxval;
            } else {
                gdes->p_data[ii] = im->minval;
            }
        }
        /* some versions of cairo go unstable when trying
           to draw way out of the canvas ... lets not even try */
        if (gdes->p_data[ii] > maxlimit) {
            gdes->p_data[ii] = maxlimit;
        }
        if (gdes->p_data[ii] < minlimit) {
            gdes->p_data[ii] = minlimit;
        }
    }           /* for */

    double areazero = 0.0;
    if (im->minval > 0.0)
        areazero = im->minval;
    if (im->maxval < 0.0)
        areazero = im->maxval;

    /* *******************************************************
       a           ___. (a,t)
       |   |    ___
       ____|   |   |   |
       |       |___|
       -------|--t-1--t--------------------------------

       if we know the value at time t was a then
       we draw a square from t-1 to t with the value a.

       ********************************************************* */
    if (gdes->col.alpha != 0.0) {
        double lastx = 0;
        double lasty = 0;
        int isArea = isnan(gdes->col2.red);
        int idxI = -1;
        double *foreY = (double *) malloc(sizeof(double) * im->xsize * 2);
        double *foreX = (double *) malloc(sizeof(double) * im->xsize * 2);
        double *backY = (double *) malloc(sizeof(double) * im->xsize * 2);
        double *backX = (double *) malloc(sizeof(double) * im->xsize * 2);
        int drawem = 0;

        for (int ii = 0; ii <= im->xsize; ii++) {
            double ybase, ytop;

            if (idxI > 0 && (drawem != 0 || ii == im->xsize)) {
                int cntI = 1;
                int lastI = 0;

                while ((cntI < idxI) &&
                       almostequal2scomplement(foreY [lastI], foreY[cntI], 4) &&
                       almostequal2scomplement(foreY [lastI], foreY[cntI + 1], 4)) {
                    cntI++;
                }
                if (isArea) {
                    gfx->new_area(gfx_ctx,
                                 backX[0], backY[0],
                                 foreX[0], foreY[0],
                                 foreX[cntI],
                                 foreY[cntI], gdes->col);
                } else {
                    lastx = foreX[cntI];
                    lasty = foreY[cntI];
                }
                while (cntI < idxI) {
                    lastI = cntI;
                    cntI++;
                    while ((cntI < idxI) &&
                           almostequal2scomplement(foreY [lastI], foreY[cntI], 4) &&
                           almostequal2scomplement(foreY [lastI], foreY[cntI + 1], 4)) {
                        cntI++;
                    }
                    if (isArea) {
                        gfx->add_point(gfx_ctx, foreX[cntI], foreY[cntI]);
                    } else {
                        gfx->add_rect_fadey(gfx_ctx,
                                           lastx, foreY[0],
                                           foreX[cntI], foreY[cntI], lasty,
                                           gdes->col, gdes->col2, gdes->gradheight);
                        lastx = foreX[cntI];
                        lasty = foreY[cntI];
                    }
                }
                if (isArea) {
                    gfx->add_point(im, backX[idxI], backY[idxI]);
                } else {
                    gfx->add_rect_fadey(gfx_ctx, lastx, foreY[0],
                                        backX[idxI], backY[idxI], lasty,
                                        gdes->col, gdes->col2, gdes->gradheight);
                    lastx = backX[idxI];
                    lasty = backY[idxI];
                }
                while (idxI > 1) {
                    lastI = idxI;
                    idxI--;
                    while ((idxI > 1) &&
                           almostequal2scomplement(backY [lastI], backY[idxI], 4) &&
                           almostequal2scomplement(backY [lastI], backY[idxI - 1], 4)) {
                        idxI--;
                    }
                    if (isArea) {
                        gfx->add_point(gfx_ctx, backX[idxI], backY[idxI]);
                    } else {
                        gfx->add_rect_fadey(gfx_ctx,
                                           lastx, foreY[0],
                                           backX[idxI],
                                           backY[idxI], lasty,
                                           gdes->col,
                                           gdes->col2,
                                           gdes->gradheight);
                        lastx = backX[idxI];
                        lasty = backY[idxI];
                    }
                }
                idxI = -1;
                drawem = 0;
                if (isArea)
                    gfx->close_path(gfx_ctx);
            }
            if (drawem != 0) {
                drawem = 0;
                idxI = -1;
            }
            if (ii == im->xsize)
                break;
            if (im->slopemode == 0 && ii == 0) {
                continue;
            }
            if (isnan(gdes->p_data[ii])) {
                drawem = 1;
                continue;
            }
            ytop = ytr(im, gdes->p_data[ii]);
            if (*lastgdes && gdes->stack) {
                ybase = ytr(im, (*lastgdes)->p_data[ii]);
            } else {
                ybase = ytr(im, areazero);
            }
            if (ybase == ytop) {
                drawem = 1;
                continue;
            }

            if (ybase > ytop) {
                double    extra = ytop;

                ytop = ybase;
                ybase = extra;
            }
            if (im->slopemode == 0) {
                backY[++idxI] = ybase - 0.2;
                backX[idxI] = ii + im->xorigin - 1;
                foreY[idxI] = ytop + 0.2;
                foreX[idxI] = ii + im->xorigin - 1;
            }
            backY[++idxI] = ybase - 0.2;
            backX[idxI] = ii + im->xorigin;
            foreY[idxI] = ytop + 0.2;
            foreX[idxI] = ii + im->xorigin;
        }
        /* close up any remaining area */
        free(foreY);
        free(foreX);
        free(backY);
        free(backX);
    }

    /* if color != 0x0 */
    /* make sure we do not run into trouble when stacking on NaN */
    for (int ii = 0; ii < im->xsize; ii++) {
        if (isnan(gdes->p_data[ii])) {
            if ((lastgdes != NULL) && (*lastgdes != NULL) && (gdes->stack)) {
                gdes->p_data[ii] = (*lastgdes)->p_data[ii];
            } else {
                gdes->p_data[ii] = areazero;
            }
        }
    }

    if (lastgdes != NULL)
        *lastgdes = gdes;

    return 0;
}


static int graph_paint_setup(image_desc_t *im, graph_gfx_t *gfx, void *gfx_ctx)
{
    if (gfx->setup(gfx_ctx, im->ximg * im->zoom, im->yimg * im->zoom))
       return -1;

//  gfx->scale(gfx_ctx, im->zoom, im->zoom); FIXME
//  pango_gfx->font_map_set_resolution(PANGO_CAIRO_FONT_MAP(font_map), 100);

    gfx->new_area(gfx_ctx, 0, 0,
                           0, im->yimg,
                           im->ximg, im->yimg,
                           im->graph_col[GRC_BACK]);
    gfx->add_point(gfx_ctx, im->ximg, 0);
    gfx->close_path(gfx_ctx);

    gfx->new_area(gfx_ctx, im->xorigin, im->yorigin,
                           im->xorigin + im->xsize, im->yorigin,
                           im->xorigin + im->xsize, im->yorigin - im->ysize,
                           im->graph_col[GRC_CANVAS]);

    gfx->add_point(gfx_ctx, im->xorigin, im->yorigin - im->ysize);
    gfx->close_path(gfx_ctx);

    gfx->rectangle(gfx_ctx, im->xorigin, im->yorigin - im->ysize - 1.0,
                            im->xsize, im->ysize + 2.0, 1.0, NULL);

    return 0;
}

static int graph_paint_timestring(image_desc_t *im, graph_gfx_t *gfx, void *gfx_ctx, int cnt)
{
    if (graph_size_location(im, gfx, gfx_ctx, cnt) == -1)
        return -1;

    /* get actual drawing data and find min and max values */
    if (data_proc(im) == -1)
        return -1;

    /* identify si magnitude Kilo, Mega Giga ? */
    if (!im->logarithmic)
        si_unit(im);

    /* make sure the upper and lower limit are sensible values
       if rigid is without alow_shrink skip expanding limits */
    if ((!im->rigid || im->allow_shrink) && !im->logarithmic)
        expand_range(im);

    if (!calc_horizontal_grid(im))
        return -1;
    /* reset precalc */
    ytr(im, NAN);

    if (im->gridfit && 0) // FIXME
     apply_gridfit(im);

    if (graph_paint_setup(im, gfx, gfx_ctx))
        return -1;

#if 0
    /* other stuff */
    double areazero = 0.0;
    if (im->minval > 0.0)
        areazero = im->minval;
    if (im->maxval < 0.0)
        areazero = im->maxval;
#endif
    graph_desc_t *lastgdes = NULL;
    for (int i = 0; i < im->gdes_c; i++) {
        switch (im->gdes[i].gf) {
        case GF_CDEF:
        case GF_VDEF:
        case GF_DEF:
        case GF_PRINT:
        case GF_GPRINT:
        case GF_COMMENT:
        case GF_TEXTALIGN:
        case GF_HRULE:
        case GF_VRULE:
        case GF_XPORT:
        case GF_SHIFT:
        case GF_XAXIS:
        case GF_YAXIS:
            break;
        case GF_TICK:
            graph_paint_tick(im, &im->gdes[i], gfx, gfx_ctx);
            break;
        case GF_LINE:
            graph_paint_line(im, &im->gdes[i], gfx, gfx_ctx, &lastgdes);
            break;
        case GF_AREA:
            graph_paint_area(im, &im->gdes[i], gfx, gfx_ctx, &lastgdes);
            break;
        }
    }
//    gfx->reset_clip(gfx_ctx);

    /* grid_paint also does the text */
    if (!(im->extra_flags & ONLY_GRAPH))
        if (grid_paint(im, gfx, gfx_ctx))
            return -1;
    if (!(im->extra_flags & ONLY_GRAPH))
        axis_paint(im, gfx, gfx_ctx);

    /* the RULES are the last thing to paint ... */
    for (int i = 0; i < im->gdes_c; i++) {
        switch (im->gdes[i].gf) {
            case GF_HRULE:
                graph_paint_hrule(im, &im->gdes[i], gfx, gfx_ctx);
                break;
            case GF_VRULE:
                graph_paint_vrule(im, &im->gdes[i], gfx, gfx_ctx);
                break;
            default:
                break;
        }
    }

    return gfx->finish(gfx_ctx);
}

/* draw that picture thing ... */
int graph_paint(image_desc_t *im, graph_gfx_t *gfx, void *gfx_ctx)
{
    int       cnt;

    /* pull the data from the rrd files ... */
#if 0
    if (data_fetch(im) != 0)
        return -1;
#endif
#if 0
    /* evaluate VDEF and CDEF operations ... */
    if (data_calc(im) == -1)
        return -1;
#endif
    /* calculate and PRINT and GPRINT definitions. We have to do it at
     * this point because it will affect the length of the legends
     * if there are no graph elements (i==0) we stop here ...
     */
    cnt = print_calc(im);
//    if (cnt < 0)
//        return -1;

    /* if we want and can be lazy ... quit now */
//    if (cnt == 0)
//      return 0;

    return graph_paint_timestring(im, gfx, gfx_ctx, cnt+1); // FIXME +1
}

#if 0
int graph_gfx->setup(image_desc_t *im)
{
    /* the actual graph is created by going through the individual
       graph elements and then drawing them */
    gfx->surface_destroy(im->surface);
    switch (im->imgformat) {
    case IF_PNG:
        im->surface =
            gfx->image_surface_create(CAIRO_FORMAT_ARGB32,
                                       im->ximg * im->zoom,
                                       im->yimg * im->zoom);
        break;
#ifdef CAIRO_HAS_SVG_SURFACE
    case IF_SVG:
        im->gridfit = 0;
        im->surface = gfx->svg_surface_create_for_stream(&gfx->output, im, im->ximg * im->zoom, im->yimg * im->zoom);
        gfx->svg_surface_restrict_to_version
            (im->surface, CAIRO_SVG_VERSION_1_1);
        break;
#endif
    case IF_XML:
    case IF_XMLENUM:
    case IF_CSV:
    case IF_TSV:
    case IF_SSV:
    case IF_JSON:
    case IF_JSONTIME:
        break;
    };
    gfx->destroy(gfx_ctx);
    gfx_ctx = gfx->create(im->surface);
    gfx->set_antialias(gfx_ctx, im->graph_antialias);
    gfx->scale(gfx_ctx, im->zoom, im->zoom);
//    pango_gfx->font_map_set_resolution(PANGO_CAIRO_FONT_MAP(font_map), 100);
    gfx_new_area(im, 0, 0, 0, im->yimg,
                 im->ximg, im->yimg, im->graph_col[GRC_BACK]);
    gfx_add_point(im, im->ximg, 0);
    gfx_close_path(im);
    gfx_new_area(im, im->xorigin, im->yorigin, im->xorigin + im->xsize, im->yorigin, im->xorigin + im->xsize, im->yorigin - im->ysize, im->graph_col[GRC_CANVAS]);

    gfx_add_point(im, im->xorigin, im->yorigin - im->ysize);
    gfx_close_path(im);
    gfx->rectangle(gfx_ctx, im->xorigin, im->yorigin - im->ysize - 1.0, im->xsize, im->ysize + 2.0);
    gfx->clip(gfx_ctx);
    return 0;
}
#endif

#if 0
int graph_gfx->finish(image_desc_t *im)
{

    switch (im->imgformat) {
    case IF_PNG:
    {
        gfx->status_t status;

        status = gfx->surface_write_to_png_stream(im->surface, &gfx->output, im);

        if (status != CAIRO_STATUS_SUCCESS) {
            rrd_set_error("Could not save png to '%s'", "memory");
            return 1;
        }
        break;
    }
    case IF_XML:
    case IF_XMLENUM:
    case IF_CSV:
    case IF_TSV:
    case IF_SSV:
    case IF_JSON:
    case IF_JSONTIME:
        break;
    default:
        gfx->surface_finish(im->surface);
        break;
    }

    return 0;
}
#endif

int gdes_alloc(image_desc_t *im)
{

    im->gdes_c++;
    im->gdes = realloc(im->gdes, (im->gdes_c) * sizeof(graph_desc_t));
    if (im->gdes == NULL) {
        fprintf(stderr, "realloc graph_descs");
        return -1;
    }

    /* set to zero */
    memset(&(im->gdes[im->gdes_c - 1]), 0, sizeof(graph_desc_t));

    im->gdes[im->gdes_c - 1].step = im->step;
    im->gdes[im->gdes_c - 1].step_orig = im->step;
    im->gdes[im->gdes_c - 1].stack = 0;
    im->gdes[im->gdes_c - 1].skipscale = 0;
    im->gdes[im->gdes_c - 1].linewidth = 0;
    im->gdes[im->gdes_c - 1].debug = 0;
    im->gdes[im->gdes_c - 1].start = im->start;
    im->gdes[im->gdes_c - 1].start_orig = im->start;
    im->gdes[im->gdes_c - 1].end = im->end;
    im->gdes[im->gdes_c - 1].end_orig = im->end;
    im->gdes[im->gdes_c - 1].data = NULL;
    im->gdes[im->gdes_c - 1].ds_namv = NULL;
    im->gdes[im->gdes_c - 1].data_first = 0;
    im->gdes[im->gdes_c - 1].p_data = NULL;
    im->gdes[im->gdes_c - 1].p_dashes = NULL;
    im->gdes[im->gdes_c - 1].shift = 0.0;
    im->gdes[im->gdes_c - 1].dash = 0;
    im->gdes[im->gdes_c - 1].ndash = 0;
    im->gdes[im->gdes_c - 1].offset = 0;
    im->gdes[im->gdes_c - 1].col.red = 0.0;
    im->gdes[im->gdes_c - 1].col.green = 0.0;
    im->gdes[im->gdes_c - 1].col.blue = 0.0;
    im->gdes[im->gdes_c - 1].col.alpha = 0.0;
    im->gdes[im->gdes_c - 1].col2.red = NAN;
    im->gdes[im->gdes_c - 1].col2.green = NAN;
    im->gdes[im->gdes_c - 1].col2.blue = NAN;
    im->gdes[im->gdes_c - 1].col2.alpha = 0.0;
    im->gdes[im->gdes_c - 1].gradheight = 50.0;
    im->gdes[im->gdes_c - 1].legend[0] = '\0';
    im->gdes[im->gdes_c - 1].format[0] = '\0';
    im->gdes[im->gdes_c - 1].strftm = 0;
    im->gdes[im->gdes_c - 1].vformatter = VALUE_FORMATTER_NUMERIC;
    im->gdes[im->gdes_c - 1].rrd[0] = '\0';
    im->gdes[im->gdes_c - 1].ds = -1;
    im->gdes[im->gdes_c - 1].cf_reduce = CF_AVERAGE;
    im->gdes[im->gdes_c - 1].cf_reduce_set = 0;
    im->gdes[im->gdes_c - 1].cf = CF_AVERAGE;
    im->gdes[im->gdes_c - 1].yrule = NAN;
    im->gdes[im->gdes_c - 1].xrule = 0;
    im->gdes[im->gdes_c - 1].daemon[0] = 0;

    return 0;
}

#if 0
/* copies input until the first unescaped colon is found
   or until input ends. backslashes have to be escaped as well */
static int scan_for_col(const char *const input, int len, char *const output)
{
    int       inp, outp = 0;

    for (inp = 0; inp < len && input[inp] != ':' && input[inp] != '\0'; inp++) {
        if (input[inp] == '\\'
            && input[inp + 1] != '\0'
            && (input[inp + 1] == '\\' || input[inp + 1] == ':')) {
            output[outp++] = input[++inp];
        } else {
            output[outp++] = input[inp];
        }
    }
    output[outp] = '\0';
    return inp;
}
#endif

gfx_color_t gfx_hex_to_col(long unsigned int color)
{
    gfx_color_t gfx_color;

    gfx_color.red = 1.0 / 255.0 * ((color & 0xff000000) >> (3 * 8));
    gfx_color.green = 1.0 / 255.0 * ((color & 0x00ff0000) >> (2 * 8));
    gfx_color.blue = 1.0 / 255.0 * ((color & 0x0000ff00) >> (1 * 8));
    gfx_color.alpha = 1.0 / 255.0 * (color & 0x000000ff);
    return gfx_color;
}

int rrd_graph_color(image_desc_t *im, char *var, char *err, int optional)
{
    char *color;
    graph_desc_t *gdp = &im->gdes[im->gdes_c - 1];

    color = strstr(var, "#");
    if (color == NULL) {
        if (optional == 0) {
            fprintf(stderr, "Found no color in %s", err);
            return 0;
        }
        return 0;
    } else {
        int n = 0;
        long unsigned int col = 0;

        char *rest = strstr(color, ":");
        if (rest != NULL)
            n = rest - color;
        else
            n = strlen(color);
        switch (n) {
        case 7:
            sscanf(color, "#%6lx%n", &col, &n);
            col = (col << 8) + 0xff /* shift left by 8 */ ;
            if (n != 7)
                fprintf(stderr, "Color problem in %s", err);
            break;
        case 9:
            sscanf(color, "#%8lx%n", &col, &n);
            if (n == 9)
                break;
            /* FALLTHROUGH */
        default:
            fprintf(stderr, "Color problem in %s", err);
        }
#if 0
        if (rrd_test_error())
            return 0;
#endif
        gdp->col = gfx_hex_to_col(col);
        return n;
    }
}

#define OVECCOUNT 30    /* should be a multiple of 3 */




void graph_free(image_desc_t *im)
{
    if (im == NULL)
        return;

    for (int i = 0; i < im->gdes_c; i++) {
        if (im->gdes[i].data_first) {
            /* careful here, because a single pointer can occur several times */
            free(im->gdes[i].data);
            if (im->gdes[i].ds_namv) {
                for (long unsigned int ii = 0; ii < im->gdes[i].ds_cnt; ii++)
                    free(im->gdes[i].ds_namv[ii]);
                free(im->gdes[i].ds_namv);
            }
        }
        /* free allocated memory used for dashed lines */
        if (im->gdes[i].p_dashes != NULL)
            free(im->gdes[i].p_dashes);

        free(im->gdes[i].p_data);
//        free(im->gdes[i].rpnp);
    }
    free(im->gdes);

    if (im->ylegend)
        free(im->ylegend);
    if (im->title)
        free(im->title);
    if (im->watermark)
        free(im->watermark);
    if (im->xlab_form)
        free(im->xlab_form);
    if (im->second_axis_legend)
        free(im->second_axis_legend);
    if (im->second_axis_format)
        free(im->second_axis_format);
    if (im->primary_axis_format)
        free(im->primary_axis_format);
}

void graph_init(image_desc_t *im)
{
//    char *deffont = getenv("DEFAULT_FONT");
//    PangoContext *context;

    /* zero the whole structure first */
    memset(im, 0, sizeof(image_desc_t));

#ifdef HAVE_TZSET
    tzset();
#endif
#if 0
    im->gdef_map = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    //use of g_free() cause heap damage on windows. Key is allocated by malloc() in sprintf_alloc(), so free() must use
    im->rrd_map = g_hash_table_new_full(g_str_hash, g_str_equal, free, NULL);
#endif

    im->graph_type = GTYPE_TIME;
    im->base = 1000;
    im->daemon_addr = NULL;
    im->draw_x_grid = 1;
    im->draw_y_grid = 1;
    im->draw_3d_border = 2;
    im->dynamic_labels = 0;
    im->extra_flags = 0;
    im->forceleftspace = 0;
    im->gdes_c = 0;
    im->gdes = NULL;
#if 0
    im->graph_antialias = CAIRO_ANTIALIAS_GRAY;
#endif
    im->grid_dash[0] = 1;
    im->grid_dash[1] = 1;
    im->gridfit = 1;
    im->imgformat = IF_PNG;
    im->imginfo = NULL;
    im->lazy = 0;
    im->legenddirection = TOP_DOWN;
    im->legendheight = 0;
    im->legendposition = SOUTH;
    im->legendwidth = 0;
    im->logarithmic = 0;
    im->maxval = NAN;
    im->minval = 0;
    im->minval = NAN;
    im->magfact = 1;
    im->prt_c = 0;
    im->rigid = 0;
    im->allow_shrink = 0;
    im->rendered_image_size = 0;
    im->rendered_image = NULL;
    im->slopemode = 0;
    im->step = 0;
    im->symbol = ' ';
    im->tabwidth = 40.0;
    im->title = NULL;
    im->unitsexponent = 9999;
    im->unitslength = 6;
    im->viewfactor = 1.0;
    im->watermark = NULL;
    im->xlab_form = NULL;
    im->with_markup = 0;
    im->ximg = 0;
    im->xlab_user.minsec = -1.0;
    im->xorigin = 0;
    im->xOriginLegend = 0;
    im->xOriginLegendY = 0;
    im->xOriginLegendY2 = 0;
    im->xOriginTitle = 0;
    im->xsize = 400;
    im->ygridstep = NAN;
    im->yimg = 0;
    im->ylegend = NULL;
    im->second_axis_scale = 0;  /* 0 disables it */
    im->second_axis_shift = 0;  /* no shift by default */
    im->second_axis_legend = NULL;
    im->second_axis_format = NULL;
    im->second_axis_formatter = VALUE_FORMATTER_NUMERIC;
    im->primary_axis_format = NULL;
    im->primary_axis_formatter = VALUE_FORMATTER_NUMERIC;
    im->yorigin = 0;
    im->yOriginLegend = 0;
    im->yOriginLegendY = 0;
    im->yOriginLegendY2 = 0;
    im->yOriginTitle = 0;
    im->ysize = 100;
    im->zoom = 1;
    im->last_tabwidth = -1;


    memcpy(im->text_prop, text_prop, TEXT_PROP_MAX*sizeof(text_prop[0]));

#if 0
    if (init_mode == IMAGE_INIT_CAIRO) {
        im->font_options = gfx->font_options_create();
        im->surface = gfx->image_surface_create(CAIRO_FORMAT_ARGB32, 10, 10);
        gfx_ctx = gfx->create(im->surface);

        for (i = 0; i < DIM(text_prop); i++) {
            im->text_prop[i].size = -1;
            im->text_prop[i].font = NULL;
            rrd_set_font_desc(im, i, deffont ? deffont : text_prop[i].font,
                              text_prop[i].size);
        }
        PangoFontMap *fontmap = pango_gfx->font_map_get_default();
#ifdef HAVE_PANGO_FONT_MAP_CREATE_CONTEXT
        context = pango_font_map_create_context((PangoFontMap *) fontmap);
#else
        context = pango_gfx->font_map_create_context((PangoCairoFontMap *) fontmap);
#endif
        pango_gfx->context_set_resolution(context, 100);
        pango_gfx->update_context(gfx_ctx, context);
        im->layout = pango_layout_new(context);
        g_object_unref(context);
//  im->layout = pango_gfx->create_layout(gfx_ctx);
        gfx->font_options_set_hint_style (im->font_options, CAIRO_HINT_STYLE_FULL);
        gfx->font_options_set_hint_metrics (im->font_options, CAIRO_HINT_METRICS_ON);
        gfx->font_options_set_antialias(im->font_options, CAIRO_ANTIALIAS_GRAY);
    }
#endif

    for (long unsigned int i = 0; i < DIM(graph_col); i++)
        im->graph_col[i] = graph_col[i];
}

