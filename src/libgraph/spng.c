/* SPDX-License-Identifier: BSD-2-Clause AND libpng-2.0 */

#include "spng.h"

#include <limits.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#pragma GCC diagnostic ignored "-Wcast-qual"

#define ZLIB_CONST
#include <zlib.h>

/* Not build options, edit at your own risk! */
#define SPNG_READ_SIZE (8192)
#define SPNG_WRITE_SIZE SPNG_READ_SIZE
#define SPNG_MAX_CHUNK_COUNT (1000)

enum spng_state {
    SPNG_STATE_INVALID = 0,
    SPNG_STATE_INIT = 1,                  /* No PNG buffer/stream is set */
    SPNG_STATE_INPUT,                     /* Decoder input PNG was set */
    SPNG_STATE_OUTPUT = SPNG_STATE_INPUT, /* Encoder output was set */
    SPNG_STATE_IHDR,                      /* IHDR was read/written */
    SPNG_STATE_FIRST_IDAT,                /* Encoded up to / reached first IDAT */
    SPNG_STATE_DECODE_INIT,               /* Decoder is ready for progressive reads */
    SPNG_STATE_ENCODE_INIT = SPNG_STATE_DECODE_INIT,
    SPNG_STATE_EOI,                       /* Reached the last scanline/row */
    SPNG_STATE_LAST_IDAT,                 /* Reached last IDAT, set at end of decode_image() */
    SPNG_STATE_AFTER_IDAT,                /*  */
    SPNG_STATE_IEND,                      /* Reached IEND */
};

enum spng__internal {
    SPNG__IO_SIGNAL = 1 << 9,
    SPNG__CTX_FLAGS_ALL = (SPNG_CTX_IGNORE_ADLER32 | SPNG_CTX_ENCODER)
};

#define SPNG_STR(x) _SPNG_STR(x)
#define _SPNG_STR(x) #x

#define SPNG_VERSION_STRING SPNG_STR(SPNG_VERSION_MAJOR) "." \
                            SPNG_STR(SPNG_VERSION_MINOR) "." \
                            SPNG_STR(SPNG_VERSION_PATCH)

/* Determine if the spng_option can be overridden/optimized */
#define spng__optimize(option) (ctx->optimize_option & (1 << option))

struct spng_subimage {
    uint32_t width;
    uint32_t height;
    size_t out_width; /* byte width based on output format */
    size_t scanline_width;
};

struct spng_text2 {
    int type;
    char *keyword;
    char *text;

    size_t text_length;

    uint8_t compression_flag; /* iTXt only */
    char *language_tag; /* iTXt only */
    char *translated_keyword; /* iTXt only */

    size_t cache_usage;
    char user_keyword_storage[80];
};

struct decode_flags {
    unsigned apply_trns:  1;
    unsigned apply_gamma: 1;
    unsigned use_sbit:    1;
    unsigned indexed:     1;
    unsigned do_scaling:  1;
    unsigned interlaced:  1;
    unsigned same_layout: 1;
    unsigned zerocopy:    1;
    unsigned unpack:      1;
};

struct encode_flags {
    unsigned interlace:      1;
    unsigned same_layout:    1;
    unsigned to_bigendian:   1;
    unsigned progressive:    1;
    unsigned finalize:       1;

    enum spng_filter_choice filter_choice;
};

struct spng_chunk_bitfield {
    unsigned ihdr: 1;
    unsigned plte: 1;
    unsigned chrm: 1;
    unsigned iccp: 1;
    unsigned gama: 1;
    unsigned sbit: 1;
    unsigned srgb: 1;
    unsigned text: 1;
    unsigned bkgd: 1;
    unsigned hist: 1;
    unsigned trns: 1;
    unsigned phys: 1;
    unsigned splt: 1;
    unsigned time: 1;
    unsigned offs: 1;
    unsigned exif: 1;
    unsigned unknown: 1;
};

/* Packed sample iterator */
struct spng__iter {
    const uint8_t mask;
    unsigned shift_amount;
    const unsigned initial_shift, bit_depth;
    const unsigned char *samples;
};

union spng__decode_plte {
    struct spng_plte_entry rgba[256];
    unsigned char rgb[256 * 3];
    unsigned char raw[256 * 4];
    uint32_t align_this;
};

struct spng__zlib_options {
    int compression_level;
    int window_bits;
    int mem_level;
    int strategy;
    int data_type;
};

typedef void spng__undo(spng_ctx *ctx);

struct spng_ctx {
    size_t data_size;
    size_t bytes_read;
    size_t stream_buf_size;
    unsigned char *stream_buf;
    const unsigned char *data;

    /* User-defined pointers for streaming */
    spng_write_fn *write_fn;
    void *stream_user_ptr;

    /* Used for buffer reads */
    const unsigned char *png_base;
    size_t bytes_left;
    size_t last_read_size;

    /* Used for encoding */
    int user_owns_out_png;
    unsigned char *out_png;
    unsigned char *write_ptr;
    size_t out_png_size;
    size_t bytes_encoded;

    /* These are updated by read/write_header()/read_chunk_bytes() */
    struct spng_chunk current_chunk;
    uint32_t cur_chunk_bytes_left;
    uint32_t cur_actual_crc;

    struct spng_alloc alloc;

    enum spng_ctx_flags flags;
    enum spng_format fmt;

    enum spng_state state;

    unsigned streaming: 1;
    unsigned internal_buffer: 1; /* encoding to internal buffer */

    unsigned inflate: 1;
    unsigned deflate: 1;
    unsigned strict: 1;
    unsigned discard: 1;
    unsigned skip_crc: 1;
    unsigned keep_unknown: 1;
    unsigned prev_was_idat: 1;

    struct spng__zlib_options image_options;
    struct spng__zlib_options text_options;

    spng__undo *undo;

    /* input file contains this chunk */
    struct spng_chunk_bitfield file;

    /* chunk was stored with spng_set_*() */
    struct spng_chunk_bitfield user;

    /* chunk was stored by reading or with spng_set_*() */
    struct spng_chunk_bitfield stored;

    /* used to reset the above in case of an error */
    struct spng_chunk_bitfield prev_stored;

    struct spng_chunk first_idat, last_idat;

    uint32_t max_width, max_height;

    size_t max_chunk_size;
    size_t chunk_cache_limit;
    size_t chunk_cache_usage;
    uint32_t chunk_count_limit;
    uint32_t chunk_count_total;

    int crc_action_critical;
    int crc_action_ancillary;

    uint32_t optimize_option;

    struct spng_ihdr ihdr;

    struct spng_plte plte;

    struct spng_chrm_int chrm_int;
    struct spng_iccp iccp;

    uint32_t gama;

    struct spng_sbit sbit;

    uint8_t srgb_rendering_intent;

    uint32_t n_text;
    struct spng_text2 *text_list;

    struct spng_bkgd bkgd;
    struct spng_hist hist;
    struct spng_trns trns;
    struct spng_phys phys;

    uint32_t n_splt;
    struct spng_splt *splt_list;

    struct spng_time time;
    struct spng_offs offs;
    struct spng_exif exif;

    uint32_t n_chunks;
    struct spng_unknown_chunk *chunk_list;

    struct spng_subimage subimage[7];
#if 0
    z_stream zstream;
#endif
    unsigned char *scanline_buf, *prev_scanline_buf, *row_buf, *filtered_scanline_buf;
    unsigned char *scanline, *prev_scanline, *row, *filtered_scanline;

    /* based on fmt */
    size_t image_size; /* may be zero */
    size_t image_width;

    unsigned bytes_per_pixel; /* derived from ihdr */
    unsigned pixel_size; /* derived from spng_format+ihdr */
    int widest_pass;
    int last_pass; /* last non-empty pass */

    uint16_t *gamma_lut; /* points to either _lut8 or _lut16 */
    uint16_t *gamma_lut16;
    uint16_t gamma_lut8[256];
    unsigned char trns_px[8];
    union spng__decode_plte decode_plte;
    struct spng_sbit decode_sb;
    struct decode_flags decode_flags;
    struct spng_row_info row_info;

    struct encode_flags encode_flags;
};

static const uint32_t spng_u32max = INT32_MAX;

static const uint32_t adam7_x_start[7] = { 0, 4, 0, 2, 0, 1, 0 };
static const uint32_t adam7_y_start[7] = { 0, 0, 4, 0, 2, 0, 1 };
static const uint32_t adam7_x_delta[7] = { 8, 8, 4, 4, 2, 2, 1 };
static const uint32_t adam7_y_delta[7] = { 8, 8, 8, 4, 4, 2, 2 };

static const uint8_t spng_signature[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };

static const uint8_t type_ihdr[4] = { 73, 72, 68, 82 };
static const uint8_t type_plte[4] = { 80, 76, 84, 69 };
#if 0
static const uint8_t type_idat[4] = { 73, 68, 65, 84 };
#endif

static const uint8_t type_trns[4] = { 116, 82, 78, 83 };
static const uint8_t type_chrm[4] = { 99,  72, 82, 77 };
static const uint8_t type_gama[4] = { 103, 65, 77, 65 };
static const uint8_t type_iccp[4] = { 105, 67, 67, 80 };
static const uint8_t type_sbit[4] = { 115, 66, 73, 84 };
static const uint8_t type_srgb[4] = { 115, 82, 71, 66 };
static const uint8_t type_text[4] = { 116, 69, 88, 116 };
static const uint8_t type_ztxt[4] = { 122, 84, 88, 116 };
static const uint8_t type_itxt[4] = { 105, 84, 88, 116 };
static const uint8_t type_bkgd[4] = { 98,  75, 71, 68 };
static const uint8_t type_hist[4] = { 104, 73, 83, 84 };
static const uint8_t type_phys[4] = { 112, 72, 89, 115 };
static const uint8_t type_splt[4] = { 115, 80, 76, 84 };
static const uint8_t type_time[4] = { 116, 73, 77, 69 };

static const uint8_t type_offs[4] = { 111, 70, 70, 115 };
static const uint8_t type_exif[4] = { 101, 88, 73, 102 };

#if 0
static const uint32_t crc_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
    0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
    0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
    0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
    0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
    0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
    0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
    0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
    0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
    0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
    0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
    0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
    0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
    0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
    0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
    0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
    0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
    0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
    0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
    0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

