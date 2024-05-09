#pragma once

#include <esp_err.h>

//////////////////////////////////////////////////////////////////////

#if defined(__cplusplus)
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////

void util_print_error_return_if_failed(esp_err_t rc, char const *file, int line, char const *function, char const *expression);

//////////////////////////////////////////////////////////////////////

typedef struct vec2i
{
    int x;
    int y;
} vec2i;

typedef struct vec2f
{
    float x;
    float y;
} vec2f;

typedef struct vec2b
{
    uint8_t x;
    uint8_t y;
} vec2b;

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

//////////////////////////////////////////////////////////////////////

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define LOG_TAG(x) static const char *TAG __attribute__((unused)) = x

// return if failed

#define ESP_RETURN_IF_FAILED(x)                                                           \
    do {                                                                                  \
        esp_err_t ret = (x);                                                              \
        if(ret != ESP_OK) {                                                               \
            util_print_error_return_if_failed(ret, __FILE__, __LINE__, __FUNCTION__, #x); \
            return ret;                                                                   \
        }                                                                                 \
    } while(false)
