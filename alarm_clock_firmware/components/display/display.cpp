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

LOG_CONTEXT("display");

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

    struct display_list_node
    {
        uint16_t next;
        uint16_t flags;
    };

    //////////////////////////////////////////////////////////////////////

    struct display_list_t
    {
        display_list_node *head;
        display_list_node root;
    };

    //////////////////////////////////////////////////////////////////////

    struct display_list_entry
    {
        display_list_node node;

        vec2b pos;
        vec2b size;

        union
        {
            blit_entry blit;
            uint32_t color;
        };
    };

}    // namespace

//////////////////////////////////////////////////////////////////////

namespace
{
    //////////////////////////////////////////////////////////////////////

#if LCD_BITS_PER_PIXEL == 16
    uint8_t DMA_ATTR display_buffer[LCD_WIDTH * 3 * LCD_SECTION_HEIGHT];
#endif

    uint8_t DMA_ATTR display_list_buffer[32768];

    size_t display_list_used = 0;

    // dummy root node for each section
    display_list_t display_lists[LCD_NUM_SECTIONS];

    //////////////////////////////////////////////////////////////////////

    inline display_list_entry const &get_display_list_entry(uint16_t offset)
    {
        assert(offset <= (sizeof(display_list_buffer) - sizeof(display_list_entry)));

        return *reinterpret_cast<display_list_entry *>(display_list_buffer + offset);
    }

    //////////////////////////////////////////////////////////////////////

    inline display_list_entry *alloc_display_list_entry(display_list_t *display_list)
    {
        assert(display_list_used <= (sizeof(display_list_buffer) - sizeof(display_list_entry)));

        display_list->head->next = display_list_used;

        display_list_entry *entry = reinterpret_cast<display_list_entry *>(display_list_buffer + display_list_used);

        entry->node.next = 0xffff;

        display_list_used += sizeof(display_list_entry);

        display_list->head = &entry->node;

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
        static void blend(uint8_t *dst, uint32_t src, uint8_t alpha)
        {
            dst[0] = get_r(src);
            dst[1] = get_g(src);
            dst[2] = get_b(src);
        }
    };

    //////////////////////////////////////////////////////////////////////

    struct do_blend_add
    {
        static void blend(uint8_t *dst, uint32_t src, uint8_t alpha)
        {
            uint32_t sa = (get_a(src) * alpha) >> 8;
            dst[0] = min(255lu, dst[0] + (get_r(src) * sa >> 8));
            dst[1] = min(255lu, dst[1] + (get_g(src) * sa >> 8));
            dst[2] = min(255lu, dst[2] + (get_b(src) * sa >> 8));
        }
    };

    //////////////////////////////////////////////////////////////////////

    struct do_blend_multiply
    {
        static void blend(uint8_t *dst, uint32_t src, uint8_t alpha)
        {
            uint32_t sa = (get_a(src) * alpha) >> 8;
            uint32_t da = 255 - sa;
            dst[0] = ((get_r(src) * sa) >> 8) + ((dst[0] * da) >> 8);
            dst[1] = ((get_g(src) * sa) >> 8) + ((dst[1] * da) >> 8);
            dst[2] = ((get_b(src) * sa) >> 8) + ((dst[2] * da) >> 8);
        }
    };

    //////////////////////////////////////////////////////////////////////

    template <typename T> void do_blit(display_list_entry const &e, uint8_t *buffer)
    {
        image_t const *source_image = image_get(e.blit.image_id);

        uint32_t stride = source_image->width;

        uint8_t *dst = buffer + (e.pos.x + e.pos.y * LCD_WIDTH) * 3;
        uint32_t *src = (uint32_t *)(source_image->pixel_data + e.blit.src_pos.x + e.blit.src_pos.y * stride);

        uint8_t alpha = e.blit.alpha;

        for(int y = 0; y < e.size.y; ++y) {

            uint8_t *dst_row = dst;
            uint32_t *src_row = src;

            for(int x = e.size.x; x != 0; --x) {
                T::blend(dst_row, *src_row++, alpha);
                dst_row += 3;
            }

            src += stride;
            dst += LCD_WIDTH * 3;
        }
    }

