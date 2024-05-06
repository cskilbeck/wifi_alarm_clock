// FONT: Big_font has 69 graphics

extern font_graphic Big_font_graphics[69];
extern int16_t Big_font_lookup[256];
extern uint8_t Big_font_advance[256];
extern font_data Big_font;

extern const uint8_t Big_font_png_start[];
extern const uint8_t Big_font_png_end[];
#define Big_font_png_size ((size_t)(Big_font_png_end - Big_font_png_start))
