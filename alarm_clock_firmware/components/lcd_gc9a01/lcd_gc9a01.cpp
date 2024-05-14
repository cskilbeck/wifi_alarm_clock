//////////////////////////////////////////////////////////////////////
// LCD base level driver

#include <memory.h>
#include <stdint.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/event_groups.h>

#include <esp_err.h>
#include <esp_log.h>
#include "esp_system.h"

#include "rom/ets_sys.h"

#include <driver/spi_master.h>
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "lcd_gc9a01.h"
#include "util.h"

LOG_CONTEXT("lcd");

#define SPI_BIT_DMA_COMPLETE 1
#define SPI_BIT_SETUP_COMPLETE 2

//////////////////////////////////////////////////////////////////////
// LCD SPI

#define LCD_SPI_HOST SPI2_HOST

// effectively 26.6666 MHz - can't go higher without causing I2S problems

// #define LCD_SPI_SPEED 26666666
#define LCD_SPI_SPEED 80000000

#define LCD_PIN_NUM_MISO GPIO_NUM_12
#define LCD_PIN_NUM_MOSI GPIO_NUM_13
#define LCD_PIN_NUM_CLK GPIO_NUM_14
#define LCD_PIN_NUM_CS GPIO_NUM_15
#define LCD_PIN_NUM_DC GPIO_NUM_4
#define LCD_PIN_NUM_RST GPIO_NUM_5
#define LCD_PIN_NUM_BCKL GPIO_NUM_6

//////////////////////////////////////////////////////////////////////
// LCD BACKLIGHT

#define LCD_BL_TIMER LEDC_TIMER_0
#define LCD_BL_MODE LEDC_LOW_SPEED_MODE
#define LCD_BL_OUTPUT_IO LCD_PIN_NUM_BCKL
#define LCD_BL_CHANNEL LEDC_CHANNEL_0
#define LCD_BL_DUTY_RES LEDC_TIMER_13_BIT    // duty resolution 13 bits
#define LCD_BL_FREQUENCY 4000                // 4 kHz

//////////////////////////////////////////////////////////////////////

#define LCD_NUM_SPI_TRANSFERS (LCD_NUM_SECTIONS + 5)

static_assert(LCD_HEIGHT % LCD_SECTION_HEIGHT == 0);
static_assert(LCD_BITS_PER_PIXEL == 16 || LCD_BITS_PER_PIXEL == 18);

//////////////////////////////////////////////////////////////////////

namespace
{
    struct spi_callback_user_data_s
    {
        typedef void (*spi_callback)(void);

        spi_callback pre_callback;
        spi_callback post_callback;
    };

    typedef struct spi_callback_user_data_s spi_callback_user_data_t;

    spi_device_handle_t spi;

    DMA_ATTR spi_transaction_t spi_transactions[LCD_NUM_SPI_TRANSFERS];

    IRAM_ATTR void spi_callback_set_data();
    IRAM_ATTR void spi_callback_clear_data();
    IRAM_ATTR void spi_callback_setup_complete();
    IRAM_ATTR void spi_callback_dma_complete();

    DMA_ATTR uint8_t lcd_buffer[2][LCD_BYTES_PER_LINE * LCD_SECTION_HEIGHT];

    spi_callback_user_data_t spi_callback_cmd = { .pre_callback = spi_callback_clear_data, .post_callback = nullptr };

    spi_callback_user_data_t spi_callback_cmd_last = { .pre_callback = spi_callback_clear_data, .post_callback = spi_callback_setup_complete };

    spi_callback_user_data_t spi_callback_data = { .pre_callback = spi_callback_set_data, .post_callback = nullptr };

    spi_callback_user_data_t spi_callback_dma_data = { .pre_callback = spi_callback_set_data, .post_callback = spi_callback_dma_complete };

    //////////////////////////////////////////////////////////////////////

