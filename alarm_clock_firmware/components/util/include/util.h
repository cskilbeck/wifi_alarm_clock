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
    return a > b ? a : b;
}

template <typename T, std::size_t N> constexpr std::size_t countof(T const (&)[N]) noexcept
{
    return N;
}

inline uint64_t gpio_bit(int pin_num)
{
    return 1llu << pin_num;
}

#else

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#endif

//////////////////////////////////////////////////////////////////////

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define LOG_CONTEXT(x) static const char *LOG_TAG __attribute__((unused)) = x

#define LOG_E(...) ESP_LOGE(LOG_TAG, __VA_ARGS__)
#define LOG_W(...) ESP_LOGW(LOG_TAG, __VA_ARGS__)
#define LOG_I(...) ESP_LOGI(LOG_TAG, __VA_ARGS__)
#define LOG_D(...) ESP_LOGD(LOG_TAG, __VA_ARGS__)
#define LOG_V(...) ESP_LOGV(LOG_TAG, __VA_ARGS__)

// return if failed

#define ESP_RETURN_IF_FAILED(x)                                                           \
    do {                                                                                  \
        esp_err_t ret = (x);                                                              \
        if(ret != ESP_OK) {                                                               \
            util_print_error_return_if_failed(ret, __FILE__, __LINE__, __FUNCTION__, #x); \
            return ret;                                                                   \
        }                                                                                 \
    } while(false)

#define ESP_RETURN_IF_NULL(x)                                                                        \
    do {                                                                                             \
        if((void *)(x) == 0) {                                                                       \
            util_print_error_return_if_failed(ESP_ERR_NO_MEM, __FILE__, __LINE__, __FUNCTION__, #x); \
            return ESP_ERR_NO_MEM;                                                                   \
        }                                                                                            \
    } while(false)
