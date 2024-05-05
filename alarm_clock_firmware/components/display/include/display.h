#pragma once

#include <stdint.h>

//////////////////////////////////////////////////////////////////////

struct display_list;

enum display_blendmode
{
    blend_opaque = 0,
    blend_add = 1,
    blend_multiply = 2
};

void display_reset();
void display_imagerect(vec2i const *dst_pos, vec2i const *src_pos, vec2i const *size, uint8_t image_id, uint8_t alpha, uint8_t blendmode);
void display_fillrect(vec2i const *dst_pos, vec2i const *size, uint32_t color, uint8_t blendmode);
void display_list_draw(display_list *l, uint32_t *buffer);
