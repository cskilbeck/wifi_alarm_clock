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

#define SECTION_HEIGHT 16    // largest power of 2 which is divisible into 240
#define NUM_SECTIONS (LCD_HEIGHT / SECTION_HEIGHT)
#define BITS_PER_PIXEL 16
#define BYTES_PER_LINE (LCD_WIDTH * BITS_PER_PIXEL / 8)

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

    uint32_t display_buffer[LCD_WIDTH * SECTION_HEIGHT];

    // uint8_t dma_buffer[2][LCD_WIDTH * BYTES_PER_LINE * SECTION_HEIGHT];    // 18 bpp

    // int dma_buffer_id = 0;

    uint8_t display_list_buffer[16384];

    uint16_t display_list_used;

    struct display_list display_lists[NUM_SECTIONS];

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
            uint32_t r = (min(255lu, get_r(src) + get_r(dst)) * alpha) >> 8;
            uint32_t g = (min(255lu, get_g(src) + get_g(dst)) * alpha) >> 8;
            uint32_t b = (min(255lu, get_b(src) + get_b(dst)) * alpha) >> 8;
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
        uint32_t *dst = display_buffer + e.pos.x + e.pos.y * LCD_WIDTH;
        uint32_t stride = source_image->width;
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

    uint32_t constexpr RED6_MASK = 0xfc0000;
    uint32_t constexpr GRN6_MASK = 0x00fc00;
    uint32_t constexpr BLU6_MASK = 0x0000fc;

    //////////////////////////////////////////////////////////////////////

    uint64_t rgb24_to_18(uint32_t x)
    {
        return ((x & RED6_MASK) >> 6) | ((x & GRN6_MASK) >> 4) | ((x & BLU6_MASK) >> 2);
    }

    //////////////////////////////////////////////////////////////////////

    void emit(uint64_t &v, uint32_t *p)
    {
        *p = v >> 32;
        v <<= 32;
    }

    //////////////////////////////////////////////////////////////////////

    void convert_24_to_18_line(uint32_t *src, uint32_t *dst)
    {
        for(int i = 0; i < 240; i += 16) {

            uint64_t r = 0;

            r |= rgb24_to_18(*src++) << 46;
            r |= rgb24_to_18(*src++) << 28;
            emit(r, dst++);
            r |= rgb24_to_18(*src++) << 42;
            r |= rgb24_to_18(*src++) << 24;
            emit(r, dst++);
            r |= rgb24_to_18(*src++) << 38;
            r |= rgb24_to_18(*src++) << 20;
            emit(r, dst++);
            r |= rgb24_to_18(*src++) << 34;
            r |= rgb24_to_18(*src++) << 16;
            emit(r, dst++);
            r |= rgb24_to_18(*src++) << 30;
            emit(r, dst++);
            r |= rgb24_to_18(*src++) << 44;
            r |= rgb24_to_18(*src++) << 26;
            emit(r, dst++);
            r |= rgb24_to_18(*src++) << 40;
            r |= rgb24_to_18(*src++) << 22;
            emit(r, dst++);
            r |= rgb24_to_18(*src++) << 36;
            r |= rgb24_to_18(*src++) << 18;
            emit(r, dst++);
            r |= rgb24_to_18(*src++) << 32;
            emit(r, dst++);
        }
    }

}    // namespace

//////////////////////////////////////////////////////////////////////

void display_reset()
{
    ESP_LOGI(TAG, "reset");

    display_list_used = 0;
    for(display_list &d : display_lists) {
        d.head = &d.root;
    }
}

//////////////////////////////////////////////////////////////////////

void display_imagerect(vec2i const *dst_pos, vec2i const *src_pos, vec2i const *size, uint8_t image_id, uint8_t alpha, uint8_t blendmode)
{
    int top_section = dst_pos->y / SECTION_HEIGHT;
    int section_top_y = top_section * SECTION_HEIGHT;

    int src_y = src_pos->y;
    int dst_y = dst_pos->y - section_top_y;

    int remaining_height = size->y;
    int cur_height = min(remaining_height, section_top_y + SECTION_HEIGHT - dst_pos->y);

    display_list *d = display_lists + top_section;

    do {

        cur_height = min(remaining_height, SECTION_HEIGHT);

        display_list_entry *e = alloc_display_list_entry(d);
        e->pos = vec2b{ (uint8_t)dst_pos->x, (uint8_t)dst_y };
        e->size = vec2b{ (uint8_t)size->x, (uint8_t)cur_height };
        e->blit.image_id = image_id;
        e->blit.src_pos = vec2b{ (uint8_t)src_pos->x, (uint8_t)src_y };
        e->blit.alpha = alpha;
        e->flags = blendmode | 0x80;
        e->display_list_next = 0xffff;
        src_y += cur_height;
        remaining_height -= cur_height;
        dst_y = 0;
        d += 1;

    } while(remaining_height > 0);
}

//////////////////////////////////////////////////////////////////////

void display_fillrect(vec2i const *dst_pos, vec2i const *size, uint32_t color, uint8_t blendmode)
{
    int top_section = dst_pos->y / SECTION_HEIGHT;
    int section_top_y = top_section * SECTION_HEIGHT;

    int remaining_height = size->y;
    int cur_height = min(remaining_height, section_top_y + SECTION_HEIGHT - dst_pos->y);

    display_list *d = display_lists + top_section;

    int dst_y = dst_pos->y - section_top_y;

    do {

        cur_height = min(remaining_height, SECTION_HEIGHT);

        display_list_entry *e = alloc_display_list_entry(d);
        e->pos = vec2b{ (uint8_t)dst_pos->x, (uint8_t)dst_y };
        e->size = vec2b{ (uint8_t)size->x, (uint8_t)cur_height };
        e->color = color;
        e->flags = blendmode;

        remaining_height -= cur_height;
        dst_y = 0;
        d += 1;

    } while(remaining_height > 0);
}

//////////////////////////////////////////////////////////////////////
// assumes buffer is 240x16 18bpp
// which means 135 uint32 per line

void display_list_draw(display_list *l, uint32_t *buffer)
{
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
    // now convert display_buffer from ARGB32 to 18 bpp
    uint32_t *src_row = display_buffer;
    uint32_t *dst_row = buffer;
    for(int y = 0; y < SECTION_HEIGHT; ++y) {
        convert_24_to_18_line(src_row, dst_row);
        src_row += LCD_WIDTH;
        dst_row += 135;
    }
}
