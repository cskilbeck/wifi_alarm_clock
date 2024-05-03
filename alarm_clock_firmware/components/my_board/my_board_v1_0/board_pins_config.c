//////////////////////////////////////////////////////////////////////

#include "esp_log.h"
#include "driver/gpio.h"
#include <string.h>
#include "board.h"
#include "audio_error.h"
#include "audio_mem.h"
#include "soc/soc_caps.h"

static const char *TAG = "MY_BOARD_V1_0";

//////////////////////////////////////////////////////////////////////

esp_err_t get_i2s_pins(int port, board_i2s_pin_t *i2s_config)
{
    AUDIO_NULL_CHECK(TAG, i2s_config, return ESP_FAIL);
    if(port == 0) {
        i2s_config->mck_io_num = -1;
        i2s_config->data_in_num = -1;
        i2s_config->ws_io_num = GPIO_NUM_9;
        i2s_config->bck_io_num = GPIO_NUM_10;
        i2s_config->data_out_num = GPIO_NUM_11;
    } else if(port == 1) {
        i2s_config->bck_io_num = -1;
        i2s_config->ws_io_num = -1;
        i2s_config->data_out_num = -1;
        i2s_config->data_in_num = -1;
    } else {
        memset(i2s_config, -1, sizeof(board_i2s_pin_t));
        ESP_LOGE(TAG, "i2s port %d is not supported", port);
        return ESP_FAIL;
    }

    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////
// volume up button

int8_t get_input_volup_id(void)
{
    return BUTTON_VOLUP_ID;
}

//////////////////////////////////////////////////////////////////////
// volume down button

int8_t get_input_voldown_id(void)
{
    return BUTTON_VOLDOWN_ID;
}

//////////////////////////////////////////////////////////////////////
// pa enable

int8_t get_pa_enable_gpio(void)
{
    return PA_ENABLE_GPIO;
}

//////////////////////////////////////////////////////////////////////
// mode button

int8_t get_input_mode_id(void)
{
    return BUTTON_MODE_ID;
}

//////////////////////////////////////////////////////////////////////
// set button

int8_t get_input_set_id(void)
{
    return BUTTON_SET_ID;
}

//////////////////////////////////////////////////////////////////////
// play button

int8_t get_input_play_id(void)
{
    return BUTTON_PLAY_ID;
}

//////////////////////////////////////////////////////////////////////
// mute button

int8_t get_input_mute_id(void)
{
    return BUTTON_MUTE_ID;
}