    // clang-format off
    uint8_t const GC9A01A_initcmds[] = {

        0, 0xEF,
        1, 0xEB, 0x14,
        0, 0xFE,
        0, 0xEF,
        1, 0xEB, 0x14,
        1, 0x84, 0x40,
        1, 0x85, 0xFF,
        1, 0x86, 0xFF,
        1, 0x87, 0xFF,
        1, 0x88, 0x0A,
        1, 0x89, 0x21,
        1, 0x8A, 0x00,
        1, 0x8B, 0x80,
        1, 0x8C, 0x01,
        1, 0x8D, 0x01,
        1, 0x8E, 0xFF,
        1, 0x8F, 0xFF,
        2, 0xB6, 0x00, 0x00,
        1, 0x36, 0x48,
#if LCD_BITS_PER_PIXEL == 18
        1, 0x3A, 0x06,
#else
        1, 0x3A, 0x05,
#endif
        4, 0x90, 0x08, 0x08, 0x08, 0x08,
        1, 0xBD, 0x06,
        1, 0xBC, 0x00,
        3, 0xFF, 0x60, 0x01, 0x04,
        1, 0xC3, 0x13,
        1, 0xC4, 0x13,
        1, 0xC9, 0x22,
        1, 0xBE, 0x11,
        2, 0xE1, 0x10, 0x0E,
        3, 0xDF, 0x21, 0x0c, 0x02,
        6, 0xF0, 0x45, 0x09, 0x08, 0x08, 0x26, 0x2A,
        6, 0xF1, 0x43, 0x70, 0x72, 0x36, 0x37, 0x6F,
        6, 0xF2, 0x45, 0x09, 0x08, 0x08, 0x26, 0x2A,
        6, 0xF3, 0x43, 0x70, 0x72, 0x36, 0x37, 0x6F,
        2, 0xED, 0x1B, 0x0B,
        1, 0xAE, 0x77,
        1, 0xCD, 0x63,
        1, 0xE8, 0x34,
        12, 0x62, 0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70,
        12, 0x63, 0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70,
        7, 0x64, 0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07,
        10, 0x66, 0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00,
        10, 0x67, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98,
        7, 0x74, 0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00,
        2, 0x98, 0x3e, 0x07,
        0, 0x35,
        0, 0x21,
        0x80, 0x11,
        0x80, 0x29,
        0xff
    };
    // clang-format on

    //////////////////////////////////////////////////////////////////////

    EventGroupHandle_t spi_bits;

    void init_dma_flag()
    {
        spi_bits = xEventGroupCreate();
    }

    void spi_callback_setup_complete()
    {
        BaseType_t woken = pdFALSE;
        xEventGroupSetBitsFromISR(spi_bits, SPI_BIT_SETUP_COMPLETE, &woken);
        portYIELD_FROM_ISR(woken);
    }

    void spi_callback_dma_complete()
    {
        gpio_set_level(LCD_PIN_NUM_DC, 0);
        BaseType_t woken = pdFALSE;
        xEventGroupSetBitsFromISR(spi_bits, SPI_BIT_DMA_COMPLETE, &woken);
        portYIELD_FROM_ISR(woken);
    }

    //////////////////////////////////////////////////////////////////////

    void spi_callback_set_data()
    {
        gpio_set_level(LCD_PIN_NUM_DC, 1);
    }

    //////////////////////////////////////////////////////////////////////

    void spi_callback_clear_data()
    {
        gpio_set_level(LCD_PIN_NUM_DC, 0);
    }

    //////////////////////////////////////////////////////////////////////

