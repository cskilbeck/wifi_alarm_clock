// FONT: Cascadia_font has 95 graphics

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

font_graphic Cascadia_font_graphics[95] = {
    { 4, 9, 138, 46, 5, 15 },
    { 3, 9, 34, 51, 7, 7 },
    { 1, 9, 177, 31, 11, 15 },
    { 1, 6, 12, 0, 11, 21 },
    { 0, 9, 141, 16, 13, 15 },
    { 0, 9, 154, 16, 13, 15 },
    { 5, 9, 41, 51, 3, 7 },
    { 2, 6, 90, 0, 9, 20 },
    { 2, 6, 99, 0, 8, 20 },
    { 1, 11, 216, 46, 11, 11 },
    { 1, 10, 143, 46, 11, 12 },
    { 4, 21, 21, 51, 4, 8 },
    { 1, 15, 165, 57, 11, 3 },
    { 4, 20, 0, 53, 5, 4 },
    { 1, 6, 68, 0, 11, 20 },
    { 1, 9, 188, 31, 11, 15 },
    { 1, 9, 199, 31, 11, 15 },
    { 1, 9, 210, 31, 11, 15 },
    { 1, 9, 221, 31, 11, 15 },
    { 0, 9, 167, 16, 13, 15 },
    { 1, 9, 232, 31, 11, 15 },
    { 1, 9, 243, 31, 11, 15 },
    { 1, 9, 68, 20, 12, 15 },
    { 1, 9, 116, 33, 11, 15 },
    { 1, 9, 127, 33, 11, 15 },
    { 4, 13, 249, 46, 5, 11 },
    { 4, 13, 136, 16, 5, 16 },
    { 0, 11, 180, 46, 12, 11 },
    { 1, 12, 10, 51, 11, 8 },
    { 1, 11, 227, 46, 11, 11 },
    { 1, 9, 0, 38, 10, 15 },
    { 1, 9, 107, 0, 11, 19 },
    { 0, 9, 180, 16, 13, 15 },
    { 1, 9, 80, 20, 12, 15 },
    { 0, 9, 92, 20, 12, 15 },
    { 1, 9, 104, 20, 12, 15 },
    { 1, 9, 48, 35, 11, 15 },
    { 1, 9, 12, 21, 12, 15 },
    { 0, 9, 24, 21, 12, 15 },
    { 1, 9, 59, 35, 11, 15 },
    { 1, 9, 70, 35, 11, 15 },
    { 0, 9, 36, 21, 12, 15 },
    { 1, 9, 0, 23, 12, 15 },
    { 1, 9, 81, 35, 11, 15 },
    { 1, 9, 92, 35, 11, 15 },
    { 1, 9, 103, 35, 11, 15 },
    { 0, 9, 193, 16, 13, 15 },
    { 1, 9, 141, 31, 12, 15 },
    { 0, 9, 55, 0, 13, 20 },
    { 1, 9, 153, 31, 12, 15 },
    { 1, 9, 12, 36, 11, 15 },
    { 0, 9, 206, 16, 13, 15 },
    { 1, 9, 23, 36, 11, 15 },
    { 0, 9, 219, 16, 13, 15 },
    { 0, 9, 232, 16, 13, 15 },
    { 0, 9, 118, 18, 13, 15 },
    { 0, 9, 55, 20, 13, 15 },
    { 1, 9, 34, 36, 11, 15 },
    { 4, 6, 41, 0, 7, 21 },
    { 1, 6, 79, 0, 11, 20 },
    { 2, 6, 48, 0, 7, 21 },
    { 2, 9, 25, 51, 9, 7 },
    { 1, 24, 176, 57, 11, 3 },
    { 4, 7, 0, 57, 5, 4 },
    { 1, 13, 192, 46, 12, 11 },
    { 1, 8, 173, 0, 11, 16 },
    { 1, 13, 238, 46, 11, 11 },
    { 1, 8, 184, 0, 11, 16 },
    { 1, 13, 114, 48, 11, 11 },
    { 0, 8, 136, 0, 13, 16 },
    { 1, 13, 195, 0, 11, 16 },
    { 1, 8, 206, 0, 11, 16 },
    { 1, 7, 125, 0, 11, 17 },
    { 1, 7, 3, 0, 9, 22 },
    { 1, 8, 149, 0, 12, 16 },
    { 1, 8, 217, 0, 11, 16 },
    { 1, 13, 125, 48, 11, 11 },
    { 1, 13, 45, 50, 11, 11 },
    { 1, 13, 56, 50, 11, 11 },
    { 1, 13, 228, 0, 11, 16 },
    { 1, 13, 239, 0, 11, 16 },
    { 0, 13, 154, 46, 13, 11 },
    { 1, 13, 67, 50, 11, 11 },
    { 0, 9, 165, 31, 12, 15 },
    { 1, 13, 204, 46, 12, 11 },
    { 1, 13, 78, 50, 11, 11 },
    { 0, 13, 167, 46, 13, 11 },
    { 1, 13, 89, 50, 11, 11 },
    { 0, 13, 161, 0, 12, 16 },
    { 1, 13, 100, 50, 11, 11 },
    { 2, 6, 23, 0, 9, 21 },
    { 5, 5, 0, 0, 3, 23 },
    { 2, 6, 32, 0, 9, 21 },
    { 1, 14, 154, 57, 11, 6 },
    { 1, 5, 118, 0, 7, 18 },
};

int16_t Cascadia_font_lookup[256] = {
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
     15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30,
     31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46,
     47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62,
     63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78,
     79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

uint8_t Cascadia_font_advance[256] = {
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
     13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
     13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
     13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
     13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
     13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
     13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
      0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

font_data Cascadia_font = {
    Cascadia_font_graphics,
    Cascadia_font_lookup,
    Cascadia_font_advance,
    29, // height
    95, // num_graphics
    256, // num_lookups
    256 // num_advances
};

extern const uint8_t Cascadia_png_start[] asm("_binary_Cascadia0_png_start");
extern const uint8_t Cascadia_png_end[] asm("_binary_Cascadia0_png_end");
