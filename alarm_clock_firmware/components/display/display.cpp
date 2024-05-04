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
#include "lcd_gc9a01.h"
#include "display.h"

static const char *TAG = "display";

//////////////////////////////////////////////////////////////////////

#define SECTION_HEIGHT 16    // largest power of 2 which is divisible into 240
#define NUM_SECTIONS (LCD_HEIGHT / SECTION_HEIGHT)
#define BITS_PER_PIXEL 16
#define BYTES_PER_LINE (LCD_WIDTH * BITS_PER_PIXEL / 8)

//////////////////////////////////////////////////////////////////////

struct blit_entry
{
    uint8_t image_id;
    uint8_t src_x;
    uint8_t src_y;
    uint8_t pad;
};

//////////////////////////////////////////////////////////////////////

enum display_blendmode
{
    blend_opaque = 0,
    blend_additive = 1,
    blend_multiply = 2
};

//////////////////////////////////////////////////////////////////////

struct display_list_entry
{
    uint8_t x;
    uint8_t y;
    uint8_t w;
    uint8_t h;

    uint16_t flags;
    uint16_t display_list_next;

    union
    {
        blit_entry blit;
        uint32_t color;
    };
};

//////////////////////////////////////////////////////////////////////

struct display_list
{
    struct display_list_entry *head;    // most recently added entry
    struct display_list_entry root;     // dummy head entry
};

//////////////////////////////////////////////////////////////////////

uint32_t display_buffer[LCD_WIDTH * SECTION_HEIGHT];

uint8_t dma_buffer[2][LCD_WIDTH * BYTES_PER_LINE * SECTION_HEIGHT];    // 18 bpp

int dma_buffer_id = 0;

uint8_t display_list_buffer[16384];

uint16_t display_list_used;

struct display_list display_lists[NUM_SECTIONS];

//////////////////////////////////////////////////////////////////////

inline display_list_entry *get_display_list_entry(uint16_t offset)
{
    return reinterpret_cast<display_list_entry *>(display_list_buffer + offset);
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

void display_reset()
{
    ESP_LOGI(TAG, "reset");

    display_list_used = 0;
    for(display_list &d : display_lists) {
        d.head = &d.root;
    }
}

//////////////////////////////////////////////////////////////////////

void display_imagerect(vec2 const *dst_pos, vec2 const *src_pos, vec2 const *size, uint8_t image_id, uint8_t blendmode)
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
        e->x = (uint8_t)dst_pos->x;
        e->y = (uint8_t)dst_y;
        e->w = (uint8_t)size->x;
        e->h = (uint8_t)cur_height;
        e->blit.image_id = image_id;
        e->blit.src_x = (uint8_t)src_pos->x;
        e->blit.src_y = (uint8_t)src_y;
        e->flags = blendmode | 0x80;
        e->display_list_next = 0xffff;
        src_y += cur_height;
        remaining_height -= cur_height;
        dst_y = 0;
        d += 1;

    } while(remaining_height > 0);
}

//////////////////////////////////////////////////////////////////////

void display_fillrect(vec2 const *dst_pos, vec2 const *size, uint32_t color, uint8_t blendmode)
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
        e->x = (uint8_t)dst_pos->x;
        e->y = (uint8_t)dst_y;
        e->w = (uint8_t)size->x;
        e->h = (uint8_t)cur_height;
        e->color = color;
        e->flags = blendmode;

        remaining_height -= cur_height;
        dst_y = 0;
        d += 1;

    } while(remaining_height > 0);
}

void draw_display_list(display_list *l)
{
    uint16_t offset = l->root.display_list_next;

    while(offset != 0xffff) {

        display_list_entry *e = get_display_list_entry(offset);

        if(e->flags & 0x80) {
            // blit
        } else {
            // fillrect
        }

        offset = e->display_list_next;
    }
}

void display_update()
{
}