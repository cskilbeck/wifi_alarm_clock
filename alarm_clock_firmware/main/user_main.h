#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum main_message_code_e
{
    main_message_code_invalid = 0,
    main_message_code_wifi_connected = 1,
    main_message_code_wifi_disconnected = 2,
    main_message_code_user_input = 3,

} main_message_code_t;

typedef struct main_message_s
{
    main_message_code_t code;
    uint32_t param[2];

} main_message_t;

extern QueueHandle_t main_message_queue;

#if defined(__cplusplus)
}
#endif
