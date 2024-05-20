#define _USE_MATH_DEFINES
#include <stdio.h>
#include <math.h>

namespace
{
    int constexpr diameter = 240;
    float constexpr circumference = 2.0f * (float)M_PI * diameter / 2.0f;

    size_t offsets[diameter];    // offsets into the map for each row
    size_t current_offset = 0;

    void sphere_map()
    {
        printf("short const circle_map[] = {\n");

        for(int y = 0; y < diameter; ++y) {

            float t = (y + 0.5f) / (diameter + 0.25f) * 2.0f - 1.0f;
            int width = (int)(sqrt(1.0f - t * t) * diameter + 0.5f);

            printf("\n    %d, // width for line %d\n    ", width, y);
            for(int x = 0; x < width; x++) {
                float t = x * 2.0f / (width - 0.99995f) - 1.0f;
                float v = sqrtf(1.0f - t * t);
                int u = int((atan2f(t, v) / (float)M_PI + 0.5f) * circumference / 2.0f);
                printf("%d,", u);
            }
            printf("\n");
            offsets[y] = current_offset;
            current_offset += (size_t)(width + 1);
        }
        printf("};\n");
    }
}    // namespace

int main(int, char **)
{
    sphere_map();

    printf("int const circle_offsets[%d] = {\n", diameter);
    for(int i = 0; i < diameter; ++i) {
        printf("%llu,", offsets[i]);
    }
    printf("\n};\n");
}