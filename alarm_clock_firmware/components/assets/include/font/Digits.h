// FONT: Digits_font has 11 graphics

extern font_graphic Digits_font_graphics[11];
extern int16_t Digits_font_lookup[256];
extern uint8_t Digits_font_advance[256];
extern font_data Digits_font;

extern const uint8_t Digits_font_png_start[];
extern const uint8_t Digits_font_png_end[];
#define Digits_font_png_size ((size_t)(Digits_font_png_end - Digits_font_png_start))