    //////////////////////////////////////////////////////////////////////

    template <typename T> void do_fill(display_list_entry const &e, uint8_t *buffer)
    {
        uint8_t *dst = buffer + (e.pos.x + e.pos.y * LCD_WIDTH) * 3;
        uint8_t alpha = get_a(e.color);
        uint32_t color = e.color;

        for(int y = 0; y < e.size.y; ++y) {

            uint8_t *dst_row = dst;

            for(int x = e.size.x; x != 0; --x) {
                T::blend(dst_row, color, alpha);
                dst_row += 3;
            }
            dst += LCD_WIDTH * 3;
        }
    }

#if LCD_BITS_PER_PIXEL == 16

    //////////////////////////////////////////////////////////////////////
    // convert display_buffer from ARGB32 to 16 bpp

    void convert_display_buffer(uint8_t *dst)
    {
        uint8_t *src = display_buffer;

        for(int y = 0; y < LCD_SECTION_HEIGHT; ++y) {

            uint8_t *src_row = src;
            uint16_t *dst_row = reinterpret_cast<uint16_t *>(dst);

            for(int i = 0; i < LCD_WIDTH; i++) {

                uint32_t r = *src_row++ >> 3 << 11;
                uint32_t g = *src_row++ >> 2 << 5;
                uint32_t b = *src_row++ >> 3;

                *dst_row++ = __builtin_bswap16(r | g | b);
            }
            src += LCD_WIDTH * 3;
            dst += LCD_BYTES_PER_LINE;
        }
    }

#endif

}    // namespace

//////////////////////////////////////////////////////////////////////

void display_begin_frame()
{
    // allocate one dummy head node for each list
    // we can use 0 as a sentinel because the
    // entry at location 0 is guaranteed to be a dummy

    display_list_used = 0;

    for(display_list_t &d : display_lists) {

        d.root.next = 0xffff;
        d.head = &d.root;
    }
}

//////////////////////////////////////////////////////////////////////

void display_image(vec2i *pos, uint8_t image_id, uint8_t alpha, uint8_t blendmode, vec2f *pivot)
{
    image_t const *image = image_get(image_id);
    vec2i src_pos = { 0, 0 };
    vec2i size = { image->width, image->height };
    vec2i dst_pos = { pos->x - (int)(size.x * pivot->x), pos->y - (int)(size.y * pivot->y) };
    display_imagerect(&dst_pos, &src_pos, &size, image_id, alpha, blendmode);
}

//////////////////////////////////////////////////////////////////////

