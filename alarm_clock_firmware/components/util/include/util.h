#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////

typedef struct vec2i
{
    int x;
    int y;
} vec2i;

typedef struct vec2b
{
    uint8_t x;
    uint8_t y;
} vec2b;

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

//////////////////////////////////////////////////////////////////////

#if defined(__cplusplus)
}
#endif

//////////////////////////////////////////////////////////////////////
// C++ only

#if defined(__cplusplus)

template <typename T> T min(T const &a, T const &b)
{
    return a < b ? a : b;
}

template <typename T> T max(T const &a, T const &b)
{
    return a < b ? a : b;
}

template <typename T, std::size_t N> constexpr std::size_t countof(T const (&)[N]) noexcept
{
    return N;
}

#endif
