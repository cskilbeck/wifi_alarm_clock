//////////////////////////////////////////////////////////////////////

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_timer.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "driver/gpio.h"
#include "esp_rom_gpio.h"

#include "util.h"
#include "encoder.h"
#include "led.h"

//////////////////////////////////////////////////////////////////////

LOG_CONTEXT("encoder");

//////////////////////////////////////////////////////////////////////

struct encoder
{
    uint8_t encoder_state;
    uint8_t encoder_store;
    uint8_t button_history;
    uint8_t button_state;
    esp_timer_handle_t timer_handle;
    QueueHandle_t input_queue;
    encoder_config_t config;
};

//////////////////////////////////////////////////////////////////////

namespace
{
    uint16_t const encoder_valid_bits = 0b0110100110010110;

    //////////////////////////////////////////////////////////////////////

    void IRAM_ATTR button_on_timer(void *e)
    {
        encoder_handle_t encoder = reinterpret_cast<encoder_handle_t>(e);
        int b = 1 - gpio_get_level(encoder->config.gpio_button);
        encoder->button_state = b;
        int cur_state = encoder->button_history;
        cur_state <<= 1;
        cur_state |= b;
        cur_state &= 3;
        encoder->button_history = cur_state;
        BaseType_t woken = pdFALSE;
        uint8_t msg = ENCODER_MSG_NULL;
        switch(cur_state) {
        case 1:
            msg = ENCODER_MSG_PRESS;
            xQueueSend(encoder->input_queue, &msg, 0);
            break;
        case 2:
            msg = ENCODER_MSG_RELEASE;
            xQueueSend(encoder->input_queue, &msg, 0);
            break;
        }
    }

    //////////////////////////////////////////////////////////////////////

    void IRAM_ATTR encoder_isr_handler(void *e)
    {
        encoder_handle_t encoder = reinterpret_cast<encoder_handle_t>(e);
        int a = gpio_get_level(encoder->config.gpio_a);
        int b = gpio_get_level(encoder->config.gpio_b);
        int bits = (a << 1) | b;
        encoder->encoder_state <<= 2;
        encoder->encoder_state |= bits;
        encoder->encoder_state &= 0xf;

        BaseType_t woken = pdFALSE;

        uint8_t msg = ENCODER_MSG_NULL;

        if((encoder_valid_bits & (1 << encoder->encoder_state)) != 0) {
            encoder->encoder_store = (encoder->encoder_store << 4) | encoder->encoder_state;
            switch((encoder->encoder_store)) {
            case 0xe8:
                msg = ENCODER_MSG_ROTATE_CW;
                xQueueSendFromISR(encoder->input_queue, &msg, &woken);
                break;
            case 0x2b:
                msg = ENCODER_MSG_ROTATE_CCW;
                xQueueSendFromISR(encoder->input_queue, &msg, &woken);
                break;
            }
        }
        portYIELD_FROM_ISR(woken);
    }

}    // namespace

//////////////////////////////////////////////////////////////////////

esp_err_t encoder_init(encoder_config_t *cfg, encoder_handle_t *handle)
{
    LOG_I("init");

    encoder_handle_t encoder = (encoder_handle_t)heap_caps_calloc(1, sizeof(struct encoder), MALLOC_CAP_INTERNAL);

    if(encoder == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    encoder->config = *cfg;

    encoder->input_queue = xQueueCreate(32, sizeof(uint8_t));

    gpio_config_t gpiocfg = {};
    gpiocfg.pin_bit_mask = gpio_bit(cfg->gpio_a) | gpio_bit(cfg->gpio_b);
    gpiocfg.mode = GPIO_MODE_INPUT;
    gpiocfg.pull_up_en = GPIO_PULLUP_ENABLE;
    gpiocfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpiocfg.intr_type = GPIO_INTR_ANYEDGE;
    gpio_config(&gpiocfg);

    gpiocfg.intr_type = GPIO_INTR_DISABLE;
    gpiocfg.pin_bit_mask = gpio_bit(cfg->gpio_button);
    gpio_config(&gpiocfg);

    LOG_I("Adding GPIO interrupt handlers");

    ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_CPU_AFFINITY_AUTO));

    ESP_ERROR_CHECK(gpio_set_intr_type(cfg->gpio_a, GPIO_INTR_ANYEDGE));
    ESP_ERROR_CHECK(gpio_set_intr_type(cfg->gpio_b, GPIO_INTR_ANYEDGE));
    ESP_ERROR_CHECK(gpio_isr_handler_add(cfg->gpio_a, encoder_isr_handler, encoder));
    ESP_ERROR_CHECK(gpio_isr_handler_add(cfg->gpio_b, encoder_isr_handler, encoder));
    ESP_ERROR_CHECK(gpio_intr_enable(cfg->gpio_a));
    ESP_ERROR_CHECK(gpio_intr_enable(cfg->gpio_b));

    LOG_I("starting button timer");

    esp_timer_create_args_t button_timer_args = {};
    button_timer_args.callback = button_on_timer;
    button_timer_args.dispatch_method = ESP_TIMER_TASK;
    button_timer_args.skip_unhandled_events = false;
    button_timer_args.arg = encoder;
    ESP_ERROR_CHECK(esp_timer_create(&button_timer_args, &encoder->timer_handle));
    ESP_ERROR_CHECK(esp_timer_start_periodic(encoder->timer_handle, 1000000 / 100));

    LOG_I("init done");

    *handle = encoder;

    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

esp_err_t encoder_destroy(encoder_handle_t encoder)
{
    LOG_I("destroy");
    assert(false && "encoder_destroy not implemented");
    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

esp_err_t encoder_get_message(encoder_handle_t encoder, encoder_message_t *msg)
{
    uint8_t m;
    if(xQueueReceive(encoder->input_queue, &m, 0)) {
        *msg = static_cast<encoder_message_t>(m);
        return ESP_OK;
    }
    return ESP_ERR_NOT_FOUND;
}
