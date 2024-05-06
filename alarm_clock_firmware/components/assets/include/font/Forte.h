// FONT: Forte_font has 68 graphics

extern font_graphic Forte_font_graphics[68];
extern int16_t Forte_font_lookup[256];
extern uint8_t Forte_font_advance[256];
extern font_data Forte_font;

extern const uint8_t Forte_font_png_start[];
extern const uint8_t Forte_font_png_end[];
#define Forte_font_png_size ((size_t)(Forte_font_png_end - Forte_font_png_start))
