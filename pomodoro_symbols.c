/*******************************************************************************
 * Size: 10 px
 * Bpp: 4
 * Opts: --bpp 4 --size 10 --no-compress --font fontawesome-webfont.woff --range 61713,61708 --format lvgl -o pomodoro_symbols.c
 ******************************************************************************/

#include "lvgl.h"

#ifndef POMODORO_SYMBOLS
#define POMODORO_SYMBOLS 1
#endif

#if POMODORO_SYMBOLS

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+F10C "" */
    0x0, 0x6c, 0xd9, 0x20, 0x0, 0xce, 0x86, 0xbf,
    0x40, 0x7e, 0x10, 0x0, 0x8e, 0xd, 0x60, 0x0,
    0x0, 0xe5, 0xf4, 0x0, 0x0, 0xc, 0x7d, 0x60,
    0x0, 0x0, 0xe5, 0x7e, 0x10, 0x0, 0x8e, 0x0,
    0xce, 0x86, 0xbf, 0x40, 0x0, 0x7c, 0xda, 0x20,
    0x0,

    /* U+F111 "" */
    0x0, 0x6c, 0xda, 0x30, 0x0, 0xcf, 0xff, 0xff,
    0x50, 0x8f, 0xff, 0xff, 0xff, 0x1d, 0xff, 0xff,
    0xff, 0xf7, 0xff, 0xff, 0xff, 0xff, 0x8d, 0xff,
    0xff, 0xff, 0xf6, 0x6f, 0xff, 0xff, 0xfe, 0x10,
    0xaf, 0xff, 0xff, 0x40, 0x0, 0x49, 0xa8, 0x10,
    0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 137, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 41, .adv_w = 137, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_0[] = {
    0x0, 0x5
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 61708, .range_length = 6, .glyph_id_start = 1,
        .unicode_list = unicode_list_0, .glyph_id_ofs_list = NULL, .list_length = 2, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 4,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t pomodoro_symbols = {
#else
lv_font_t pomodoro_symbols = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 9,          /*The maximum line height required by the font*/
    .base_line = 1,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = 0,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if POMODORO_SYMBOLS*/