static uint32_t crc32(uint32_t crc, const uint8_t *buf, size_t len)
{
    crc = ~crc;
    for (size_t n = 0; n < len; n++) {
        crc = crc_table[(crc ^ buf[n]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}
#endif

static inline void *spng__malloc(spng_ctx *ctx,  size_t size)
{
    return ctx->alloc.malloc_fn(size);
}

static inline void *spng__calloc(spng_ctx *ctx, size_t nmemb, size_t size)
{
    return ctx->alloc.calloc_fn(nmemb, size);
}

static inline void *spng__realloc(spng_ctx *ctx, void *ptr, size_t size)
{
    return ctx->alloc.realloc_fn(ptr, size);
}

static inline void spng__free(spng_ctx *ctx, void *ptr)
{
    ctx->alloc.free_fn(ptr);
}
#if 0
static void *spng__zalloc(void *opaque, uInt items, uInt size)
{
    spng_ctx *ctx = opaque;

    if (size > SIZE_MAX / items)
        return NULL;

    size_t len = (size_t)items * size;

    return spng__malloc(ctx, len);
}
#endif
#if 0
static void spng__zfree(void *opqaue, void *ptr)
{
    spng_ctx *ctx = opqaue;
    spng__free(ctx, ptr);
}
#endif

#if 0
static inline uint16_t read_u16(const void *src)
{
    const unsigned char *data = src;

    return (data[0] & 0xFFU) << 8 | (data[1] & 0xFFU);
}

static inline uint32_t read_u32(const void *src)
{
    const unsigned char *data = src;

    return (data[0] & 0xFFUL) << 24 | (data[1] & 0xFFUL) << 16 |
           (data[2] & 0xFFUL) << 8  | (data[3] & 0xFFUL);
}

static inline int32_t read_s32(const void *src)
{
    int32_t ret = (int32_t)read_u32(src);

    return ret;
}
#endif
static inline void write_u16(void *dest, uint16_t x)
{
    unsigned char *data = dest;

    data[0] = x >> 8;
    data[1] = x & 0xFF;
}

static inline void write_u32(void *dest, uint32_t x)
{
    unsigned char *data = dest;

    data[0] = (x >> 24);
    data[1] = (x >> 16) & 0xFF;
    data[2] = (x >> 8) & 0xFF;
    data[3] = x & 0xFF;
}

static inline void write_s32(void *dest, int32_t x)
{
    uint32_t n = x;
    write_u32(dest, n);
}

#if 0
/* Returns an iterator for 1,2,4,8-bit samples */
static struct spng__iter spng__iter_init(unsigned bit_depth, const unsigned char *samples)
{
    struct spng__iter iter = {
        .mask = (uint32_t)(1 << bit_depth) - 1,
        .shift_amount = 8 - bit_depth,
        .initial_shift = 8 - bit_depth,
        .bit_depth = bit_depth,
        .samples = samples
    };

    return iter;
}

/* Returns the current sample unpacked, iterates to the next one */
static inline uint8_t get_sample(struct spng__iter *iter)
{
    uint8_t x = (iter->samples[0] >> iter->shift_amount) & iter->mask;

    iter->shift_amount -= iter->bit_depth;

    if (iter->shift_amount > 7) {
        iter->shift_amount = iter->initial_shift;
        iter->samples++;
    }

    return x;
}
#endif

static void u16_row_to_bigendian(void *row, size_t size)
{
    uint16_t *px = (uint16_t*)row;
    size_t i, n = size / 2;

    for (i=0; i < n; i++) {
        write_u16(&px[i], px[i]);
    }
}

static unsigned num_channels(const struct spng_ihdr *ihdr)
{
    switch(ihdr->color_type) {
        case SPNG_COLOR_TYPE_TRUECOLOR: return 3;
        case SPNG_COLOR_TYPE_GRAYSCALE_ALPHA: return 2;
        case SPNG_COLOR_TYPE_TRUECOLOR_ALPHA: return 4;
        case SPNG_COLOR_TYPE_GRAYSCALE:
        case SPNG_COLOR_TYPE_INDEXED:
            return 1;
        default: return 0;
    }
}

/* Calculate scanline width in bits, round up to the nearest byte */
static int calculate_scanline_width(const struct spng_ihdr *ihdr, uint32_t width,
                                                                  size_t *scanline_width)
{
    if (ihdr == NULL || !width)
        return SPNG_EINTERNAL;

    size_t res = num_channels(ihdr) * ihdr->bit_depth;

    if (res > SIZE_MAX / width)
        return SPNG_EOVERFLOW;
    res = res * width;

    res += 15; /* Filter byte + 7 for rounding */

    if (res < 15)
        return SPNG_EOVERFLOW;

    res /= 8;

    if (res > UINT32_MAX)
        return SPNG_EOVERFLOW;

    *scanline_width = res;

    return 0;
}

static int calculate_subimages(struct spng_ctx *ctx)
{
    if (ctx == NULL)
        return SPNG_EINTERNAL;

    struct spng_ihdr *ihdr = &ctx->ihdr;
    struct spng_subimage *sub = ctx->subimage;

    if (ihdr->interlace_method == 1) {
        sub[0].width = (ihdr->width + 7) >> 3;
        sub[0].height = (ihdr->height + 7) >> 3;
        sub[1].width = (ihdr->width + 3) >> 3;
        sub[1].height = (ihdr->height + 7) >> 3;
        sub[2].width = (ihdr->width + 3) >> 2;
        sub[2].height = (ihdr->height + 3) >> 3;
        sub[3].width = (ihdr->width + 1) >> 2;
        sub[3].height = (ihdr->height + 3) >> 2;
        sub[4].width = (ihdr->width + 1) >> 1;
        sub[4].height = (ihdr->height + 1) >> 2;
        sub[5].width = ihdr->width >> 1;
        sub[5].height = (ihdr->height + 1) >> 1;
        sub[6].width = ihdr->width;
        sub[6].height = ihdr->height >> 1;
    } else {
        sub[0].width = ihdr->width;
        sub[0].height = ihdr->height;
    }

    int i;
    for (i=0; i < 7; i++) {
        if (sub[i].width == 0 || sub[i].height == 0)
            continue;

        int ret = calculate_scanline_width(ihdr, sub[i].width, &sub[i].scanline_width);
        if (ret)
            return ret;

        if (sub[ctx->widest_pass].scanline_width < sub[i].scanline_width)
            ctx->widest_pass = i;

        ctx->last_pass = i;
    }

    return 0;
}

static int calculate_image_width(const struct spng_ihdr *ihdr, int fmt, size_t *len)
{
    if (ihdr == NULL || len == NULL)
        return SPNG_EINTERNAL;

    size_t res = ihdr->width;
    unsigned bytes_per_pixel;

    switch(fmt) {
    case SPNG_FMT_RGBA8:
    case SPNG_FMT_GA16:
        bytes_per_pixel = 4;
        break;
    case SPNG_FMT_RGBA16:
        bytes_per_pixel = 8;
        break;
    case SPNG_FMT_RGB8:
        bytes_per_pixel = 3;
        break;
    case SPNG_FMT_PNG:
    case SPNG_FMT_RAW: {
        int ret = calculate_scanline_width(ihdr, ihdr->width, &res);
        if (ret)
            return ret;
        res -= 1; /* exclude filter byte */
        bytes_per_pixel = 1;
    }   break;
    case SPNG_FMT_G8:
        bytes_per_pixel = 1;
        break;
    case SPNG_FMT_GA8:
        bytes_per_pixel = 2;
        break;
    default:
        return SPNG_EINTERNAL;
    }

    if (res > SIZE_MAX / bytes_per_pixel)
        return SPNG_EOVERFLOW;
    res = res * bytes_per_pixel;

    *len = res;

    return 0;
}

static int encode_err(spng_ctx *ctx, int err)
{
    ctx->state = SPNG_STATE_INVALID;

    return err;
}

/* Ensure there is enough space for encoding starting at ctx->write_ptr  */
static int require_bytes(spng_ctx *ctx, size_t bytes)
{
    if (ctx == NULL)
        return SPNG_EINTERNAL;

    if (ctx->streaming) {
        if (bytes > ctx->stream_buf_size) {
            size_t new_size = ctx->stream_buf_size;

            /* Start at default IDAT size + header + crc */
            if (new_size < (SPNG_WRITE_SIZE + 12))
                new_size = SPNG_WRITE_SIZE + 12;

            if (new_size < bytes)
                new_size = bytes;

            void *temp = spng__realloc(ctx, ctx->stream_buf, new_size);

            if (temp == NULL)
                return encode_err(ctx, SPNG_EMEM);

            ctx->stream_buf = temp;
            ctx->stream_buf_size = bytes;
            ctx->write_ptr = ctx->stream_buf;
        }

        return 0;
    }

    if (!ctx->internal_buffer)
        return SPNG_ENODST;

    size_t required = ctx->bytes_encoded + bytes;
    if (required < bytes)
        return SPNG_EOVERFLOW;

    if (required > ctx->out_png_size) {
        size_t new_size = ctx->out_png_size;

        /* Start with a size that doesn't require a realloc() 100% of the time */
        if (new_size < (SPNG_WRITE_SIZE * 2))
            new_size = SPNG_WRITE_SIZE * 2;

        /* Prefer the next power of two over the requested size */
        while (new_size < required) {
            if (new_size / SIZE_MAX > 2)
                return encode_err(ctx, SPNG_EOVERFLOW);

            new_size *= 2;
        }

        void *temp = spng__realloc(ctx, ctx->out_png, new_size);

        if (temp == NULL)
            return encode_err(ctx, SPNG_EMEM);

        ctx->out_png = temp;
        ctx->out_png_size = new_size;
        ctx->write_ptr = ctx->out_png + ctx->bytes_encoded;
    }

    return 0;
}

static int write_data(spng_ctx *ctx, const void *data, size_t bytes)
{
    if (ctx == NULL)
        return SPNG_EINTERNAL;
    if (!bytes)
        return 0;

    if (ctx->streaming) {
        if (bytes > SPNG_WRITE_SIZE)
            return SPNG_EINTERNAL;

        int ret = ctx->write_fn(ctx, ctx->stream_user_ptr, (void*)data, bytes);

        if (ret) {
            if (ret > 0 || ret < SPNG_IO_ERROR)
                ret = SPNG_IO_ERROR;

            return encode_err(ctx, ret);
        }
    } else {
        int ret = require_bytes(ctx, bytes);
        if (ret)
            return encode_err(ctx, ret);

        memcpy(ctx->write_ptr, data, bytes);

        ctx->write_ptr += bytes;
    }

    ctx->bytes_encoded += bytes;
    if (ctx->bytes_encoded < bytes)
        return SPNG_EOVERFLOW;

    return 0;
}

static int write_header(spng_ctx *ctx, const uint8_t chunk_type[4], size_t chunk_length,
                                       unsigned char **data)
{
    if (ctx == NULL || chunk_type == NULL)
        return SPNG_EINTERNAL;
    if (chunk_length > spng_u32max)
        return SPNG_EINTERNAL;

    size_t total = chunk_length + 12;

    int ret = require_bytes(ctx, total);
    if (ret)
        return ret;

    uint32_t crc = crc32(0, NULL, 0);
    ctx->current_chunk.crc = crc32(crc, chunk_type, 4);

    memcpy(&ctx->current_chunk.type, chunk_type, 4);
    ctx->current_chunk.length = (uint32_t)chunk_length;

    if (!data)
        return SPNG_EINTERNAL;

    if (ctx->streaming)
        *data = ctx->stream_buf + 8;
    else
        *data = ctx->write_ptr + 8;

    return 0;
}
#if 0
static int trim_chunk(spng_ctx *ctx, uint32_t length)
{
    if (length > spng_u32max)
        return SPNG_EINTERNAL;
    if (length > ctx->current_chunk.length)
        return SPNG_EINTERNAL;

    ctx->current_chunk.length = length;

    return 0;
}
#endif
static int finish_chunk(spng_ctx *ctx)
{
    if (ctx == NULL)
        return SPNG_EINTERNAL;

    struct spng_chunk *chunk = &ctx->current_chunk;

    unsigned char *header;
    unsigned char *chunk_data;

    if (ctx->streaming) {
        chunk_data = ctx->stream_buf + 8;
        header = ctx->stream_buf;
    } else {
        chunk_data = ctx->write_ptr + 8;
        header = ctx->write_ptr;
    }

    write_u32(header, chunk->length);
    memcpy(header + 4, chunk->type, 4);

    chunk->crc = crc32(chunk->crc, chunk_data, chunk->length);

    write_u32(chunk_data + chunk->length, chunk->crc);

    if (ctx->streaming) {
        const unsigned char *ptr = ctx->stream_buf;
        uint32_t bytes_left = chunk->length + 12;
        uint32_t len = 0;

        while (bytes_left) {
            ptr += len;
            len = SPNG_WRITE_SIZE;

            if (len > bytes_left)
                len = bytes_left;

            int ret = write_data(ctx, ptr, len);
            if (ret)
                return ret;

            bytes_left -= len;
        }
    } else {
        ctx->bytes_encoded += chunk->length;
        if (ctx->bytes_encoded < chunk->length)
            return SPNG_EOVERFLOW;

        ctx->bytes_encoded += 12;
        if (ctx->bytes_encoded < 12)
            return SPNG_EOVERFLOW;

        ctx->write_ptr += chunk->length + 12;
    }

    return 0;
}

static int write_chunk(spng_ctx *ctx, const uint8_t type[4], const void *data, size_t length)
{
    if (ctx == NULL || type == NULL)
        return SPNG_EINTERNAL;
    if (length && data == NULL)
        return SPNG_EINTERNAL;

    unsigned char *write_ptr;

    int ret = write_header(ctx, type, length, &write_ptr);
    if (ret)
        return ret;

    if (length)
        memcpy(write_ptr, data, length);

    return finish_chunk(ctx);
}

static int write_iend(spng_ctx *ctx)
{
    unsigned char iend_chunk[12] = { 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130 };
    return write_data(ctx, iend_chunk, 12);
}

static int write_unknown_chunks(spng_ctx *ctx, enum spng_location location)
{
    if (!ctx->stored.unknown)
        return 0;

    const struct spng_unknown_chunk *chunk = ctx->chunk_list;

    uint32_t i;
    for (i=0; i < ctx->n_chunks; i++, chunk++) {
        if (chunk->location != location)
            continue;

        int ret = write_chunk(ctx, chunk->type, chunk->data, chunk->length);
        if (ret)
            return ret;
    }

    return 0;
}

static int spng__deflate_init(spng_ctx *ctx, struct spng__zlib_options *options)
{
    (void)ctx;
    (void)options;
#if 0
    if (ctx->zstream.state)
        deflateEnd(&ctx->zstream);

    ctx->deflate = 1;
    z_stream *zstream = &ctx->zstream;
    zstream->zalloc = spng__zalloc;
    zstream->zfree = spng__zfree;
    zstream->opaque = ctx;
    zstream->data_type = options->data_type;

    int ret = deflateInit2(zstream, options->compression_level, Z_DEFLATED, options->window_bits,
                                    options->mem_level, options->strategy);
    if (ret != Z_OK)
        return SPNG_EZLIB_INIT;
#endif
    return 0;
}

static uint8_t paeth(uint8_t a, uint8_t b, uint8_t c)
{
    int16_t p = a + b - c;
    int16_t pa = abs(p - a);
    int16_t pb = abs(p - b);
    int16_t pc = abs(p - c);

    if (pa <= pb && pa <= pc)
        return a;
    else if (pb <= pc)
        return b;

    return c;
}

static int filter_scanline(unsigned char *filtered, const unsigned char *prev_scanline,
                           const unsigned char *scanline, size_t scanline_width,
                           unsigned bytes_per_pixel, const unsigned filter)
{
    if (prev_scanline == NULL || scanline == NULL || scanline_width <= 1)
        return SPNG_EINTERNAL;

    if (filter > 4)
        return SPNG_EFILTER;
    if (filter == 0)
        return 0;

    scanline_width--;

    uint32_t i;
    for (i=0; i < scanline_width; i++) {
        uint8_t x, a, b, c;

        if (i >= bytes_per_pixel) {
            a = scanline[i - bytes_per_pixel];
            b = prev_scanline[i];
            c = prev_scanline[i - bytes_per_pixel];
        } else { /* first pixel in row */
            a = 0;
            b = prev_scanline[i];
            c = 0;
        }

        x = scanline[i];

        switch(filter) {
        case SPNG_FILTER_SUB:
            x = x - a;
            break;
        case SPNG_FILTER_UP:
            x = x - b;
            break;
        case SPNG_FILTER_AVERAGE: {
            uint16_t avg = (a + b) / 2;
            x = x - avg;
        }   break;
        case SPNG_FILTER_PAETH:
            x = x - paeth(a,b,c);
            break;
        }

        filtered[i] = x;
    }

    return 0;
}

static int32_t filter_sum(const unsigned char *prev_scanline, const unsigned char *scanline,
                          size_t size, unsigned bytes_per_pixel, const unsigned filter)
{
    /* prevent potential over/underflow, bails out at a width of ~8M pixels for RGBA8 */
    if (size > (INT32_MAX / 128))
        return INT32_MAX;

    uint32_t i;
    int32_t sum = 0;
    uint8_t x, a, b, c;

    for (i=0; i < size; i++) {
        if (i >= bytes_per_pixel) {
            a = scanline[i - bytes_per_pixel];
            b = prev_scanline[i];
            c = prev_scanline[i - bytes_per_pixel];
        } else { /* first pixel in row */
            a = 0;
            b = prev_scanline[i];
            c = 0;
        }

        x = scanline[i];

        switch(filter) {
        case SPNG_FILTER_NONE:
            break;
        case SPNG_FILTER_SUB:
            x = x - a;
            break;
        case SPNG_FILTER_UP:
            x = x - b;
            break;
        case SPNG_FILTER_AVERAGE: {
            uint16_t avg = (a + b) / 2;
            x = x - avg;
        }   break;
        case SPNG_FILTER_PAETH:
            x = x - paeth(a,b,c);
            break;
        }

        sum += 128 - abs((int)x - 128);
    }

    return sum;
}

static unsigned get_best_filter(const unsigned char *prev_scanline, const unsigned char *scanline,
                                size_t scanline_width, unsigned bytes_per_pixel, const int choices)
{
    if (!choices)
        return SPNG_FILTER_NONE;

    scanline_width--;

    int i;
    unsigned int best_filter = 0;
    enum spng_filter_choice flag;
    int32_t sum, best_score = INT32_MAX;
    int32_t filter_scores[5] = { INT32_MAX, INT32_MAX, INT32_MAX, INT32_MAX, INT32_MAX };

    if (!(choices & (choices - 1))) {
        /* only one choice/bit is set */
        for (i=0; i < 5; i++) {
            if (choices == 1 << (i + 3)) return i;
        }
    }

    for (i=0; i < 5; i++) {
        flag = 1 << (i + 3);

        if (choices & flag)
            sum = filter_sum(prev_scanline, scanline, scanline_width, bytes_per_pixel, i);
        else
            continue;

        filter_scores[i] = abs(sum);

        if (filter_scores[i] < best_score) {
            best_score = filter_scores[i];
            best_filter = i;
        }
    }

    return best_filter;
}

#if 0
/* Scale "sbits" significant bits in "sample" from "bit_depth" to "target"

   "bit_depth" must be a valid PNG depth
   "sbits" must be less than or equal to "bit_depth"
   "target" must be between 1 and 16
*/
static uint16_t sample_to_target(uint16_t sample, unsigned bit_depth, unsigned sbits,
                                 unsigned target)
{
    if (bit_depth == sbits) {
        if (target == sbits)
            return sample; /* No scaling */
    } else { /* bit_depth > sbits */
        sample = sample >> (bit_depth - sbits); /* Shift significant bits to bottom */
    }

    /* Downscale */
    if (target < sbits)
        return sample >> (sbits - target);

    /* Upscale using left bit replication */
    int8_t shift_amount = target - sbits;
    uint16_t sample_bits = sample;
    sample = 0;

    while (shift_amount >= 0) {
        sample = sample | (sample_bits << shift_amount);
        shift_amount -= sbits;
    }

    int8_t partial = shift_amount + (int8_t)sbits;

    if (partial != 0)
        sample = sample | (sample_bits >> abs(shift_amount));

    return sample;
}

static inline void gamma_correct_row(unsigned char *row, uint32_t pixels, int fmt,
                                     const uint16_t *gamma_lut)
{
    uint32_t i;

    if (fmt == SPNG_FMT_RGBA8) {
        unsigned char *px;
        for (i=0; i < pixels; i++) {
            px = row + i * 4;

            px[0] = gamma_lut[px[0]];
            px[1] = gamma_lut[px[1]];
            px[2] = gamma_lut[px[2]];
        }
    } else if (fmt == SPNG_FMT_RGBA16) {
        for (i=0; i < pixels; i++) {
            uint16_t px[4];
            memcpy(px, row + i * 8, 8);

            px[0] = gamma_lut[px[0]];
            px[1] = gamma_lut[px[1]];
            px[2] = gamma_lut[px[2]];

            memcpy(row + i * 8, px, 8);
        }
    } else if (fmt == SPNG_FMT_RGB8) {
        unsigned char *px;
        for (i=0; i < pixels; i++) {
            px = row + i * 3;

            px[0] = gamma_lut[px[0]];
            px[1] = gamma_lut[px[1]];
            px[2] = gamma_lut[px[2]];
        }
    }
}

/* Apply transparency to output row */
static inline void trns_row(unsigned char *row, const unsigned char *scanline,
                            const unsigned char *trns, unsigned scanline_stride,
                            struct spng_ihdr *ihdr, uint32_t pixels, int fmt)
{
    uint32_t i;
    unsigned row_stride;
    unsigned depth = ihdr->bit_depth;

    if (fmt == SPNG_FMT_RGBA8) {
        if (ihdr->color_type == SPNG_COLOR_TYPE_GRAYSCALE)
            return; /* already applied in the decoding loop */

        row_stride = 4;
        for (i=0; i < pixels; i++, scanline+=scanline_stride, row+=row_stride) {
            if (!memcmp(scanline, trns, scanline_stride))
                row[3] = 0;
        }
    } else if (fmt == SPNG_FMT_RGBA16) {
        if (ihdr->color_type == SPNG_COLOR_TYPE_GRAYSCALE)
            return; /* already applied in the decoding loop */

        row_stride = 8;
        for (i=0; i < pixels; i++, scanline+=scanline_stride, row+=row_stride) {
            if (!memcmp(scanline, trns, scanline_stride))
                memset(row + 6, 0, 2);
        }
    } else if (fmt == SPNG_FMT_GA8) {
        row_stride = 2;

        if (depth == 16) {
            for (i=0; i < pixels; i++, scanline+=scanline_stride, row+=row_stride) {
                if (!memcmp(scanline, trns, scanline_stride))
                    memset(row + 1, 0, 1);
            }
        } else { /* depth <= 8 */
            struct spng__iter iter = spng__iter_init(depth, scanline);

            for (i=0; i < pixels; i++, row+=row_stride) {
                if (trns[0] == get_sample(&iter))
                    row[1] = 0;
            }
        }
    } else if (fmt == SPNG_FMT_GA16) {
        row_stride = 4;

        if (depth == 16) {
            for (i=0; i< pixels; i++, scanline+=scanline_stride, row+=row_stride) {
                if (!memcmp(scanline, trns, 2))
                    memset(row + 2, 0, 2);
            }
        } else {
            struct spng__iter iter = spng__iter_init(depth, scanline);

            for (i=0; i< pixels; i++, row+=row_stride) {
                if (trns[0] == get_sample(&iter))
                    memset(row + 2, 0, 2);
            }
        }
    } else {
        return;
    }
}

static inline void scale_row(unsigned char *row, uint32_t pixels, int fmt,
                             unsigned depth, const struct spng_sbit *sbit)
{
    uint32_t i;

    if (fmt == SPNG_FMT_RGBA8) {
        unsigned char px[4];
        for (i=0; i < pixels; i++) {
            memcpy(px, row + i * 4, 4);

            px[0] = sample_to_target(px[0], depth, sbit->red_bits, 8);
            px[1] = sample_to_target(px[1], depth, sbit->green_bits, 8);
            px[2] = sample_to_target(px[2], depth, sbit->blue_bits, 8);
            px[3] = sample_to_target(px[3], depth, sbit->alpha_bits, 8);

            memcpy(row + i * 4, px, 4);
        }
    } else if (fmt == SPNG_FMT_RGBA16) {
        uint16_t px[4];
        for (i=0; i < pixels; i++) {
            memcpy(px, row + i * 8, 8);

            px[0] = sample_to_target(px[0], depth, sbit->red_bits, 16);
            px[1] = sample_to_target(px[1], depth, sbit->green_bits, 16);
            px[2] = sample_to_target(px[2], depth, sbit->blue_bits, 16);
            px[3] = sample_to_target(px[3], depth, sbit->alpha_bits, 16);

            memcpy(row + i * 8, px, 8);
        }
    } else if (fmt == SPNG_FMT_RGB8) {
        unsigned char px[4];
        for (i=0; i < pixels; i++) {
            memcpy(px, row + i * 3, 3);

            px[0] = sample_to_target(px[0], depth, sbit->red_bits, 8);
            px[1] = sample_to_target(px[1], depth, sbit->green_bits, 8);
            px[2] = sample_to_target(px[2], depth, sbit->blue_bits, 8);

            memcpy(row + i * 3, px, 3);
        }
    } else if (fmt == SPNG_FMT_G8) {
        for (i=0; i < pixels; i++) {
            row[i] = sample_to_target(row[i], depth, sbit->grayscale_bits, 8);
        }
    } else if (fmt == SPNG_FMT_GA8) {
        for (i=0; i < pixels; i++) {
            row[i*2] = sample_to_target(row[i*2], depth, sbit->grayscale_bits, 8);
        }
    }
}
#endif

static int check_ihdr(const struct spng_ihdr *ihdr, uint32_t max_width, uint32_t max_height)
{
    if (ihdr->width > spng_u32max || !ihdr->width)
        return SPNG_EWIDTH;
    if (ihdr->height > spng_u32max || !ihdr->height)
        return SPNG_EHEIGHT;

    if (ihdr->width > max_width)
        return SPNG_EUSER_WIDTH;
    if (ihdr->height > max_height)
        return SPNG_EUSER_HEIGHT;

    switch(ihdr->color_type) {
    case SPNG_COLOR_TYPE_GRAYSCALE:
        if (!(ihdr->bit_depth == 1 || ihdr->bit_depth == 2 || ihdr->bit_depth == 4 ||
              ihdr->bit_depth == 8 || ihdr->bit_depth == 16))
              return SPNG_EBIT_DEPTH;
        break;
    case SPNG_COLOR_TYPE_TRUECOLOR:
    case SPNG_COLOR_TYPE_GRAYSCALE_ALPHA:
    case SPNG_COLOR_TYPE_TRUECOLOR_ALPHA:
        if (!(ihdr->bit_depth == 8 || ihdr->bit_depth == 16))
            return SPNG_EBIT_DEPTH;
        break;
    case SPNG_COLOR_TYPE_INDEXED:
        if (!(ihdr->bit_depth == 1 || ihdr->bit_depth == 2 || ihdr->bit_depth == 4 ||
              ihdr->bit_depth == 8))
            return SPNG_EBIT_DEPTH;
        break;
    default:
        return SPNG_ECOLOR_TYPE;
    }

    if (ihdr->compression_method)
        return SPNG_ECOMPRESSION_METHOD;
    if (ihdr->filter_method)
        return SPNG_EFILTER_METHOD;

    if (ihdr->interlace_method > 1)
        return SPNG_EINTERLACE_METHOD;

    return 0;
}

static int check_plte(const struct spng_plte *plte, const struct spng_ihdr *ihdr)
{
    if (plte == NULL || ihdr == NULL)
        return 1;

    if (plte->n_entries == 0)
        return 1;
    if (plte->n_entries > 256)
        return 1;

    if (ihdr->color_type == SPNG_COLOR_TYPE_INDEXED) {
        if (plte->n_entries > (1U << ihdr->bit_depth))
            return 1;
    }

    return 0;
}

static int check_sbit(const struct spng_sbit *sbit, const struct spng_ihdr *ihdr)
{
    if (sbit == NULL || ihdr == NULL)
        return 1;

    if (ihdr->color_type == 0) {
        if (sbit->grayscale_bits == 0)
            return SPNG_ESBIT;
        if (sbit->grayscale_bits > ihdr->bit_depth)
            return SPNG_ESBIT;
    } else if (ihdr->color_type == 2 || ihdr->color_type == 3) {
        if (sbit->red_bits == 0)
            return SPNG_ESBIT;
        if (sbit->green_bits == 0)
            return SPNG_ESBIT;
        if (sbit->blue_bits == 0)
            return SPNG_ESBIT;

        uint8_t bit_depth;
        if (ihdr->color_type == 3)
            bit_depth = 8;
        else
            bit_depth = ihdr->bit_depth;

        if (sbit->red_bits > bit_depth)
            return SPNG_ESBIT;
        if (sbit->green_bits > bit_depth)
            return SPNG_ESBIT;
        if (sbit->blue_bits > bit_depth)
            return SPNG_ESBIT;
    } else if (ihdr->color_type == 4) {
        if (sbit->grayscale_bits == 0)
            return SPNG_ESBIT;
        if (sbit->alpha_bits == 0)
            return SPNG_ESBIT;

        if (sbit->grayscale_bits > ihdr->bit_depth)
            return SPNG_ESBIT;
        if (sbit->alpha_bits > ihdr->bit_depth)
            return SPNG_ESBIT;
    } else if (ihdr->color_type == 6) {
        if (sbit->red_bits == 0)
            return SPNG_ESBIT;
        if (sbit->green_bits == 0)
            return SPNG_ESBIT;
        if (sbit->blue_bits == 0)
            return SPNG_ESBIT;
        if (sbit->alpha_bits == 0)
            return SPNG_ESBIT;

        if (sbit->red_bits > ihdr->bit_depth)
            return SPNG_ESBIT;
        if (sbit->green_bits > ihdr->bit_depth)
            return SPNG_ESBIT;
        if (sbit->blue_bits > ihdr->bit_depth)
            return SPNG_ESBIT;
        if (sbit->alpha_bits > ihdr->bit_depth)
            return SPNG_ESBIT;
    }

    return 0;
}

static int check_chrm_int(const struct spng_chrm_int *chrm_int)
{
    if (chrm_int == NULL)
        return 1;

    if (chrm_int->white_point_x > spng_u32max || chrm_int->white_point_y > spng_u32max ||
        chrm_int->red_x > spng_u32max || chrm_int->red_y > spng_u32max ||
        chrm_int->green_x  > spng_u32max || chrm_int->green_y  > spng_u32max ||
        chrm_int->blue_x > spng_u32max || chrm_int->blue_y > spng_u32max)
        return SPNG_ECHRM;

    return 0;
}

static int check_phys(const struct spng_phys *phys)
{
    if (phys == NULL)
        return 1;

    if (phys->unit_specifier > 1)
        return SPNG_EPHYS;

    if (phys->ppu_x > spng_u32max)
        return SPNG_EPHYS;
    if (phys->ppu_y > spng_u32max)
        return SPNG_EPHYS;

    return 0;
}

static int check_time(const struct spng_time *time)
{
    if (time == NULL)
        return 1;

    if (time->month == 0 || time->month > 12)
        return 1;
    if (time->day == 0 || time->day > 31)
        return 1;
    if (time->hour > 23)
        return 1;
    if (time->minute > 59)
        return 1;
    if (time->second > 60)
        return 1;

    return 0;
}

static int check_offs(const struct spng_offs *offs)
{
    if (offs == NULL)
        return 1;

    if (offs->unit_specifier > 1)
        return 1;

    return 0;
}

static int check_exif(const struct spng_exif *exif)
{
    if (exif == NULL)
        return 1;
    if (exif->data == NULL)
        return 1;

    if (exif->length < 4)
        return SPNG_ECHUNK_SIZE;
    if (exif->length > spng_u32max)
        return SPNG_ECHUNK_STDLEN;

    const uint8_t exif_le[4] = { 73, 73, 42, 0 };
    const uint8_t exif_be[4] = { 77, 77, 0, 42 };

    if (memcmp(exif->data, exif_le, 4) && memcmp(exif->data, exif_be, 4))
        return 1;

    return 0;
}

/* Validate PNG keyword */
static int check_png_keyword(const char *str)
{
    if (str == NULL)
        return 1;

    size_t len = strlen(str);
    const char *end = str + len;

    if (!len)
        return 1;
    if (len > 79)
        return 1;
    if (str[0] == ' ')
        return 1; /* Leading space */
    if (end[-1] == ' ')
        return 1; /* Trailing space */
    if (strstr(str, "  ") != NULL)
        return 1; /* Consecutive spaces */

    uint8_t c;
    while (str != end) {
        memcpy(&c, str, 1);

        if ((c >= 32 && c <= 126) || (c >= 161))
            str++;
        else
            return 1; /* Invalid character */
    }

    return 0;
}

/* Validate PNG text *str up to 'len' bytes */
static int check_png_text(const char *str, size_t len)
{/* XXX: are consecutive newlines permitted? */
    if (str == NULL || len == 0)
        return 1;

    uint8_t c;
    size_t i = 0;
    while (i < len) {
        memcpy(&c, str + i, 1);

        if ((c >= 32 && c <= 126) || (c >= 161) || c == 10)
            i++;
        else
            return 1; /* Invalid character */
    }

    return 0;
}

static int update_row_info(spng_ctx *ctx)
{
    int interlacing = ctx->ihdr.interlace_method;
    struct spng_row_info *ri = &ctx->row_info;
    const struct spng_subimage *sub = ctx->subimage;

    if (ri->scanline_idx == (sub[ri->pass].height - 1)) { /* Last scanline */
        if (ri->pass == ctx->last_pass) {
            ctx->state = SPNG_STATE_EOI;
            return SPNG_EOI;
        }

        ri->scanline_idx = 0;
        ri->pass++;

        /* Skip empty passes */
        while ((!sub[ri->pass].width || !sub[ri->pass].height) && (ri->pass < ctx->last_pass))
            ri->pass++;
    } else {
        ri->row_num++;
        ri->scanline_idx++;
    }

    if (interlacing)
        ri->row_num = adam7_y_start[ri->pass] + ri->scanline_idx * adam7_y_delta[ri->pass];

    return 0;
}

int spng_get_row_info(spng_ctx *ctx, struct spng_row_info *row_info)
{
    if (ctx == NULL || row_info == NULL || ctx->state < SPNG_STATE_DECODE_INIT)
        return 1;

    if (ctx->state >= SPNG_STATE_EOI)
        return SPNG_EOI;

    *row_info = ctx->row_info;

    return 0;
}

static int write_chunks_before_idat(spng_ctx *ctx)
{
    if (ctx == NULL)
        return SPNG_EINTERNAL;
    if (!ctx->stored.ihdr)
        return SPNG_EINTERNAL;

    int ret;
    uint32_t i;
    size_t length;
    const struct spng_ihdr *ihdr = &ctx->ihdr;
    unsigned char *data = ctx->decode_plte.raw;

    ret = write_data(ctx, spng_signature, 8);
    if (ret)
        return ret;

    write_u32(data,     ihdr->width);
    write_u32(data + 4, ihdr->height);
    data[8]  = ihdr->bit_depth;
    data[9]  = ihdr->color_type;
    data[10] = ihdr->compression_method;
    data[11] = ihdr->filter_method;
    data[12] = ihdr->interlace_method;

    ret = write_chunk(ctx, type_ihdr, data, 13);
    if (ret)
        return ret;

    if (ctx->stored.chrm) {
        write_u32(data,      ctx->chrm_int.white_point_x);
        write_u32(data + 4,  ctx->chrm_int.white_point_y);
        write_u32(data + 8,  ctx->chrm_int.red_x);
        write_u32(data + 12, ctx->chrm_int.red_y);
        write_u32(data + 16, ctx->chrm_int.green_x);
        write_u32(data + 20, ctx->chrm_int.green_y);
        write_u32(data + 24, ctx->chrm_int.blue_x);
        write_u32(data + 28, ctx->chrm_int.blue_y);

        ret = write_chunk(ctx, type_chrm, data, 32);
        if (ret)
            return ret;
    }

    if (ctx->stored.gama) {
        write_u32(data, ctx->gama);

        ret = write_chunk(ctx, type_gama, data, 4);
        if (ret)
            return ret;
    }

    if (ctx->stored.iccp) {
        uLongf dest_len = compressBound((uLong)ctx->iccp.profile_len);

        Bytef *buf = spng__malloc(ctx, dest_len);
        if (buf == NULL)
            return SPNG_EMEM;

        ret = compress2(buf, &dest_len, (void*)ctx->iccp.profile,
                             (uLong)ctx->iccp.profile_len, Z_DEFAULT_COMPRESSION);

        if (ret != Z_OK) {
            spng__free(ctx, buf);
            return SPNG_EZLIB;
        }

        size_t name_len = strlen(ctx->iccp.profile_name);

        length = name_len + 2;
        length += dest_len;

        if (dest_len > length)
            return SPNG_EOVERFLOW;

        unsigned char *cdata = NULL;

        ret = write_header(ctx, type_iccp, length, &cdata);

        if (ret) {
            spng__free(ctx, buf);
            return ret;
        }

        memcpy(cdata, ctx->iccp.profile_name, name_len + 1);
        cdata[name_len + 1] = 0; /* compression method */
        memcpy(cdata + name_len + 2, buf, dest_len);

        spng__free(ctx, buf);

        ret = finish_chunk(ctx);
        if (ret)
            return ret;
    }

    if (ctx->stored.sbit) {
        switch(ctx->ihdr.color_type) {
        case SPNG_COLOR_TYPE_GRAYSCALE:
            length = 1;
            data[0] = ctx->sbit.grayscale_bits;
            break;
        case SPNG_COLOR_TYPE_TRUECOLOR:
        case SPNG_COLOR_TYPE_INDEXED:
            length = 3;
            data[0] = ctx->sbit.red_bits;
            data[1] = ctx->sbit.green_bits;
            data[2] = ctx->sbit.blue_bits;
            break;
        case SPNG_COLOR_TYPE_GRAYSCALE_ALPHA:
            length = 2;
            data[0] = ctx->sbit.grayscale_bits;
            data[1] = ctx->sbit.alpha_bits;
            break;
        case SPNG_COLOR_TYPE_TRUECOLOR_ALPHA:
            length = 4;
            data[0] = ctx->sbit.red_bits;
            data[1] = ctx->sbit.green_bits;
            data[2] = ctx->sbit.blue_bits;
            data[3] = ctx->sbit.alpha_bits;
            break;
        default:
            return SPNG_EINTERNAL;
        }

        ret = write_chunk(ctx, type_sbit, data, length);
        if (ret)
            return ret;
    }

    if (ctx->stored.srgb) {
        ret = write_chunk(ctx, type_srgb, &ctx->srgb_rendering_intent, 1);
        if (ret)
            return ret;
    }

    ret = write_unknown_chunks(ctx, SPNG_AFTER_IHDR);
    if (ret)
        return ret;

    if (ctx->stored.plte) {
        for (i=0; i < ctx->plte.n_entries; i++) {
            data[i * 3 + 0] = ctx->plte.entries[i].red;
            data[i * 3 + 1] = ctx->plte.entries[i].green;
            data[i * 3 + 2] = ctx->plte.entries[i].blue;
        }

        ret = write_chunk(ctx, type_plte, data, ctx->plte.n_entries * 3);
        if (ret)
            return ret;
    }

    if (ctx->stored.bkgd) {
        switch(ctx->ihdr.color_type) {
        case SPNG_COLOR_TYPE_GRAYSCALE:
        case SPNG_COLOR_TYPE_GRAYSCALE_ALPHA:
            length = 2;
            write_u16(data, ctx->bkgd.gray);
            break;
        case SPNG_COLOR_TYPE_TRUECOLOR:
        case SPNG_COLOR_TYPE_TRUECOLOR_ALPHA:
            length = 6;
            write_u16(data,     ctx->bkgd.red);
            write_u16(data + 2, ctx->bkgd.green);
            write_u16(data + 4, ctx->bkgd.blue);
            break;
        case SPNG_COLOR_TYPE_INDEXED:
            length = 1;
            data[0] = ctx->bkgd.plte_index;
            break;
        default: return SPNG_EINTERNAL;
        }

        ret = write_chunk(ctx, type_bkgd, data, length);
        if (ret)
            return ret;
    }

    if (ctx->stored.hist) {
        length = ctx->plte.n_entries * 2;

        for (i=0; i < ctx->plte.n_entries; i++) {
            write_u16(data + i * 2, ctx->hist.frequency[i]);
        }

        ret = write_chunk(ctx, type_hist, data, length);
        if (ret)
            return ret;
    }

    if (ctx->stored.trns) {
        if (ctx->ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE) {
            write_u16(data, ctx->trns.gray);

            ret = write_chunk(ctx, type_trns, data, 2);
        } else if (ctx->ihdr.color_type == SPNG_COLOR_TYPE_TRUECOLOR) {
            write_u16(data,     ctx->trns.red);
            write_u16(data + 2, ctx->trns.green);
            write_u16(data + 4, ctx->trns.blue);

            ret = write_chunk(ctx, type_trns, data, 6);
        } else if (ctx->ihdr.color_type == SPNG_COLOR_TYPE_INDEXED) {
            ret = write_chunk(ctx, type_trns, ctx->trns.type3_alpha, ctx->trns.n_type3_entries);
        }

        if (ret)
            return ret;
    }

    if (ctx->stored.phys) {
        write_u32(data,     ctx->phys.ppu_x);
        write_u32(data + 4, ctx->phys.ppu_y);
        data[8] = ctx->phys.unit_specifier;

        ret = write_chunk(ctx, type_phys, data, 9);
        if (ret)
            return ret;
    }

    if (ctx->stored.splt) {
        const struct spng_splt *splt;
        unsigned char *cdata = NULL;

        uint32_t k;
        for (i=0; i < ctx->n_splt; i++) {
            splt = &ctx->splt_list[i];

            size_t name_len = strlen(splt->name);
            length = name_len + 1;

            if (splt->sample_depth == 8) {
                length += splt->n_entries * 6 + 1;
            } else if (splt->sample_depth == 16) {
                length += splt->n_entries * 10 + 1;
            }

            ret = write_header(ctx, type_splt, length, &cdata);
            if (ret)
                return ret;

            memcpy(cdata, splt->name, name_len + 1);
            cdata += name_len + 2;
            cdata[-1] = splt->sample_depth;

            if (splt->sample_depth == 8) {
                for (k=0; k < splt->n_entries; k++) {
                    cdata[k * 6 + 0] = splt->entries[k].red;
                    cdata[k * 6 + 1] = splt->entries[k].green;
                    cdata[k * 6 + 2] = splt->entries[k].blue;
                    cdata[k * 6 + 3] = splt->entries[k].alpha;
                    write_u16(cdata + k * 6 + 4, splt->entries[k].frequency);
                }
            } else if (splt->sample_depth == 16) {
                for (k=0; k < splt->n_entries; k++) {
                    write_u16(cdata + k * 10 + 0, splt->entries[k].red);
                    write_u16(cdata + k * 10 + 2, splt->entries[k].green);
                    write_u16(cdata + k * 10 + 4, splt->entries[k].blue);
                    write_u16(cdata + k * 10 + 6, splt->entries[k].alpha);
                    write_u16(cdata + k * 10 + 8, splt->entries[k].frequency);
                }
            }

            ret = finish_chunk(ctx);
            if (ret)
                return ret;
        }
    }

    if (ctx->stored.time) {
        write_u16(data, ctx->time.year);
        data[2] = ctx->time.month;
        data[3] = ctx->time.day;
        data[4] = ctx->time.hour;
        data[5] = ctx->time.minute;
        data[6] = ctx->time.second;

        ret = write_chunk(ctx, type_time, data, 7);
        if (ret)
            return ret;
    }

    if (ctx->stored.text) {
        unsigned char *cdata = NULL;
        const struct spng_text2 *text;
        const uint8_t *text_type_array[4] = { 0, type_text, type_ztxt, type_itxt };

        for (i=0; i < ctx->n_text; i++) {
            text = &ctx->text_list[i];

            const uint8_t *text_chunk_type = text_type_array[text->type];
            Bytef *compressed_text = NULL;
            size_t keyword_len = 0;
            size_t language_tag_len = 0;
            size_t translated_keyword_len = 0;
            size_t compressed_length = 0;
            size_t text_length = 0;

            keyword_len = strlen(text->keyword);
            text_length = strlen(text->text);

            length = keyword_len + 1;

            if (text->type == SPNG_ZTXT) {
                length += 1; /* compression method */
            } else if (text->type == SPNG_ITXT) {
                if (!text->language_tag || !text->translated_keyword)
                    return SPNG_EINTERNAL;

                language_tag_len = strlen(text->language_tag);
                translated_keyword_len = strlen(text->translated_keyword);

                length += language_tag_len;
                if (length < language_tag_len)
                    return SPNG_EOVERFLOW;

                length += translated_keyword_len;
                if (length < translated_keyword_len)
                    return SPNG_EOVERFLOW;

                length += 4; /* compression flag + method + nul for the two strings */
                if (length < 4)
                    return SPNG_EOVERFLOW;
            }

            if (text->compression_flag) {
                ret = spng__deflate_init(ctx, &ctx->text_options);
                if (ret)
                    return ret;
#if 0
                z_stream *zstream = &ctx->zstream;
                uLongf dest_len = deflateBound(zstream, (uLong)text_length);

                compressed_text = spng__malloc(ctx, dest_len);

                if (compressed_text == NULL)
                    return SPNG_EMEM;

                zstream->next_in = (void*)text->text;
                zstream->avail_in = (uInt)text_length;

                zstream->next_out = compressed_text;
                zstream->avail_out = dest_len;

                ret = deflate(zstream, Z_FINISH);

                if (ret != Z_STREAM_END) {
                    spng__free(ctx, compressed_text);
                    return SPNG_EZLIB;
                }

                compressed_length = zstream->total_out;

                length += compressed_length;
                if (length < compressed_length)
                    return SPNG_EOVERFLOW;
#endif
            } else {
                text_length = strlen(text->text);

                length += text_length;
                if (length < text_length)
                    return SPNG_EOVERFLOW;
            }

            ret = write_header(ctx, text_chunk_type, length, &cdata);
            if (ret) {
                spng__free(ctx, compressed_text);
                return ret;
            }

            memcpy(cdata, text->keyword, keyword_len + 1);
            cdata += keyword_len + 1;

            if (text->type == SPNG_ITXT) {
                cdata[0] = text->compression_flag;
                cdata[1] = 0; /* compression method */
                cdata += 2;

                memcpy(cdata, text->language_tag, language_tag_len + 1);
                cdata += language_tag_len + 1;

                memcpy(cdata, text->translated_keyword, translated_keyword_len + 1);
                cdata += translated_keyword_len + 1;
            } else if (text->type == SPNG_ZTXT) {
                cdata[0] = 0; /* compression method */
                cdata++;
            }

            if (text->compression_flag)
                memcpy(cdata, compressed_text, compressed_length);
            else
                memcpy(cdata, text->text, text_length);

            spng__free(ctx, compressed_text);

            ret = finish_chunk(ctx);
            if (ret)
                return ret;
        }
    }

    if (ctx->stored.offs) {
        write_s32(data,     ctx->offs.x);
        write_s32(data + 4, ctx->offs.y);
        data[8] = ctx->offs.unit_specifier;

        ret = write_chunk(ctx, type_offs, data, 9);
        if (ret)
            return ret;
    }

    if (ctx->stored.exif) {
        ret = write_chunk(ctx, type_exif, ctx->exif.data, ctx->exif.length);
        if (ret)
            return ret;
    }

    ret = write_unknown_chunks(ctx, SPNG_AFTER_PLTE);
    if (ret)
        return ret;

    return 0;
}

static int write_chunks_after_idat(spng_ctx *ctx)
{
    if (ctx == NULL)
        return SPNG_EINTERNAL;

    int ret = write_unknown_chunks(ctx, SPNG_AFTER_IDAT);
    if (ret)
        return ret;

    return write_iend(ctx);
}

/* Compress and write scanline to IDAT stream */
static int write_idat_bytes(spng_ctx *ctx, const void *scanline, size_t len, int flush)
{
    if (ctx == NULL || scanline == NULL)
        return SPNG_EINTERNAL;
    if (len > UINT_MAX)
        return SPNG_EINTERNAL;

    (void)flush;
#if 0
    int ret = 0;
    unsigned char *data = NULL;
    z_stream *zstream = &ctx->zstream;
    uint32_t idat_length = SPNG_WRITE_SIZE;

    zstream->next_in = scanline;
    zstream->avail_in = (uInt)len;

    do {
        ret = deflate(zstream, flush);

        if (zstream->avail_out == 0) {
            ret = finish_chunk(ctx);
            if (ret)
                return encode_err(ctx, ret);

            ret = write_header(ctx, type_idat, idat_length, &data);
            if (ret)
                return encode_err(ctx, ret);

            zstream->next_out = data;
            zstream->avail_out = idat_length;
        }

    } while (zstream->avail_in);

    if (ret != Z_OK)
        return SPNG_EZLIB;
#endif
    return 0;
}

static int finish_idat(spng_ctx *ctx)
{
#if 0
    int ret = 0;
    unsigned char *data = NULL;
    z_stream *zstream = &ctx->zstream;
    uint32_t idat_length = SPNG_WRITE_SIZE;

    while (ret != Z_STREAM_END) {
        ret = deflate(zstream, Z_FINISH);

        if (ret) {
            if (ret == Z_STREAM_END)
                break;
            if (ret != Z_BUF_ERROR)
                return SPNG_EZLIB;
        }

        if (zstream->avail_out == 0) {
            ret = finish_chunk(ctx);
            if (ret)
                return encode_err(ctx, ret);

            ret = write_header(ctx, type_idat, idat_length, &data);
            if (ret)
                return encode_err(ctx, ret);

            zstream->next_out = data;
            zstream->avail_out = idat_length;
        }
    }

    uint32_t trimmed_length = idat_length - zstream->avail_out;

    ret = trim_chunk(ctx, trimmed_length);
    if (ret)
        return ret;
#endif
    return finish_chunk(ctx);
}

static int encode_scanline(spng_ctx *ctx, const void *scanline, size_t len)
{
    if (ctx == NULL || scanline == NULL)
        return SPNG_EINTERNAL;

    int ret, pass = ctx->row_info.pass;
    uint8_t filter = 0;
    struct spng_row_info *ri = &ctx->row_info;
    const struct spng_subimage *sub = ctx->subimage;
    struct encode_flags f = ctx->encode_flags;
    unsigned char *filtered_scanline = ctx->filtered_scanline;
    size_t scanline_width = sub[pass].scanline_width;

    if (len < scanline_width - 1)
        return SPNG_EINTERNAL;

    /* encode_row() interlaces directly to ctx->scanline */
    if (scanline != ctx->scanline)
        memcpy(ctx->scanline, scanline, scanline_width - 1);

    if (f.to_bigendian)
        u16_row_to_bigendian(ctx->scanline, scanline_width - 1);

    const int requires_previous = f.filter_choice & (SPNG_FILTER_CHOICE_UP |
                                                     SPNG_FILTER_CHOICE_AVG |
                                                     SPNG_FILTER_CHOICE_PAETH);

    /* XXX: exclude 'requires_previous' filters by default for first scanline? */
    if (!ri->scanline_idx && requires_previous) {
        /* prev_scanline is all zeros for the first scanline */
        memset(ctx->prev_scanline, 0, scanline_width);
    }

    filter = get_best_filter(ctx->prev_scanline, ctx->scanline, scanline_width,
                                                 ctx->bytes_per_pixel, f.filter_choice);

    if (!filter)
        filtered_scanline = ctx->scanline;

    filtered_scanline[-1] = filter;

    if (filter) {
        ret = filter_scanline(filtered_scanline, ctx->prev_scanline, ctx->scanline,
                                                 scanline_width, ctx->bytes_per_pixel, filter);
        if (ret)
            return encode_err(ctx, ret);
    }

    ret = write_idat_bytes(ctx, filtered_scanline - 1, scanline_width, Z_NO_FLUSH);
    if (ret)
        return encode_err(ctx, ret);

    /* The previous scanline is always unfiltered */
    void *t = ctx->prev_scanline;
    ctx->prev_scanline = ctx->scanline;
    ctx->scanline = t;

    ret = update_row_info(ctx);

    if (ret == SPNG_EOI) {
        int error = finish_idat(ctx);
        if (error)
            encode_err(ctx, error);

        if (f.finalize) {
            error = spng_encode_chunks(ctx);
            if (error)
                return encode_err(ctx, error);
        }
    }

    return ret;
}

static int encode_row(spng_ctx *ctx, const void *row, size_t len)
{
    if (ctx == NULL || row == NULL)
        return SPNG_EINTERNAL;

    const int pass = ctx->row_info.pass;

    if (!ctx->ihdr.interlace_method || pass == 6)
        return encode_scanline(ctx, row, len);

    uint32_t k;
    const unsigned pixel_size = ctx->pixel_size;
    const unsigned bit_depth = ctx->ihdr.bit_depth;

    if (bit_depth < 8) {
        const unsigned samples_per_byte = 8 / bit_depth;
        const uint8_t mask = (1 << bit_depth) - 1;
        const unsigned initial_shift = 8 - bit_depth;
        unsigned shift_amount = initial_shift;

        unsigned char *scanline = ctx->scanline;
        const unsigned char *row_uc = row;
        uint8_t sample;

        memset(scanline, 0, ctx->subimage[pass].scanline_width);

        for (k=0; k < ctx->subimage[pass].width; k++) {
            size_t ioffset = adam7_x_start[pass] + k * adam7_x_delta[pass];

            sample = row_uc[ioffset / samples_per_byte];

            sample = sample >> (initial_shift - ioffset * bit_depth % 8);
            sample = sample & mask;
            sample = sample << shift_amount;

            scanline[0] |= sample;

            shift_amount -= bit_depth;

            if (shift_amount > 7) {
                shift_amount = initial_shift;
                scanline++;
            }
        }

        return encode_scanline(ctx, ctx->scanline, len);
    }

    for (k=0; k < ctx->subimage[pass].width; k++) {
        size_t ioffset = (adam7_x_start[pass] + (size_t) k * adam7_x_delta[pass]) * pixel_size;

        memcpy(ctx->scanline + k * pixel_size, (unsigned char*)row + ioffset, pixel_size);
    }

    return encode_scanline(ctx, ctx->scanline, len);
}

int spng_encode_scanline(spng_ctx *ctx, const void *scanline, size_t len)
{
    if (ctx == NULL || scanline == NULL)
        return SPNG_EINVAL;
    if (ctx->state >= SPNG_STATE_EOI)
        return SPNG_EOI;
    if (len < (ctx->subimage[ctx->row_info.pass].scanline_width -1))
        return SPNG_EBUFSIZ;

    return encode_scanline(ctx, scanline, len);
}

int spng_encode_row(spng_ctx *ctx, const void *row, size_t len)
{
    if (ctx == NULL || row == NULL)
        return SPNG_EINVAL;
    if (ctx->state >= SPNG_STATE_EOI)
        return SPNG_EOI;
    if (len < ctx->image_width)
        return SPNG_EBUFSIZ;

    return encode_row(ctx, row, len);
}

int spng_encode_chunks(spng_ctx *ctx)
{
    if (ctx == NULL)
        return 1;
    if (!ctx->state)
        return SPNG_EBADSTATE;
    if (ctx->state < SPNG_STATE_OUTPUT)
        return SPNG_ENODST;

    int ret = 0;

    if (ctx->state < SPNG_STATE_FIRST_IDAT) {
        if (!ctx->stored.ihdr)
            return SPNG_ENOIHDR;

        ret = write_chunks_before_idat(ctx);
        if (ret)
            return encode_err(ctx, ret);

        ctx->state = SPNG_STATE_FIRST_IDAT;
    } else if (ctx->state == SPNG_STATE_FIRST_IDAT) {
        return 0;
    } else if (ctx->state == SPNG_STATE_EOI) {
        ret = write_chunks_after_idat(ctx);
        if (ret)
            return encode_err(ctx, ret);

        ctx->state = SPNG_STATE_IEND;
    } else {
        return SPNG_EOPSTATE;
    }

    return 0;
}

int spng_encode_image(spng_ctx *ctx, const void *img, size_t len, int fmt, int flags)
{
    if (ctx == NULL)
        return 1;
    if (!ctx->state)
        return SPNG_EBADSTATE;
    if (!ctx->stored.ihdr)
        return SPNG_ENOIHDR;
    if (!(fmt == SPNG_FMT_PNG || fmt == SPNG_FMT_RAW))
        return SPNG_EFMT;

    int ret = 0;
    const struct spng_ihdr *ihdr = &ctx->ihdr;
    struct encode_flags *encode_flags = &ctx->encode_flags;

    if (ihdr->color_type == SPNG_COLOR_TYPE_INDEXED && !ctx->stored.plte)
        return SPNG_ENOPLTE;

    ret = calculate_image_width(ihdr, fmt, &ctx->image_width);
    if (ret)
        return encode_err(ctx, ret);

    if (ctx->image_width > SIZE_MAX / ihdr->height)
        ctx->image_size = 0; /* overflow */
    else
        ctx->image_size = ctx->image_width * ihdr->height;

    if (!(flags & SPNG_ENCODE_PROGRESSIVE) ) {
        if (img == NULL)
            return 1;
        if (!ctx->image_size)
            return SPNG_EOVERFLOW;
        if (len != ctx->image_size)
            return SPNG_EBUFSIZ;
    }

    ret = spng_encode_chunks(ctx);
    if (ret)
        return encode_err(ctx, ret);

    ret = calculate_subimages(ctx);
    if (ret)
        return encode_err(ctx, ret);

    if (ihdr->bit_depth < 8)
        ctx->bytes_per_pixel = 1;
    else
        ctx->bytes_per_pixel = num_channels(ihdr) * (ihdr->bit_depth / 8);

    if (spng__optimize(SPNG_FILTER_CHOICE)) {
        /* Filtering would make no difference */
        if (!ctx->image_options.compression_level)
            encode_flags->filter_choice = SPNG_DISABLE_FILTERING;

        /* Palette indices and low bit-depth images do not benefit from filtering */
        if (ihdr->color_type == SPNG_COLOR_TYPE_INDEXED || ihdr->bit_depth < 8)
            encode_flags->filter_choice = SPNG_DISABLE_FILTERING;
    }

    /* This is technically the same as disabling filtering */
    if (encode_flags->filter_choice == SPNG_FILTER_CHOICE_NONE)
        encode_flags->filter_choice = SPNG_DISABLE_FILTERING;

    if (!encode_flags->filter_choice && spng__optimize(SPNG_IMG_COMPRESSION_STRATEGY))
        ctx->image_options.strategy = Z_DEFAULT_STRATEGY;

    ret = spng__deflate_init(ctx, &ctx->image_options);
    if (ret)
        return encode_err(ctx, ret);

    size_t scanline_buf_size = ctx->subimage[ctx->widest_pass].scanline_width;

    scanline_buf_size += 32;

    if (scanline_buf_size < 32)
        return SPNG_EOVERFLOW;

    ctx->scanline_buf = spng__malloc(ctx, scanline_buf_size);
    ctx->prev_scanline_buf = spng__malloc(ctx, scanline_buf_size);

    if (ctx->scanline_buf == NULL || ctx->prev_scanline_buf == NULL)
        return encode_err(ctx, SPNG_EMEM);

    /* Maintain alignment for pixels, filter at [-1] */
    ctx->scanline = ctx->scanline_buf + 16;
    ctx->prev_scanline = ctx->prev_scanline_buf + 16;

    if (encode_flags->filter_choice) {
        ctx->filtered_scanline_buf = spng__malloc(ctx, scanline_buf_size);
        if (ctx->filtered_scanline_buf == NULL)
            return encode_err(ctx, SPNG_EMEM);

        ctx->filtered_scanline = ctx->filtered_scanline_buf + 16;
    }

    struct spng_subimage *sub = ctx->subimage;
    struct spng_row_info *ri = &ctx->row_info;

    ctx->fmt = fmt;
#if 0
    z_stream *zstream = &ctx->zstream;
    zstream->avail_out = SPNG_WRITE_SIZE;

    ret = write_header(ctx, type_idat, zstream->avail_out, &zstream->next_out);
    if (ret)
        return encode_err(ctx, ret);
#endif

    if (ihdr->interlace_method)
        encode_flags->interlace = 1;

    if (fmt & (SPNG_FMT_PNG | SPNG_FMT_RAW))
        encode_flags->same_layout = 1;

    if (ihdr->bit_depth == 16 && fmt != SPNG_FMT_RAW)
        encode_flags->to_bigendian = 1;

    if (flags & SPNG_ENCODE_FINALIZE)
        encode_flags->finalize = 1;

    while (!sub[ri->pass].width || !sub[ri->pass].height)
        ri->pass++;

    if (encode_flags->interlace)
        ri->row_num = adam7_y_start[ri->pass];

    ctx->pixel_size = 4; /* SPNG_FMT_RGBA8 */
#if 0
    if (fmt == SPNG_FMT_RGBA16)
        ctx->pixel_size = 8;
    else if (fmt == SPNG_FMT_RGB8)
        ctx->pixel_size = 3;
    else if (fmt == SPNG_FMT_G8)
        ctx->pixel_size = 1;
    else if (fmt == SPNG_FMT_GA8)
        ctx->pixel_size = 2;
    else if (fmt & (SPNG_FMT_PNG | SPNG_FMT_RAW))
        ctx->pixel_size = ctx->bytes_per_pixel;
#endif
    ctx->state = SPNG_STATE_ENCODE_INIT;

    if (flags & SPNG_ENCODE_PROGRESSIVE) {
        encode_flags->progressive = 1;
        return 0;
    }

    do {
        size_t ioffset = ri->row_num * ctx->image_width;
        ret = encode_row(ctx, (unsigned char*)img + ioffset, ctx->image_width);
    } while (!ret);

    if (ret != SPNG_EOI)
        return encode_err(ctx, ret);

    return 0;
}

spng_ctx *spng_ctx_new(int flags)
{
    struct spng_alloc alloc = {
        .malloc_fn = malloc,
        .realloc_fn = realloc,
        .calloc_fn = calloc,
        .free_fn = free
    };

    return spng_ctx_new2(&alloc, flags);
}

spng_ctx *spng_ctx_new2(struct spng_alloc *alloc, int flags)
{
    if (alloc == NULL)
        return NULL;
    if (flags != (flags & SPNG__CTX_FLAGS_ALL))
        return NULL;

    if (alloc->malloc_fn == NULL)
        return NULL;
    if (alloc->realloc_fn == NULL)
        return NULL;
    if (alloc->calloc_fn == NULL)
        return NULL;
    if (alloc->free_fn == NULL)
        return NULL;

    spng_ctx *ctx = alloc->calloc_fn(1, sizeof(spng_ctx));
    if (ctx == NULL)
        return NULL;

    ctx->alloc = *alloc;

    ctx->max_width = spng_u32max;
    ctx->max_height = spng_u32max;

    ctx->max_chunk_size = spng_u32max;
    ctx->chunk_cache_limit = SIZE_MAX;
    ctx->chunk_count_limit = SPNG_MAX_CHUNK_COUNT;

    ctx->state = SPNG_STATE_INIT;

    ctx->crc_action_critical = SPNG_CRC_ERROR;
    ctx->crc_action_ancillary = SPNG_CRC_DISCARD;

    const struct spng__zlib_options image_defaults = {
        .compression_level = Z_DEFAULT_COMPRESSION,
        .window_bits = 15,
        .mem_level = 8,
        .strategy = Z_FILTERED,
        .data_type = 0 /* Z_BINARY */
    };

    const struct spng__zlib_options text_defaults = {
        .compression_level = Z_DEFAULT_COMPRESSION,
        .window_bits = 15,
        .mem_level = 8,
        .strategy = Z_DEFAULT_STRATEGY,
        .data_type = 1 /* Z_TEXT */
    };

    ctx->image_options = image_defaults;
    ctx->text_options = text_defaults;

    ctx->optimize_option = ~0;
    ctx->encode_flags.filter_choice = SPNG_FILTER_CHOICE_ALL;

    ctx->flags = flags;

    return ctx;
}

void spng_ctx_free(spng_ctx *ctx)
{
    if (ctx == NULL)
        return;

    if (ctx->streaming && ctx->stream_buf != NULL)
        spng__free(ctx, ctx->stream_buf);

    if (!ctx->user.exif)
        spng__free(ctx, ctx->exif.data);

    if (!ctx->user.iccp)
        spng__free(ctx, ctx->iccp.profile);

    uint32_t i;

    if (ctx->splt_list != NULL && !ctx->user.splt) {
        for (i=0; i < ctx->n_splt; i++) {
            spng__free(ctx, ctx->splt_list[i].entries);
        }
        spng__free(ctx, ctx->splt_list);
    }

    if (ctx->text_list != NULL) {
        for (i=0; i< ctx->n_text; i++) {
            if (ctx->user.text)
                break;

            spng__free(ctx, ctx->text_list[i].keyword);
            if (ctx->text_list[i].compression_flag)
                spng__free(ctx, ctx->text_list[i].text);
        }
        spng__free(ctx, ctx->text_list);
    }

    if (ctx->chunk_list != NULL && !ctx->user.unknown) {
        for (i=0; i< ctx->n_chunks; i++) {
            spng__free(ctx, ctx->chunk_list[i].data);
        }
        spng__free(ctx, ctx->chunk_list);
    }
#if 0
    if (ctx->deflate)
        deflateEnd(&ctx->zstream);
    else
        inflateEnd(&ctx->zstream);
#endif
    if (!ctx->user_owns_out_png)
        spng__free(ctx, ctx->out_png);

    spng__free(ctx, ctx->gamma_lut16);
    spng__free(ctx, ctx->row_buf);
    spng__free(ctx, ctx->scanline_buf);
    spng__free(ctx, ctx->prev_scanline_buf);
    spng__free(ctx, ctx->filtered_scanline_buf);

    spng_free_fn *free_fn = ctx->alloc.free_fn;

    memset(ctx, 0, sizeof(spng_ctx));

    free_fn(ctx);
}

static int file_write_fn(spng_ctx *ctx, void *user, void *data, size_t n)
{
    FILE *file = user;
    (void)ctx;

    if (fwrite(data, n, 1, file) != 1)
        return SPNG_IO_ERROR;

    return 0;
}

int spng_set_png_stream(spng_ctx *ctx, spng_rw_fn *rw_func, void *user)
{
    if (ctx == NULL || rw_func == NULL)
        return 1;
    if (!ctx->state)
        return SPNG_EBADSTATE;

    /* SPNG_STATE_OUTPUT shares the same value */
    if (ctx->state >= SPNG_STATE_INPUT)
        return SPNG_EBUF_SET;

    if (ctx->out_png != NULL)
        return SPNG_EBUF_SET;

    ctx->write_fn = rw_func;
    ctx->write_ptr = ctx->stream_buf;

    ctx->state = SPNG_STATE_OUTPUT;

    ctx->stream_user_ptr = user;

    ctx->streaming = 1;

    return 0;
}

int spng_set_png_file(spng_ctx *ctx, FILE *file)
{
    if (file == NULL)
        return 1;

    return spng_set_png_stream(ctx, file_write_fn, file);
}

void *spng_get_png_buffer(spng_ctx *ctx, size_t *len, int *error)
{
    int tmp = 0;
    error = error ? error : &tmp;
    *error = 0;

    if (ctx == NULL || !len)
        *error = SPNG_EINVAL;

    if (*error)
        return NULL;

    if (!ctx->state)
        *error = SPNG_EBADSTATE;
    else if (!ctx->internal_buffer)
        *error = SPNG_EOPSTATE;
    else if (ctx->state < SPNG_STATE_EOI)
        *error = SPNG_EOPSTATE;
    else if (ctx->state != SPNG_STATE_IEND)
        *error = SPNG_ENOTFINAL;

    if (*error)
        return NULL;

    ctx->user_owns_out_png = 1;

    *len = ctx->bytes_encoded;

    return ctx->out_png;
}

int spng_set_image_limits(spng_ctx *ctx, uint32_t width, uint32_t height)
{
    if (ctx == NULL)
        return 1;

    if (width > spng_u32max || height > spng_u32max)
        return 1;

    ctx->max_width = width;
    ctx->max_height = height;

    return 0;
}

int spng_get_image_limits(spng_ctx *ctx, uint32_t *width, uint32_t *height)
{
    if (ctx == NULL || width == NULL || height == NULL)
        return 1;

    *width = ctx->max_width;
    *height = ctx->max_height;

    return 0;
}

int spng_set_chunk_limits(spng_ctx *ctx, size_t chunk_size, size_t cache_limit)
{
    if (ctx == NULL || chunk_size > spng_u32max || chunk_size > cache_limit)
        return 1;

    ctx->max_chunk_size = chunk_size;

    ctx->chunk_cache_limit = cache_limit;

    return 0;
}

int spng_get_chunk_limits(spng_ctx *ctx, size_t *chunk_size, size_t *cache_limit)
{
    if (ctx == NULL || chunk_size == NULL || cache_limit == NULL)
        return 1;

    *chunk_size = ctx->max_chunk_size;

    *cache_limit = ctx->chunk_cache_limit;

    return 0;
}

int spng_set_option(spng_ctx *ctx, enum spng_option option, int value)
{
    if (ctx == NULL)
        return 1;
    if (!ctx->state)
        return SPNG_EBADSTATE;

    switch(option) {
    case SPNG_KEEP_UNKNOWN_CHUNKS:
        ctx->keep_unknown = value ? 1 : 0;
        break;
    case SPNG_IMG_COMPRESSION_LEVEL:
        ctx->image_options.compression_level = value;
        break;
    case SPNG_IMG_WINDOW_BITS:
        ctx->image_options.window_bits = value;
        break;
    case SPNG_IMG_MEM_LEVEL:
        ctx->image_options.mem_level = value;
        break;
    case SPNG_IMG_COMPRESSION_STRATEGY:
        ctx->image_options.strategy = value;
        break;
    case SPNG_TEXT_COMPRESSION_LEVEL:
        ctx->text_options.compression_level = value;
        break;
    case SPNG_TEXT_WINDOW_BITS:
        ctx->text_options.window_bits = value;
        break;
    case SPNG_TEXT_MEM_LEVEL:
        ctx->text_options.mem_level = value;
        break;
    case SPNG_TEXT_COMPRESSION_STRATEGY:
        ctx->text_options.strategy = value;
        break;
    case SPNG_FILTER_CHOICE:
        if (value & ~SPNG_FILTER_CHOICE_ALL)
            return 1;
        ctx->encode_flags.filter_choice = value;
        break;
    case SPNG_CHUNK_COUNT_LIMIT:
        if (value < 0)
            return 1;
        if (value > (int)ctx->chunk_count_total)
            return 1;
        ctx->chunk_count_limit = value;
        break;
    case SPNG_ENCODE_TO_BUFFER:
        if (value < 0)
            return 1;
        if (ctx->state >= SPNG_STATE_OUTPUT)
            return SPNG_EOPSTATE;

        if (!value)
            break;

        ctx->internal_buffer = 1;
        ctx->state = SPNG_STATE_OUTPUT;

        break;
    default:
        return 1;
    }

    /* Option can no longer be overridden by the library */
    if (option < 32)
        ctx->optimize_option &= ~(1 << option);

    return 0;
}

int spng_get_option(spng_ctx *ctx, enum spng_option option, int *value)
{
    if (ctx == NULL || value == NULL)
        return 1;
    if (!ctx->state)
        return SPNG_EBADSTATE;

    switch(option) {
    case SPNG_KEEP_UNKNOWN_CHUNKS:
        *value = ctx->keep_unknown;
        break;
    case SPNG_IMG_COMPRESSION_LEVEL:
        *value = ctx->image_options.compression_level;
        break;
    case SPNG_IMG_WINDOW_BITS:
        *value = ctx->image_options.window_bits;
        break;
    case SPNG_IMG_MEM_LEVEL:
        *value = ctx->image_options.mem_level;
        break;
    case SPNG_IMG_COMPRESSION_STRATEGY:
        *value = ctx->image_options.strategy;
        break;
    case SPNG_TEXT_COMPRESSION_LEVEL:
        *value = ctx->text_options.compression_level;
        break;
    case SPNG_TEXT_WINDOW_BITS:
        *value = ctx->text_options.window_bits;
        break;
    case SPNG_TEXT_MEM_LEVEL:
        *value = ctx->text_options.mem_level;
        break;
    case SPNG_TEXT_COMPRESSION_STRATEGY:
        *value = ctx->text_options.strategy;
        break;
    case SPNG_FILTER_CHOICE:
        *value = ctx->encode_flags.filter_choice;
        break;
    case SPNG_CHUNK_COUNT_LIMIT:
        *value = ctx->chunk_count_limit;
        break;
    case SPNG_ENCODE_TO_BUFFER:
        if (ctx->internal_buffer)
            *value = 1;
        else
            *value = 0;
        break;
    default: return 1;
    }

    return 0;
}

int spng_set_ihdr(spng_ctx *ctx, struct spng_ihdr *ihdr)
{
    if ((ctx == NULL) || (ihdr == NULL))
        return 1;

    if (ctx->stored.ihdr)
        return 1;

    int ret = check_ihdr(ihdr, ctx->max_width, ctx->max_height);
    if (ret)
        return ret;

    ctx->ihdr = *ihdr;

    ctx->stored.ihdr = 1;
    ctx->user.ihdr = 1;

    return 0;
}

int spng_set_plte(spng_ctx *ctx, struct spng_plte *plte)
{
    if ((ctx == NULL) || (plte == NULL))
        return 1;

    if (!ctx->stored.ihdr)
        return 1;

    if (check_plte(plte, &ctx->ihdr))
        return 1;

    ctx->plte.n_entries = plte->n_entries;

    memcpy(ctx->plte.entries, plte->entries, plte->n_entries * sizeof(struct spng_plte_entry));

    ctx->stored.plte = 1;
    ctx->user.plte = 1;

    return 0;
}

int spng_set_trns(spng_ctx *ctx, struct spng_trns *trns)
{
    if ((ctx == NULL) || (trns == NULL))
        return 1;

    if (!ctx->stored.ihdr)
        return SPNG_ENOIHDR;

    if (ctx->ihdr.color_type == SPNG_COLOR_TYPE_GRAYSCALE) {
        ctx->trns.gray = trns->gray;
    } else if (ctx->ihdr.color_type == SPNG_COLOR_TYPE_TRUECOLOR) {
        ctx->trns.red = trns->red;
        ctx->trns.green = trns->green;
        ctx->trns.blue = trns->blue;
    } else if (ctx->ihdr.color_type == SPNG_COLOR_TYPE_INDEXED) {
        if (!ctx->stored.plte)
            return SPNG_ETRNS_NO_PLTE;
        if (trns->n_type3_entries > ctx->plte.n_entries)
            return 1;

        ctx->trns.n_type3_entries = trns->n_type3_entries;
        memcpy(ctx->trns.type3_alpha, trns->type3_alpha, trns->n_type3_entries);
    } else {
        return SPNG_ETRNS_COLOR_TYPE;
    }

    ctx->stored.trns = 1;
    ctx->user.trns = 1;

    return 0;
}

int spng_set_chrm(spng_ctx *ctx, struct spng_chrm *chrm)
{
    if ((ctx == NULL) || (chrm == NULL))
        return 1;

    struct spng_chrm_int chrm_int;

    chrm_int.white_point_x = (uint32_t)(chrm->white_point_x * 100000.0);
    chrm_int.white_point_y = (uint32_t)(chrm->white_point_y * 100000.0);
    chrm_int.red_x = (uint32_t)(chrm->red_x * 100000.0);
    chrm_int.red_y = (uint32_t)(chrm->red_y * 100000.0);
    chrm_int.green_x = (uint32_t)(chrm->green_x * 100000.0);
    chrm_int.green_y = (uint32_t)(chrm->green_y * 100000.0);
    chrm_int.blue_x = (uint32_t)(chrm->blue_x * 100000.0);
    chrm_int.blue_y = (uint32_t)(chrm->blue_y * 100000.0);

    if (check_chrm_int(&chrm_int))
        return SPNG_ECHRM;

    ctx->chrm_int = chrm_int;

    ctx->stored.chrm = 1;
    ctx->user.chrm = 1;

    return 0;
}

int spng_set_chrm_int(spng_ctx *ctx, struct spng_chrm_int *chrm_int)
{
    if ((ctx == NULL) || (chrm_int == NULL))
        return 1;

    if (check_chrm_int(chrm_int))
        return SPNG_ECHRM;

    ctx->chrm_int = *chrm_int;

    ctx->stored.chrm = 1;
    ctx->user.chrm = 1;

    return 0;
}

int spng_set_gama(spng_ctx *ctx, double gamma)
{
    if (ctx == NULL)
        return 1;

    uint32_t gama = gamma * 100000.0;

    if (!gama)
        return 1;
    if (gama > spng_u32max)
        return 1;

    ctx->gama = gama;

    ctx->stored.gama = 1;
    ctx->user.gama = 1;

    return 0;
}

int spng_set_gama_int(spng_ctx *ctx, uint32_t gamma)
{
    if (ctx == NULL)
        return 1;

    if (!gamma)
        return 1;
    if (gamma > spng_u32max)
        return 1;

    ctx->gama = gamma;

    ctx->stored.gama = 1;
    ctx->user.gama = 1;

    return 0;
}

int spng_set_iccp(spng_ctx *ctx, struct spng_iccp *iccp)
{
    if ((ctx == NULL) || (iccp == NULL))
        return 1;

    if (check_png_keyword(iccp->profile_name))
        return SPNG_EICCP_NAME;
    if (!iccp->profile_len)
        return SPNG_ECHUNK_SIZE;
    if (iccp->profile_len > spng_u32max)
        return SPNG_ECHUNK_STDLEN;

    if (ctx->iccp.profile && !ctx->user.iccp)
        spng__free(ctx, ctx->iccp.profile);

    ctx->iccp = *iccp;

    ctx->stored.iccp = 1;
    ctx->user.iccp = 1;

    return 0;
}

int spng_set_sbit(spng_ctx *ctx, struct spng_sbit *sbit)
{
    if ((ctx == NULL) || (sbit == NULL))
        return 1;

    if (check_sbit(sbit, &ctx->ihdr))
        return 1;

    if (!ctx->stored.ihdr)
        return 1;

    ctx->sbit = *sbit;

    ctx->stored.sbit = 1;
    ctx->user.sbit = 1;

    return 0;
}

int spng_set_srgb(spng_ctx *ctx, uint8_t rendering_intent)
{
    if (ctx == NULL)
        return 1;

    if (rendering_intent > 3)
        return 1;

    ctx->srgb_rendering_intent = rendering_intent;

    ctx->stored.srgb = 1;
    ctx->user.srgb = 1;

    return 0;
}

int spng_set_text(spng_ctx *ctx, struct spng_text *text, uint32_t n_text)
{
    if ((ctx == NULL) || (text == NULL))
        return 1;
    if (!n_text)
        return 1;

    uint32_t i;
    for (i=0; i < n_text; i++) {
        if (check_png_keyword(text[i].keyword))
            return SPNG_ETEXT_KEYWORD;
        if (!text[i].length)
            return 1;
        if (text[i].length > UINT_MAX)
            return 1;
        if (text[i].text == NULL)
            return 1;

        if (text[i].type == SPNG_TEXT) {
            if (ctx->strict && check_png_text(text[i].text, text[i].length))
                return 1;
        } else if (text[i].type == SPNG_ZTXT) {
            if (ctx->strict && check_png_text(text[i].text, text[i].length))
                return 1;

            if (text[i].compression_method != 0)
                return SPNG_EZTXT_COMPRESSION_METHOD;
        } else if (text[i].type == SPNG_ITXT) {
            if (text[i].compression_flag > 1)
                return SPNG_EITXT_COMPRESSION_FLAG;
            if (text[i].compression_method != 0)
                return SPNG_EITXT_COMPRESSION_METHOD;
            if (text[i].language_tag == NULL)
                return SPNG_EITXT_LANG_TAG;
            if (text[i].translated_keyword == NULL)
                return SPNG_EITXT_TRANSLATED_KEY;
        }
        else return 1;

    }

    struct spng_text2 *text_list = spng__calloc(ctx, sizeof(struct spng_text2), n_text);

    if (!text_list)
        return SPNG_EMEM;

    if (ctx->text_list != NULL) {
        for (i=0; i < ctx->n_text; i++) {
            if (ctx->user.text)
                break;

            spng__free(ctx, ctx->text_list[i].keyword);
            if (ctx->text_list[i].compression_flag)
                spng__free(ctx, ctx->text_list[i].text);
        }
        spng__free(ctx, ctx->text_list);
    }

    for (i=0; i < n_text; i++) {
        text_list[i].type = text[i].type;
        /* Prevent issues with spng_text.keyword[80] going out of scope */
        text_list[i].keyword = text_list[i].user_keyword_storage;
        memcpy(text_list[i].user_keyword_storage, text[i].keyword, strlen(text[i].keyword));
        text_list[i].text = text[i].text;
        text_list[i].text_length = text[i].length;

        if (text[i].type == SPNG_ZTXT) {
            text_list[i].compression_flag = 1;
        } else if (text[i].type == SPNG_ITXT) {
            text_list[i].compression_flag = text[i].compression_flag;
            text_list[i].language_tag = text[i].language_tag;
            text_list[i].translated_keyword = text[i].translated_keyword;
        }
    }

    ctx->text_list = text_list;
    ctx->n_text = n_text;

    ctx->stored.text = 1;
    ctx->user.text = 1;

    return 0;
}

int spng_set_bkgd(spng_ctx *ctx, struct spng_bkgd *bkgd)
{
    if ((ctx == NULL) || (bkgd == NULL))
        return 1;

    if (!ctx->stored.ihdr)
        return 1;

    if (ctx->ihdr.color_type == 0 || ctx->ihdr.color_type == 4) {
        ctx->bkgd.gray = bkgd->gray;
    } else if (ctx->ihdr.color_type == 2 || ctx->ihdr.color_type == 6) {
        ctx->bkgd.red = bkgd->red;
        ctx->bkgd.green = bkgd->green;
        ctx->bkgd.blue = bkgd->blue;
    } else if (ctx->ihdr.color_type == 3) {
        if (!ctx->stored.plte)
            return SPNG_EBKGD_NO_PLTE;
        if (bkgd->plte_index >= ctx->plte.n_entries)
            return SPNG_EBKGD_PLTE_IDX;

        ctx->bkgd.plte_index = bkgd->plte_index;
    }

    ctx->stored.bkgd = 1;
    ctx->user.bkgd = 1;

    return 0;
}

int spng_set_hist(spng_ctx *ctx, struct spng_hist *hist)
{
    if ((ctx == NULL) || (hist == NULL))
        return 1;

    if (!ctx->stored.plte)
        return SPNG_EHIST_NO_PLTE;

    ctx->hist = *hist;

    ctx->stored.hist = 1;
    ctx->user.hist = 1;

    return 0;
}

int spng_set_phys(spng_ctx *ctx, struct spng_phys *phys)
{
    if ((ctx == NULL) || (phys == NULL))
        return 1;

    if (check_phys(phys))
        return SPNG_EPHYS;

    ctx->phys = *phys;

    ctx->stored.phys = 1;
    ctx->user.phys = 1;

    return 0;
}

int spng_set_splt(spng_ctx *ctx, struct spng_splt *splt, uint32_t n_splt)
{
    if ((ctx == NULL) || (splt == NULL))
        return 1;
    if (!n_splt)
        return 1;

    uint32_t i;
    for (i=0; i < n_splt; i++) {
        if (check_png_keyword(splt[i].name))
            return SPNG_ESPLT_NAME;
        if (!(splt[i].sample_depth == 8 || splt[i].sample_depth == 16))
            return SPNG_ESPLT_DEPTH;
    }

    if (ctx->stored.splt && !ctx->user.splt) {
        for (i=0; i < ctx->n_splt; i++) {
            if (ctx->splt_list[i].entries != NULL)
                spng__free(ctx, ctx->splt_list[i].entries);
        }
        spng__free(ctx, ctx->splt_list);
    }

    ctx->splt_list = splt;
    ctx->n_splt = n_splt;

    ctx->stored.splt = 1;
    ctx->user.splt = 1;

    return 0;
}

int spng_set_time(spng_ctx *ctx, struct spng_time *time)
{
    if ((ctx == NULL) || (time == NULL))
        return 1;

    if (check_time(time))
        return SPNG_ETIME;

    ctx->time = *time;

    ctx->stored.time = 1;
    ctx->user.time = 1;

    return 0;
}

int spng_set_unknown_chunks(spng_ctx *ctx, struct spng_unknown_chunk *chunks, uint32_t n_chunks)
{
    if ((ctx == NULL) || (chunks == NULL))
        return 1;
    if (!n_chunks)
        return 1;

    uint32_t i;
    for (i=0; i < n_chunks; i++) {
        if (chunks[i].length > spng_u32max)
            return SPNG_ECHUNK_STDLEN;
        if (chunks[i].length && chunks[i].data == NULL)
            return 1;

        switch(chunks[i].location) {
        case SPNG_AFTER_IHDR:
        case SPNG_AFTER_PLTE:
        case SPNG_AFTER_IDAT:
        break;
        default: return SPNG_ECHUNK_POS;
        }
    }

    if (ctx->stored.unknown && !ctx->user.unknown) {
        for (i=0; i < ctx->n_chunks; i++) {
            spng__free(ctx, ctx->chunk_list[i].data);
        }
        spng__free(ctx, ctx->chunk_list);
    }

    ctx->chunk_list = chunks;
    ctx->n_chunks = n_chunks;

    ctx->stored.unknown = 1;
    ctx->user.unknown = 1;

    return 0;
}

int spng_set_offs(spng_ctx *ctx, struct spng_offs *offs)
{
    if ((ctx == NULL) || (offs == NULL))
        return 1;

    if (check_offs(offs))
        return SPNG_EOFFS;

    ctx->offs = *offs;

    ctx->stored.offs = 1;
    ctx->user.offs = 1;

    return 0;
}

int spng_set_exif(spng_ctx *ctx, struct spng_exif *exif)
{
    if ((ctx == NULL) || (exif == NULL))
        return 1;

    if (check_exif(exif))
        return SPNG_EEXIF;

    if (ctx->exif.data != NULL && !ctx->user.exif)
        spng__free(ctx, ctx->exif.data);

    ctx->exif = *exif;

    ctx->stored.exif = 1;
    ctx->user.exif = 1;

    return 0;
}

const char *spng_strerror(int err)
{
    switch(err) {
    case SPNG_IO_EOF:
        return "end of stream";
    case SPNG_IO_ERROR:
        return "stream error";
    case SPNG_OK:
        return "success";
    case SPNG_EINVAL:
        return "invalid argument";
    case SPNG_EMEM:
        return "out of memory";
    case SPNG_EOVERFLOW:
        return "arithmetic overflow";
    case SPNG_ESIGNATURE:
        return "invalid signature";
    case SPNG_EWIDTH:
        return "invalid image width";
    case SPNG_EHEIGHT:
        return "invalid image height";
    case SPNG_EUSER_WIDTH:
        return "image width exceeds user limit";
    case SPNG_EUSER_HEIGHT:
        return "image height exceeds user limit";
    case SPNG_EBIT_DEPTH:
        return "invalid bit depth";
    case SPNG_ECOLOR_TYPE:
        return "invalid color type";
    case SPNG_ECOMPRESSION_METHOD:
        return "invalid compression method";
    case SPNG_EFILTER_METHOD:
        return "invalid filter method";
    case SPNG_EINTERLACE_METHOD:
        return "invalid interlace method";
    case SPNG_EIHDR_SIZE:
        return "invalid IHDR chunk size";
    case SPNG_ENOIHDR:
        return "missing IHDR chunk";
    case SPNG_ECHUNK_POS:
        return "invalid chunk position";
    case SPNG_ECHUNK_SIZE:
        return "invalid chunk length";
    case SPNG_ECHUNK_CRC:
        return "invalid chunk checksum";
    case SPNG_ECHUNK_TYPE:
        return "invalid chunk type";
    case SPNG_ECHUNK_UNKNOWN_CRITICAL:
        return "unknown critical chunk";
    case SPNG_EDUP_PLTE:
        return "duplicate PLTE chunk";
    case SPNG_EDUP_CHRM:
        return "duplicate cHRM chunk";
    case SPNG_EDUP_GAMA:
        return "duplicate gAMA chunk";
    case SPNG_EDUP_ICCP:
        return "duplicate iCCP chunk";
    case SPNG_EDUP_SBIT:
        return "duplicate sBIT chunk";
    case SPNG_EDUP_SRGB:
        return "duplicate sRGB chunk";
    case SPNG_EDUP_BKGD:
        return "duplicate bKGD chunk";
    case SPNG_EDUP_HIST:
        return "duplicate hIST chunk";
    case SPNG_EDUP_TRNS:
        return "duplicate tRNS chunk";
    case SPNG_EDUP_PHYS:
        return "duplicate pHYs chunk";
    case SPNG_EDUP_TIME:
        return "duplicate tIME chunk";
    case SPNG_EDUP_OFFS:
        return "duplicate oFFs chunk";
    case SPNG_EDUP_EXIF:
        return "duplicate eXIf chunk";
    case SPNG_ECHRM:
        return "invalid cHRM chunk";
    case SPNG_EPLTE_IDX:
        return "invalid palette (PLTE) index";
    case SPNG_ETRNS_COLOR_TYPE:
        return "tRNS chunk with incompatible color type";
    case SPNG_ETRNS_NO_PLTE:
        return "missing palette (PLTE) for tRNS chunk";
    case SPNG_EGAMA:
        return "invalid gAMA chunk";
    case SPNG_EICCP_NAME:
        return "invalid iCCP profile name";
    case SPNG_EICCP_COMPRESSION_METHOD:
        return "invalid iCCP compression method";
    case SPNG_ESBIT:
        return "invalid sBIT chunk";
    case SPNG_ESRGB:
        return "invalid sRGB chunk";
    case SPNG_ETEXT:
        return "invalid tEXt chunk";
    case SPNG_ETEXT_KEYWORD:
        return "invalid tEXt keyword";
    case SPNG_EZTXT:
        return "invalid zTXt chunk";
    case SPNG_EZTXT_COMPRESSION_METHOD:
        return "invalid zTXt compression method";
    case SPNG_EITXT:
        return "invalid iTXt chunk";
    case SPNG_EITXT_COMPRESSION_FLAG:
        return "invalid iTXt compression flag";
    case SPNG_EITXT_COMPRESSION_METHOD:
        return "invalid iTXt compression method";
    case SPNG_EITXT_LANG_TAG:
        return "invalid iTXt language tag";
    case SPNG_EITXT_TRANSLATED_KEY:
        return "invalid iTXt translated key";
    case SPNG_EBKGD_NO_PLTE:
        return "missing palette for bKGD chunk";
    case SPNG_EBKGD_PLTE_IDX:
        return "invalid palette index for bKGD chunk";
    case SPNG_EHIST_NO_PLTE:
        return "missing palette for hIST chunk";
    case SPNG_EPHYS:
        return "invalid pHYs chunk";
    case SPNG_ESPLT_NAME:
        return "invalid suggested palette name";
    case SPNG_ESPLT_DUP_NAME:
        return "duplicate suggested palette (sPLT) name";
    case SPNG_ESPLT_DEPTH:
        return "invalid suggested palette (sPLT) sample depth";
    case SPNG_ETIME:
        return "invalid tIME chunk";
    case SPNG_EOFFS:
        return "invalid oFFs chunk";
    case SPNG_EEXIF:
        return "invalid eXIf chunk";
    case SPNG_EIDAT_TOO_SHORT:
        return "IDAT stream too short";
    case SPNG_EIDAT_STREAM:
        return "IDAT stream error";
    case SPNG_EZLIB:
        return "zlib error";
    case SPNG_EFILTER:
        return "invalid scanline filter";
    case SPNG_EBUFSIZ:
        return "invalid buffer size";
    case SPNG_EIO:
        return "i/o error";
    case SPNG_EOF:
        return "end of file";
    case SPNG_EBUF_SET:
        return "buffer already set";
    case SPNG_EBADSTATE:
        return "non-recoverable state";
    case SPNG_EFMT:
        return "invalid format";
    case SPNG_EFLAGS:
        return "invalid flags";
    case SPNG_ECHUNKAVAIL:
        return "chunk not available";
    case SPNG_ENCODE_ONLY:
        return "encode only context";
    case SPNG_EOI:
        return "reached end-of-image state";
    case SPNG_ENOPLTE:
        return "missing PLTE for indexed image";
    case SPNG_ECHUNK_LIMITS:
        return "reached chunk/cache limits";
    case SPNG_EZLIB_INIT:
        return "zlib init error";
    case SPNG_ECHUNK_STDLEN:
        return "chunk exceeds maximum standard length";
    case SPNG_EINTERNAL:
        return "internal error";
    case SPNG_ECTXTYPE:
        return "invalid operation for context type";
    case SPNG_ENOSRC:
        return "source PNG not set";
    case SPNG_ENODST:
        return "PNG output not set";
    case SPNG_EOPSTATE:
        return "invalid operation for state";
    case SPNG_ENOTFINAL:
        return "PNG not finalized";
    default: return "unknown error";
    }
}

const char *spng_version_string(void)
{
    return SPNG_VERSION_STRING;
}
