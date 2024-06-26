// FONT: Big_font has 69 graphics

// struct font_graphic {
//     int8_t offset_x;
//     int8_t offset_y;
//     uint8_t x;
//     uint8_t y;
//     uint8_t width;
//     uint8_t height;
// };

// struct font_data {
//     font_graphic *graphics;
//     int16_t *lookup;
//     uint8_t *advance;
//     int height;
//     int num_graphics;
//     int num_lookups;
//     int num_advances;
// };

font_graphic Big_font_graphics[69] = {
    { 2, 3, 72, 191, 9, 38 },
    { 1, 3, 38, 229, 18, 15 },
    { 1, 32, 243, 219, 9, 18 },
    { 2, 23, 56, 229, 20, 9 },
    { 1, 32, 99, 226, 9, 9 },
    { 2, 3, 182, 0, 24, 39 },
    { 1, 3, 46, 191, 14, 38 },
    { 1, 3, 50, 153, 24, 38 },
    { 1, 3, 206, 0, 24, 39 },
    { 2, 3, 112, 115, 26, 38 },
    { 2, 3, 122, 153, 23, 38 },
    { 1, 3, 145, 153, 23, 38 },
    { 1, 3, 168, 153, 23, 38 },
    { 2, 3, 157, 0, 25, 39 },
    { 1, 3, 191, 153, 23, 38 },
    { 1, 14, 234, 219, 9, 27 },
    { 0, 3, 214, 153, 23, 38 },
    { 0, 3, 104, 39, 34, 38 },
    { 3, 3, 208, 77, 28, 38 },
    { 2, 3, 73, 0, 28, 39 },
    { 3, 3, 0, 78, 28, 38 },
    { 3, 3, 138, 115, 26, 38 },
    { 3, 3, 164, 115, 26, 38 },
    { 2, 3, 101, 0, 28, 39 },
    { 3, 3, 121, 77, 29, 38 },
    { 2, 3, 81, 191, 9, 38 },
    { 0, 3, 0, 154, 23, 38 },
    { 3, 3, 172, 39, 31, 38 },
    { 3, 3, 190, 115, 26, 38 },
    { 3, 3, 138, 39, 34, 38 },
    { 3, 3, 91, 77, 30, 38 },
    { 2, 3, 129, 0, 28, 39 },
    { 3, 3, 28, 78, 28, 38 },
    { 2, 3, 13, 0, 31, 39 },
    { 3, 3, 150, 77, 29, 38 },
    { 1, 3, 44, 0, 29, 39 },
    { 0, 3, 56, 115, 28, 38 },
    { 2, 3, 84, 115, 28, 38 },
    { 0, 3, 203, 39, 31, 38 },
    { 0, 3, 60, 39, 44, 38 },
    { 0, 3, 60, 77, 31, 38 },
    { 0, 3, 179, 77, 29, 38 },
    { 1, 3, 0, 116, 25, 38 },
    { 1, 13, 138, 191, 24, 29 },
    { 2, 3, 23, 154, 23, 38 },
    { 2, 13, 162, 191, 24, 29 },
    { 2, 3, 74, 153, 24, 38 },
    { 2, 13, 186, 191, 24, 29 },
    { 0, 3, 237, 153, 16, 38 },
    { 2, 13, 230, 0, 24, 39 },
    { 2, 3, 98, 153, 24, 38 },
    { 2, 3, 90, 191, 9, 38 },
    { 0, 3, 0, 0, 13, 48 },
    { 2, 3, 25, 116, 25, 38 },
    { 2, 3, 60, 191, 12, 38 },
    { 2, 13, 0, 220, 38, 28 },
    { 2, 13, 165, 220, 24, 28 },
    { 2, 13, 210, 191, 24, 29 },
    { 2, 13, 37, 39, 23, 39 },
    { 2, 13, 13, 39, 24, 39 },
    { 2, 13, 234, 191, 20, 28 },
    { 0, 13, 113, 191, 25, 29 },
    { 0, 6, 99, 191, 14, 35 },
    { 2, 13, 189, 220, 23, 28 },
    { 0, 13, 113, 220, 26, 28 },
    { 0, 13, 0, 192, 39, 28 },
    { 0, 13, 139, 220, 26, 28 },
    { 0, 13, 216, 115, 26, 38 },
    { 2, 13, 212, 220, 22, 28 },
};

int16_t Big_font_lookup[256] = {
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1,  0,  1, -1, -1, -1, -1, -1, -1, -1, -1, -1,  2,  3,  4, -1,
      5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, 16,
     -1, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
     32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, -1, -1, -1, -1, -1,
     -1, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57,
     58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

uint8_t Big_font_advance[256] = {
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     13, 14, 19,  0,  0,  0,  0,  0,  0,  0,  0,  0, 11, 24, 11,  0,
     28, 17, 26, 26, 28, 27, 26, 25, 28, 26, 11,  0,  0,  0,  0, 22,
      0, 32, 32, 31, 33, 30, 28, 32, 34, 14, 25, 32, 29, 39, 35, 32,
     31, 33, 33, 31, 24, 33, 29, 42, 28, 25, 27,  0,  0,  0,  0,  0,
      0, 27, 27, 25, 27, 27, 16, 27, 28, 13, 13, 26, 14, 43, 28, 27,
     27, 27, 21, 26, 16, 28, 25, 39, 26, 24, 25,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

font_data Big_font = {
    Big_font_graphics,
    Big_font_lookup,
    Big_font_advance,
    50, // height
    69, // num_graphics
    256, // num_lookups
    256 // num_advances
};

extern const uint8_t Big_png_start[] asm("_binary_Big0_png_start");
extern const uint8_t Big_png_end[] asm("_binary_Big0_png_end");
