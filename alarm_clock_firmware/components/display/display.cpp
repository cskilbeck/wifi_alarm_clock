//////////////////////////////////////////////////////////////////////
// there is one display list per screen section
// each screen section is, say, 16 (32?) lines high
// for a blit or fill, we add to all the display lists which the rectangle intersects with

// two 18 (16?) bit buffers for dma
// one 32 (24?) bit buffer for drawing into

// for each display_list
// 1. draw into the 32 (24?) bit buffer using the display_list_entries
// 2. convert to 18 (16?) bit into buffer A (while buffer B is being DMA'ed)
// 3. wait for buffer B DMA to complete
// 4. kick off buffer A DMA
// 5. swap buffer A/B

// to add a blit
// for each display list which the blit intersects
//    add a cropped display list entry

#include <memory.h>
#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include <stdint.h>
#include <stdio.h>
#include "util.h"
#include "image.h"
#include "lcd_gc9a01.h"
#include "display.h"

static const char *TAG = "display";

//////////////////////////////////////////////////////////////////////


namespace
{
    struct blit_entry
    {
        vec2b src_pos;
        uint8_t image_id;
        uint8_t alpha;
    };

    //////////////////////////////////////////////////////////////////////

    struct display_list_entry
    {
        vec2b pos;
        vec2b size;

        uint16_t flags;
        uint16_t display_list_next;

        union
        {
            blit_entry blit;
            uint32_t color;
        };
    };

}    // namespace

//////////////////////////////////////////////////////////////////////

struct display_list
{
    struct display_list_entry *head;    // most recently added entry
    struct display_list_entry root;     // dummy head entry
};

//////////////////////////////////////////////////////////////////////

namespace
{
    //////////////////////////////////////////////////////////////////////

    uint32_t display_buffer[LCD_WIDTH * LCD_SECTION_HEIGHT];

    uint8_t display_list_buffer[16384];

    uint16_t display_list_used = 0;

    struct display_list display_lists[LCD_NUM_SECTIONS];

    //////////////////////////////////////////////////////////////////////

    inline display_list_entry const &get_display_list_entry(uint16_t offset)
    {
        return *reinterpret_cast<display_list_entry *>(display_list_buffer + offset);
    }

    //////////////////////////////////////////////////////////////////////

    inline display_list_entry *alloc_display_list_entry(display_list *list)
    {
        assert(display_list_used < (sizeof(display_list_buffer) - sizeof(display_list_entry)));

        list->head->display_list_next = display_list_used;
        display_list_entry *entry = (display_list_entry *)(display_list_buffer + display_list_used);
        display_list_used += sizeof(display_list_entry);
        list->head = entry;
        return entry;
    }

    //////////////////////////////////////////////////////////////////////

    uint32_t get_a(uint32_t x)
    {
        return (x >> 24) & 0xff;
    }

    //////////////////////////////////////////////////////////////////////

    uint32_t get_r(uint32_t x)
    {
        return (x >> 16) & 0xff;
    }

    //////////////////////////////////////////////////////////////////////

    uint32_t get_g(uint32_t x)
    {
        return (x >> 8) & 0xff;
    }

    //////////////////////////////////////////////////////////////////////

    uint32_t get_b(uint32_t x)
    {
        return x & 0xff;
    }

    //////////////////////////////////////////////////////////////////////

    struct do_blend_opaque
    {
        static uint32_t blend(uint32_t src, uint32_t dst, uint8_t alpha)
        {
            return src;
        }
    };

    //////////////////////////////////////////////////////////////////////

    struct do_blend_add
    {
        static uint32_t blend(uint32_t src, uint32_t dst, uint8_t alpha)
        {
            uint32_t sa = (get_a(src) * alpha) >> 8;
            uint32_t r = min(255lu, get_r(dst) + ((get_r(src) * sa) >> 8));
            uint32_t g = min(255lu, get_g(dst) + ((get_g(src) * sa) >> 8));
            uint32_t b = min(255lu, get_b(dst) + ((get_b(src) * sa) >> 8));
            return (r << 16) | (g << 8) | b;
        }
    };

