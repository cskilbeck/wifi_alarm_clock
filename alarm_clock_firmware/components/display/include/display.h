#pragma once

#include <stdint.h>
#include "util.h"

#if defined(__cplusplus)
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////

struct display_list;

typedef enum display_blendmode
{
    blend_opaque = 0,
    blend_add = 1,
    blend_multiply = 2
} display_blendmode;

void display_reset();
void display_imagerect(vec2i const *dst_pos, vec2i const *src_pos, vec2i const *size, uint8_t image_id, uint8_t alpha, uint8_t blendmode);
void display_fillrect(vec2i const *dst_pos, vec2i const *size, uint32_t color, uint8_t blendmode);
void display_list_draw(int section, uint8_t *buffer);

#if defined(__cplusplus)
}
#endif