void display_imagerect(vec2i const *dst_pos, vec2i const *src_pos, vec2i const *size, uint8_t image_id, uint8_t alpha, uint8_t blendmode)
{
    // clip
    vec2i sz = *size;
    vec2i src = *src_pos;
    vec2i dst = *dst_pos;
    if(dst.x < 0) {
        sz.x += dst.x;
        src.x -= dst.x;
        dst.x = 0;
    }
    if(sz.x <= 0) {
        return;
    }
    if(dst.y < 0) {
        sz.y += dst.y;
        src.y -= dst.y;
        dst.y = 0;
    }
    if(sz.y <= 0) {
        return;
    }
    int right_clip = (dst.x + sz.x) - LCD_WIDTH;
    if(right_clip > 0) {
        sz.x -= right_clip;
        if(sz.x <= 0) {
            return;
        }
    }
    int bottom_clip = (dst.y + sz.y) - LCD_HEIGHT;
    if(bottom_clip > 0) {
        sz.y -= bottom_clip;
        if(sz.y <= 0) {
            return;
        }
    }

    int top_section = dst.y / LCD_SECTION_HEIGHT;
    int section_top_y = top_section * LCD_SECTION_HEIGHT;

    int src_y = src.y;
    int dst_y = dst.y - section_top_y;

    int remaining_height = sz.y;
    int cur_height = min(remaining_height, section_top_y + LCD_SECTION_HEIGHT - dst.y);

    display_list_t *display_list = display_lists + top_section;

    do {

        cur_height = min(remaining_height, LCD_SECTION_HEIGHT);

        int overflow = LCD_SECTION_HEIGHT - (dst_y + cur_height);

        if(overflow < 0) {
            cur_height += overflow;
        }

        display_list_entry *e = alloc_display_list_entry(display_list);

        e->pos = vec2b{ (uint8_t)dst.x, (uint8_t)dst_y };
        e->size = vec2b{ (uint8_t)sz.x, (uint8_t)cur_height };
        e->blit.image_id = image_id;
        e->blit.src_pos = vec2b{ (uint8_t)src.x, (uint8_t)src_y };
        e->blit.alpha = alpha;
        e->node.flags = blendmode | 0x8000;

        src_y += cur_height;
        remaining_height -= cur_height;
        dst_y = 0;
        display_list += 1;

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
    int right_clip = (d.x + sz.x) - LCD_WIDTH;
    if(right_clip > 0) {
        sz.x -= right_clip;
        if(sz.x <= 0) {
            return;
        }
    }
    int bottom_clip = (d.y + sz.y) - LCD_HEIGHT;
    if(bottom_clip > 0) {
        sz.y -= bottom_clip;
        if(sz.y <= 0) {
            return;
        }
    }

    int top_section = dst_pos->y / LCD_SECTION_HEIGHT;
    int section_top_y = top_section * LCD_SECTION_HEIGHT;

    int remaining_height = size->y;
    int cur_height = min(remaining_height, section_top_y + LCD_SECTION_HEIGHT - dst_pos->y);

    display_list_t *display_list = display_lists + top_section;

    int dst_y = dst_pos->y - section_top_y;

    do {
        cur_height = min(remaining_height, LCD_SECTION_HEIGHT);

        int overflow = LCD_SECTION_HEIGHT - (dst_y + cur_height);

        cur_height += min(0, overflow);

        display_list_entry *e = alloc_display_list_entry(display_list);
        e->pos = vec2b{ (uint8_t)dst_pos->x, (uint8_t)dst_y };
        e->size = vec2b{ (uint8_t)size->x, (uint8_t)cur_height };
        e->color = color;
        e->node.flags = blendmode;

        remaining_height -= cur_height;
        dst_y = 0;
        display_list += 1;

    } while(remaining_height > 0);
}

//////////////////////////////////////////////////////////////////////
// assumes buffer is 240x16 18bpp
// which means 135 uint32 per line

void display_list_draw(int section, uint8_t *buffer)
{
#if LCD_BITS_PER_PIXEL == 16
    uint8_t *draw_buffer = display_buffer;
#else
    uint8_t *draw_buffer = buffer;
#endif

    uint16_t offset = display_lists[section].root.next;

    while(offset != 0xffff) {

        display_list_entry const &e = get_display_list_entry(offset);

        if(e.node.flags & 0x8000) {

            switch(e.node.flags & 3) {
            case blend_opaque:
                do_blit<do_blend_opaque>(e, draw_buffer);
                break;
            case blend_add:
                do_blit<do_blend_add>(e, draw_buffer);
                break;
            case blend_multiply:
                do_blit<do_blend_multiply>(e, draw_buffer);
                break;
            default:
                break;
            }

        } else {
            switch(e.node.flags & 3) {
            case blend_opaque:
                do_fill<do_blend_opaque>(e, draw_buffer);
                break;
            case blend_add:
                do_fill<do_blend_add>(e, draw_buffer);
                break;
            case blend_multiply:
                do_fill<do_blend_multiply>(e, draw_buffer);
                break;
            default:
                break;
            }
        }
        offset = e.node.next;
    }

#if LCD_BITS_PER_PIXEL == 16

    convert_display_buffer(buffer);

#endif
}

//////////////////////////////////////////////////////////////////////

void display_end_frame()
{
    lcd_update(display_list_draw);
}

//////////////////////////////////////////////////////////////////////

void display_init()
{
    lcd_init();
}