    //////////////////////////////////////////////////////////////////////

    struct do_blend_multiply
    {
        static uint32_t blend(uint32_t src, uint32_t dst, uint8_t alpha)
        {
            uint32_t sa = (get_a(src) * alpha) >> 8;
            uint32_t da = 255 - sa;
            uint32_t r = ((get_r(src) * sa) >> 8) + ((get_r(dst) * da) >> 8);
            uint32_t g = ((get_g(src) * sa) >> 8) + ((get_g(dst) * da) >> 8);
            uint32_t b = ((get_b(src) * sa) >> 8) + ((get_b(dst) * da) >> 8);
            return (r << 16) | (g << 8) | b;
        }
    };

    //////////////////////////////////////////////////////////////////////

    template <typename T> void do_blit(display_list_entry const &e)
    {
        image_t const *source_image = image_get(e.blit.image_id);

        uint32_t stride = source_image->width;

        uint32_t *dst = display_buffer + e.pos.x + e.pos.y * LCD_WIDTH;
        uint32_t *src = (uint32_t *)(source_image->pixel_data + e.blit.src_pos.x + e.blit.src_pos.y * stride);

        for(int y = 0; y < e.size.y; ++y) {
            uint32_t *dst_row = dst;
            uint32_t *src_row = src;
            for(int x = 0; x < e.size.x; ++x) {
                *dst_row = T::blend(*src_row, *dst_row, e.blit.alpha);
                src_row += 1;
                dst_row += 1;
            }
            src += stride;
            dst += LCD_WIDTH;
        }
    }

    //////////////////////////////////////////////////////////////////////

    template <typename T> void do_fill(display_list_entry const &e)
    {
        uint32_t *dst = display_buffer + e.pos.x + e.pos.y * LCD_WIDTH;
        uint32_t color = e.color;
        uint8_t alpha = get_a(color);
        for(int y = 0; y < e.size.y; ++y) {
            uint32_t *dst_row = dst;
            for(int x = 0; x < e.size.x; ++x) {
                *dst_row = T::blend(color, *dst_row, alpha);
                dst_row += 1;
            }
            dst += LCD_WIDTH;
        }
    }

    //////////////////////////////////////////////////////////////////////

    uint32_t constexpr RED5_MASK = 0xf80000;
    uint32_t constexpr RED6_MASK = 0xfc0000;

    uint32_t constexpr GRN6_MASK = 0x00fc00;

    uint32_t constexpr BLU5_MASK = 0x0000f8;
    uint32_t constexpr BLU6_MASK = 0x0000fc;

#if LCD_BITS_PER_PIXEL == 18

    //////////////////////////////////////////////////////////////////////

    uint64_t rgb24_to_18(uint32_t x)
    {
        return ((x & RED6_MASK) >> 6) | ((x & GRN6_MASK) >> 4) | ((x & BLU6_MASK) >> 2);
    }

    //////////////////////////////////////////////////////////////////////

    void emit(uint64_t &v, uint32_t *p)
    {
        uint32_t t = static_cast<uint32_t>(v >> 32);
        //*p = __builtin_bswap32(t);
        *p = t;
        //*p = 0b11111100000011111100000011111100;
        v <<= 32;
    }

    //////////////////////////////////////////////////////////////////////