    esp_err_t lcd_cmd(const uint8_t cmd, bool keep_cs_active)
    {
        spi_transaction_t t;
        memset(&t, 0, sizeof(t));
        t.length = 8;
        t.tx_data[0] = cmd;
        t.user = &spi_callback_cmd;
        t.flags = SPI_TRANS_USE_TXDATA;
        if(keep_cs_active) {
            t.flags |= SPI_TRANS_CS_KEEP_ACTIVE;
        }
        ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &t));
        return ESP_OK;
    }

    //////////////////////////////////////////////////////////////////////

    esp_err_t lcd_data(const uint8_t *data, int len)
    {
        if(len == 0) {
            return ESP_OK;
        }

        spi_transaction_t t;
        memset(&t, 0, sizeof(t));
        t.length = len * 8;
        t.tx_buffer = data;
        t.user = &spi_callback_data;
        ESP_ERROR_CHECK(spi_device_polling_transmit(spi, &t));
        return ESP_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // This function is called (in irq context!) just before a transmission starts. It will
    // set the D/C line to the value indicated in the user field.

    void lcd_spi_pre_transfer_callback(spi_transaction_t *t)
    {
        spi_callback_user_data_t *p = (spi_callback_user_data_t *)t->user;
        if(p != nullptr && p->pre_callback != nullptr) {
            p->pre_callback();
        }
    }

    //////////////////////////////////////////////////////////////////////

    void lcd_spi_post_transfer_complete(spi_transaction_t *t)
    {
        spi_callback_user_data_t *p = (spi_callback_user_data_t *)t->user;
        if(p != nullptr && p->post_callback != nullptr) {
            p->post_callback();
        }
    }

    //////////////////////////////////////////////////////////////////////

    esp_err_t init_backlight_pwm(void)
    {
        ledc_timer_config_t ledc_timer = {};
        ledc_timer.speed_mode = LCD_BL_MODE;
        ledc_timer.timer_num = LCD_BL_TIMER;
        ledc_timer.duty_resolution = LCD_BL_DUTY_RES;
        ledc_timer.freq_hz = LCD_BL_FREQUENCY;
        ledc_timer.clk_cfg = LEDC_AUTO_CLK;
        ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

        ledc_channel_config_t ledc_channel = {};
        ledc_channel.speed_mode = LCD_BL_MODE;
        ledc_channel.channel = LCD_BL_CHANNEL;
        ledc_channel.timer_sel = LCD_BL_TIMER;
        ledc_channel.intr_type = LEDC_INTR_DISABLE;
        ledc_channel.gpio_num = LCD_BL_OUTPUT_IO;
        ledc_channel.duty = 0;
        ledc_channel.hpoint = 0;
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

        return ESP_OK;
    }

}    // namespace

//////////////////////////////////////////////////////////////////////

