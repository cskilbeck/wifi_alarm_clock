// FONT: Cascadia_font has 95 graphics

extern font_graphic Cascadia_font_graphics[95];
extern int16_t Cascadia_font_lookup[256];
extern uint8_t Cascadia_font_advance[256];
extern font_data Cascadia_font;

extern const uint8_t Cascadia_font_png_start[] asm("_binary_Cascadia_font0_png_start")
extern const uint8_t Cascadia_font_png_end[] asm("_binary_Cascadia_font0_png_end")
#define Cascadia_font_png_size ((size_t)(Cascadia_font_png_end - Cascadia_font_png_start))