    void convert_24_to_line(uint32_t *src, uint8_t *dst)
    {
        uint32_t *d = reinterpret_cast<uint32_t *>(dst);

        for(int i = 0; i < 240; i += 16) {

            uint64_t r = 0;

            r |= rgb24_to_18(*src++) << 46;
            r |= rgb24_to_18(*src++) << 28;
            emit(r, d++);
            r |= rgb24_to_18(*src++) << 42;
            r |= rgb24_to_18(*src++) << 24;
            emit(r, d++);
            r |= rgb24_to_18(*src++) << 38;
            r |= rgb24_to_18(*src++) << 20;
            emit(r, d++);
            r |= rgb24_to_18(*src++) << 34;
            r |= rgb24_to_18(*src++) << 16;
            emit(r, d++);
            r |= rgb24_to_18(*src++) << 30;
            emit(r, d++);
            r |= rgb24_to_18(*src++) << 44;
            r |= rgb24_to_18(*src++) << 26;
            emit(r, d++);
            r |= rgb24_to_18(*src++) << 40;
            r |= rgb24_to_18(*src++) << 22;
            emit(r, d++);
            r |= rgb24_to_18(*src++) << 36;
            r |= rgb24_to_18(*src++) << 18;
            emit(r, d++);
            r |= rgb24_to_18(*src++) << 32;
            emit(r, d++);
        }
    }

#else

    //////////////////////////////////////////////////////////////////////

    uint16_t rgb24_to_16(uint32_t x)
    {
        uint16_t y = ((x & RED5_MASK) >> 8) | ((x & GRN6_MASK) >> 5) | ((x & BLU5_MASK) >> 3);
        return __builtin_bswap16(y);
    }

    //////////////////////////////////////////////////////////////////////

    void convert_24_to_line(uint32_t *src, uint8_t *dst)
    {
        uint16_t *d = reinterpret_cast<uint16_t *>(dst);

        for(int i = 0; i < 240; i += 4) {

            *d++ = rgb24_to_16(*src++);
            *d++ = rgb24_to_16(*src++);
            *d++ = rgb24_to_16(*src++);
            *d++ = rgb24_to_16(*src++);
        }
    }

#endif

}    // namespace

//////////////////////////////////////////////////////////////////////

void display_reset()
{
    ESP_LOGI(TAG, "reset");

    display_list_used = 0;
    for(display_list &d : display_lists) {
        d.head = &d.root;
        d.head->flags = 0;
        d.head->display_list_next = 0xffff;
    }
}

//////////////////////////////////////////////////////////////////////

void display_imagerect(vec2i const *dst_pos, vec2i const *src_pos, vec2i const *size, uint8_t image_id, uint8_t alpha, uint8_t blendmode)
{
    // clip
    vec2i sz = *size;
    vec2i s = *src_pos;
    vec2i d = *dst_pos;
    if(d.x < 0) {
        sz.x += d.x;
        s.x -= d.x;
        d.x = 0;
    }
    if(sz.x <= 0) {
        return;
    }
    if(d.y < 0) {
        sz.y += d.y;
        s.y -= d.y;
        d.y = 0;
    }
    if(sz.y <= 0) {
        return;
    }
    int right = (d.x + sz.x) - LCD_WIDTH;
    if(right > 0) {
        sz.x -= right;
        if(sz.x <= 0) {
            return;
        }
    }
    int bottom = (d.y + sz.y) - LCD_HEIGHT;
    if(bottom > 0) {
        sz.y -= bottom;
        if(sz.y <= 0) {
            return;
        }
    }

    int top_section = d.y / LCD_SECTION_HEIGHT;
    int section_top_y = top_section * LCD_SECTION_HEIGHT;

    int src_y = s.y;
    int dst_y = d.y - section_top_y;

    int remaining_height = sz.y;
    int cur_height = min(remaining_height, section_top_y + LCD_SECTION_HEIGHT - d.y);

    display_list *dsp_list = display_lists + top_section;

    do {

        cur_height = min(remaining_height, LCD_SECTION_HEIGHT);

        int overflow = LCD_SECTION_HEIGHT - (dst_y + cur_height);

        cur_height += min(0, overflow);

        display_list_entry *e = alloc_display_list_entry(dsp_list);

        e->pos = vec2b{ (uint8_t)d.x, (uint8_t)dst_y };
        e->size = vec2b{ (uint8_t)sz.x, (uint8_t)cur_height };
        e->blit.image_id = image_id;
        e->blit.src_pos = vec2b{ (uint8_t)s.x, (uint8_t)src_y };
        e->blit.alpha = alpha;
        e->flags = blendmode | 0x80;
        e->display_list_next = 0xffff;
        src_y += cur_height;
        remaining_height -= cur_height;
        dst_y = 0;
        dsp_list += 1;

    } while(remaining_height > 0);
}

