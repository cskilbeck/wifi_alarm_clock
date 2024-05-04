#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////

typedef struct vec2
{
    int x;
    int y;
} vec2;

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

#endif