esp_err_t lcd_init()
{
    LOG_I("INIT");

    ESP_ERROR_CHECK(init_backlight_pwm());

    spi_bus_config_t buscfg = {};
    buscfg.miso_io_num = LCD_PIN_NUM_MISO;
    buscfg.mosi_io_num = LCD_PIN_NUM_MOSI;
    buscfg.sclk_io_num = LCD_PIN_NUM_CLK;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.flags = SPICOMMON_BUSFLAG_MASTER;
    buscfg.max_transfer_sz = LCD_BYTES_PER_LINE * LCD_SECTION_HEIGHT;

    spi_device_interface_config_t devcfg = {};
    devcfg.flags = SPI_DEVICE_NO_RETURN_RESULT;
    devcfg.clock_speed_hz = LCD_SPI_SPEED;
    devcfg.mode = 0;
    devcfg.queue_size = 6;
    devcfg.spics_io_num = LCD_PIN_NUM_CS;
    devcfg.queue_size = LCD_NUM_SPI_TRANSFERS;
    devcfg.pre_cb = lcd_spi_pre_transfer_callback;
    devcfg.post_cb = lcd_spi_post_transfer_complete;

    // Initialize the SPI bus
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(spi_bus_add_device(LCD_SPI_HOST, &devcfg, &spi));

    // Initialize non-SPI GPIOs
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << LCD_PIN_NUM_DC) | (1ULL << LCD_PIN_NUM_RST);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    // Reset the LCD
    gpio_set_level(LCD_PIN_NUM_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));

    gpio_set_level(LCD_PIN_NUM_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    uint8_t const *i = GC9A01A_initcmds;

    while(*i != 0xff) {

        uint8_t len = *i++;
        uint8_t cmd = *i++;

        bool delay = (len & 0x80) != 0;
        uint8_t actual_len = len & 0x1f;

        lcd_cmd(cmd, false);
        lcd_data(i, actual_len);

        if(delay) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        i += actual_len;
    }

    memset(spi_transactions, 0, sizeof(spi_transactions));

    spi_transactions[0].tx_data[0] = 0x2A;    // Column Address Set
    spi_transactions[0].length = 8;
    spi_transactions[0].user = &spi_callback_cmd;
    spi_transactions[0].flags = SPI_TRANS_USE_TXDATA;

    spi_transactions[1].tx_data[0] = 0;                         // start col High
    spi_transactions[1].tx_data[1] = 0;                         // start col Low
    spi_transactions[1].tx_data[2] = (LCD_WIDTH - 1) >> 8;      // end col High
    spi_transactions[1].tx_data[3] = (LCD_WIDTH - 1) & 0xff;    // end col Low
    spi_transactions[1].length = 8 * 4;
    spi_transactions[1].user = &spi_callback_data;
    spi_transactions[1].flags = SPI_TRANS_USE_TXDATA;

    spi_transactions[2].tx_data[0] = 0x2B;    // Row address set
    spi_transactions[2].length = 8;
    spi_transactions[2].user = &spi_callback_cmd;
    spi_transactions[2].flags = SPI_TRANS_USE_TXDATA;

    spi_transactions[3].tx_data[0] = 0;                          // start row high
    spi_transactions[3].tx_data[1] = 0;                          // start row low
    spi_transactions[3].tx_data[2] = (LCD_HEIGHT - 1) >> 8;      // end row high
    spi_transactions[3].tx_data[3] = (LCD_HEIGHT - 1) & 0xff;    // end row low
    spi_transactions[3].length = 8 * 4;
    spi_transactions[3].user = &spi_callback_data;
    spi_transactions[3].flags = SPI_TRANS_USE_TXDATA;

    spi_transactions[4].tx_data[0] = 0x2C;
    spi_transactions[4].length = 8;
    spi_transactions[4].user = &spi_callback_cmd_last;
    spi_transactions[4].flags = SPI_TRANS_USE_TXDATA;

    for(int i = 5; i < LCD_NUM_SPI_TRANSFERS; i++) {

        int x = i - 5;
        spi_transactions[i].tx_buffer = lcd_buffer[x & 1];
        spi_transactions[i].length = LCD_WIDTH * LCD_BYTES_PER_PIXEL * 8 * LCD_SECTION_HEIGHT;
        spi_transactions[i].user = &spi_callback_dma_data;
        spi_transactions[i].flags = 0;
    }

    init_dma_flag();

    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

esp_err_t lcd_update(lcd_buffer_filler filler_callback)
{
    if(filler_callback == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    // send the setup transfers

    xEventGroupClearBits(spi_bits, SPI_BIT_SETUP_COMPLETE);

    for(int i = 0; i < 5; ++i) {
        spi_device_queue_trans(spi, spi_transactions + i, portMAX_DELAY);
    }

    // wait for setup to complete and set the dma complete flag

    xEventGroupSync(spi_bits, SPI_BIT_DMA_COMPLETE, SPI_BIT_SETUP_COMPLETE, portMAX_DELAY);

    for(int i = 0; i < LCD_NUM_SECTIONS; ++i) {

        // draw the pixels into the buffer
        filler_callback(i, lcd_buffer[i & 1]);

        // wait for previous dma to complete
        xEventGroupWaitBits(spi_bits, SPI_BIT_DMA_COMPLETE, pdTRUE, pdTRUE, portMAX_DELAY);

        // start this buffer dma transfer
        ESP_ERROR_CHECK(spi_device_queue_trans(spi, spi_transactions + i + 5, portMAX_DELAY));
    }
    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

esp_err_t lcd_set_backlight(uint32_t brightness_0_8191)
{
    ESP_ERROR_CHECK(ledc_set_duty(LCD_BL_MODE, LCD_BL_CHANNEL, brightness_0_8191));
    ESP_ERROR_CHECK(ledc_update_duty(LCD_BL_MODE, LCD_BL_CHANNEL));
    return ESP_OK;
}