//////////////////////////////////////////////////////////////////////

void display_fillrect(vec2i const *dst_pos, vec2i const *size, uint32_t color, uint8_t blendmode)
{
    // clip
    vec2i sz = *size;
    vec2i d = *dst_pos;
    if(d.x < 0) {
        sz.x += d.x;
        d.x = 0;
    }
    if(sz.x <= 0) {
        return;
    }
    if(d.y < 0) {
        sz.y += d.y;
        d.y = 0;
    }
    if(sz.y <= 0) {
        return;
    }
    int right = (d.x + sz.x) - LCD_WIDTH;
    if(right > 0) {
        sz.x -= right;
        if(sz.x <= 0) {
            return;
        }
    }
    int bottom = (d.y + sz.y) - LCD_HEIGHT;
    if(bottom > 0) {
        sz.y -= bottom;
        if(sz.y <= 0) {
            return;
        }
    }

    int top_section = dst_pos->y / LCD_SECTION_HEIGHT;
    int section_top_y = top_section * LCD_SECTION_HEIGHT;

    int remaining_height = size->y;
    int cur_height = min(remaining_height, section_top_y + LCD_SECTION_HEIGHT - dst_pos->y);

    display_list *dsp_list = display_lists + top_section;

    int dst_y = dst_pos->y - section_top_y;

    do {
        cur_height = min(remaining_height, LCD_SECTION_HEIGHT);

        int overflow = LCD_SECTION_HEIGHT - (dst_y + cur_height);

        cur_height += min(0, overflow);

        display_list_entry *e = alloc_display_list_entry(dsp_list);
        e->pos = vec2b{ (uint8_t)dst_pos->x, (uint8_t)dst_y };
        e->size = vec2b{ (uint8_t)size->x, (uint8_t)cur_height };
        e->color = color;
        e->flags = blendmode;

        remaining_height -= cur_height;
        dst_y = 0;
        dsp_list += 1;

    } while(remaining_height > 0);
}

//////////////////////////////////////////////////////////////////////
// assumes buffer is 240x16 18bpp
// which means 135 uint32 per line

void display_list_draw(int section, uint8_t *buffer)
{
    display_list *l = display_lists + section;

    uint16_t offset = l->root.display_list_next;

    while(offset != 0xffff) {

        display_list_entry const &e = get_display_list_entry(offset);

        if(e.flags & 0x80) {

            switch(e.flags & 3) {
            case blend_opaque:
                do_blit<do_blend_opaque>(e);
                break;
            case blend_add:
                do_blit<do_blend_add>(e);
                break;
            case blend_multiply:
                do_blit<do_blend_multiply>(e);
                break;
            default:
                break;
            }

        } else {
            switch(e.flags & 3) {
            case blend_opaque:
                do_fill<do_blend_opaque>(e);
                break;
            case blend_add:
                do_fill<do_blend_add>(e);
                break;
            case blend_multiply:
                do_fill<do_blend_multiply>(e);
                break;
            default:
                break;
            }
        }
        offset = e.display_list_next;
    }

    // convert display_buffer from ARGB32 to 16 (18?) bpp

    // TODO (chs): double buffer and do this on the other core

    uint32_t *src_row = display_buffer;
    uint8_t *dst_row = buffer;
    for(int y = 0; y < LCD_SECTION_HEIGHT; ++y) {
        convert_24_to_line(src_row, dst_row);
        src_row += LCD_WIDTH;
        dst_row += LCD_BYTES_PER_LINE;
    }
}
