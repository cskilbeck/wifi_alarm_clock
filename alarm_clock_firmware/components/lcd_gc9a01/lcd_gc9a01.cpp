//////////////////////////////////////////////////////////////////////
// lcd_init();
// lcd_set_backlight(7000);
// uint16_t *buffer;
// if(lcd_get_backbuffer(&buffer, portMAX_DELAY) == ESP_OK) {
//      buffer[200] = 0xffff;
//      buffer[300] = 0xffff;
//      lcd_release_backbuffer_and_update();
// }

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
#include "display.h"

static char const *TAG = "lcd";

//////////////////////////////////////////////////////////////////////
// LCD SPI

#define LCD_HOST SPI2_HOST

// effectively 26.6666 MHz - can't go higher without causing I2S problems

#define LCD_SPI_SPEED 26666666

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

namespace
{
    struct spi_callback_user_data_s
    {
        typedef void (*spi_callback)(void);

        spi_callback pre_callback;
        spi_callback post_callback;
    };

    typedef struct spi_callback_user_data_s spi_callback_user_data_t;

    DMA_ATTR uint8_t lcd_buffer[2][LCD_BYTES_PER_LINE * LCD_SECTION_HEIGHT];

    spi_device_handle_t spi;
    int constexpr num_transfers = LCD_NUM_SECTIONS + 5;

    static_assert(LCD_HEIGHT % LCD_SECTION_HEIGHT == 0);

    volatile bool dma_buffer_sent = true;

    spi_transaction_t spi_transactions[num_transfers];

    void spi_callback_set_data();
    void spi_callback_clear_data();
    void spi_callback_dma_complete();

    spi_callback_user_data_t spi_callback_cmd = { .pre_callback = spi_callback_clear_data, .post_callback = nullptr };

    spi_callback_user_data_t spi_callback_data = { .pre_callback = spi_callback_set_data, .post_callback = nullptr };

    spi_callback_user_data_t spi_callback_dma_data = { .pre_callback = spi_callback_set_data, .post_callback = spi_callback_dma_complete };

    //////////////////////////////////////////////////////////////////////
    // The LCD needs a bunch of command/argument values to be initialized. They are stored in this struct.

    typedef struct
    {
        uint8_t cmd;
        uint8_t data[16];
        uint8_t databytes;    // No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
    } lcd_init_cmd_t;

    //////////////////////////////////////////////////////////////////////

    lcd_init_cmd_t const GC9A01A_initcmds[] = {

        { 0xEF, { 0x00 }, 0 },          //
        { 0xEB, { 0x14 }, 1 },          //
        { 0xFE, { 0x00 }, 0 },          //
        { 0xEF, { 0x00 }, 0 },          //
        { 0xEB, { 0x14 }, 1 },          //
        { 0x84, { 0x40 }, 1 },          //
        { 0x85, { 0xFF }, 1 },          //
        { 0x86, { 0xFF }, 1 },          //
        { 0x87, { 0xFF }, 1 },          //
        { 0x88, { 0x0A }, 1 },          //
        { 0x89, { 0x21 }, 1 },          //
        { 0x8A, { 0x00 }, 1 },          //
        { 0x8B, { 0x80 }, 1 },          //
        { 0x8C, { 0x01 }, 1 },          //
        { 0x8D, { 0x01 }, 1 },          //
        { 0x8E, { 0xFF }, 1 },          //
        { 0x8F, { 0xFF }, 1 },          //
        { 0xB6, { 0x00, 0x00 }, 2 },    //
        { 0x36, { 0x48 }, 1 },          //
#if LCD_BITS_PER_PIXEL == 18
        { 0x3A, { 0x06 }, 1 },    //
#else
        { 0x3A, { 0x05 }, 1 },    //
#endif
        { 0x90, { 0x08, 0x08, 0x08, 0x08 }, 4 },                //
        { 0xBD, { 0x06 }, 1 },                                  //
        { 0xBC, { 0x00 }, 1 },                                  //
        { 0xFF, { 0x60, 0x01, 0x04 }, 3 },                      //
        { 0xC3, { 0x13 }, 1 },                                  //
        { 0xC4, { 0x13 }, 1 },                                  //
        { 0xC9, { 0x22 }, 1 },                                  //
        { 0xBE, { 0x11 }, 1 },                                  //
        { 0xE1, { 0x10, 0x0E }, 2 },                            //
        { 0xDF, { 0x21, 0x0c, 0x02 }, 3 },                      //
        { 0xF0, { 0x45, 0x09, 0x08, 0x08, 0x26, 0x2A }, 6 },    //
        { 0xF1, { 0x43, 0x70, 0x72, 0x36, 0x37, 0x6F }, 6 },    //
        { 0xF2, { 0x45, 0x09, 0x08, 0x08, 0x26, 0x2A }, 6 },    //
        { 0xF3, { 0x43, 0x70, 0x72, 0x36, 0x37, 0x6F }, 6 },    //
        { 0xED, { 0x1B, 0x0B }, 2 },                            //
        { 0xAE, { 0x77 }, 1 },                                  //
        { 0xCD, { 0x63 }, 1 },                                  //
        // { 0x70, {0x07, 0x07, 0x04, 0x0E, 0x0F, 0x09, 0x07, 0x08, 0x03}, 9},     // see note
        { 0xE8, { 0x34 }, 1 },                                                                       //
        { 0x62, { 0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70 }, 12 },    //
        { 0x63, { 0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70 }, 12 },    //
        { 0x64, { 0x28, 0x29, 0xF1, 0x01, 0xF1, 0x00, 0x07 }, 7 },                                   //
        { 0x66, { 0x3C, 0x00, 0xCD, 0x67, 0x45, 0x45, 0x10, 0x00, 0x00, 0x00 }, 10 },                //
        { 0x67, { 0x00, 0x3C, 0x00, 0x00, 0x00, 0x01, 0x54, 0x10, 0x32, 0x98 }, 10 },                //
        { 0x74, { 0x10, 0x85, 0x80, 0x00, 0x00, 0x4E, 0x00 }, 7 },                                   //
        { 0x98, { 0x3e, 0x07 }, 2 },                                                                 //
        { 0x35, { 0x00 }, 0 },                                                                       //
        { 0x21, { 0x00 }, 0 },                                                                       //
        { 0x11, { 0x00 }, 0x80 },                                                                    //
        { 0x29, { 0x00 }, 0x80 },                                                                    //
        { 0x00, { 0x00 }, 0xff },                                                                    //
    };

    // note: Unsure what this line (from manufacturer's boilerplate code) is meant to do, but users reported issues, seems to work OK without:

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

    void spi_callback_dma_complete()
    {
        dma_buffer_sent = true;
    }

    //////////////////////////////////////////////////////////////////////

    /* Send a command to the LCD. Uses spi_device_polling_transmit, which waits
     * until the transfer is complete.
     *
     * Since command transactions are usually small, they are handled in polling
     * mode for higher speed. The overhead of interrupt transactions is more than
     * just waiting for the transaction to complete.
     */

    void lcd_cmd(const uint8_t cmd, bool keep_cs_active)
    {
        esp_err_t ret;
        spi_transaction_t t;
        memset(&t, 0, sizeof(t));      // Zero out the transaction
        t.length = 8;                  // Command is 8 bits
        t.tx_buffer = &cmd;            // The data is the cmd itself
        t.user = &spi_callback_cmd;    // D/C needs to be set to 0
        if(keep_cs_active) {
            t.flags = SPI_TRANS_CS_KEEP_ACTIVE;    // Keep CS active after data transfer
        }
        ret = spi_device_polling_transmit(spi, &t);    // Transmit!
        assert(ret == ESP_OK);                         // Should have had no issues.
    }

    //////////////////////////////////////////////////////////////////////

    /* Send data to the LCD. Uses spi_device_polling_transmit, which waits until the
     * transfer is complete.
     *
     * Since data transactions are usually small, they are handled in polling
     * mode for higher speed. The overhead of interrupt transactions is more than
     * just waiting for the transaction to complete.
     */

    void lcd_data(const uint8_t *data, int len)
    {
        esp_err_t ret;
        spi_transaction_t t;
        if(len == 0) {
            return;    // no need to send anything
        }
        memset(&t, 0, sizeof(t));                      // Zero out the transaction
        t.length = len * 8;                            // Len is in bytes, transaction length is in bits.
        t.tx_buffer = data;                            // Data
        t.user = &spi_callback_data;                   // D/C needs to be set to 1
        ret = spi_device_polling_transmit(spi, &t);    // Transmit!
        assert(ret == ESP_OK);                         // Should have had no issues.
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
    ESP_LOGI(TAG, "INIT");

    ESP_ERROR_CHECK(init_backlight_pwm());

    spi_bus_config_t buscfg = {};
    buscfg.miso_io_num = LCD_PIN_NUM_MISO;
    buscfg.mosi_io_num = LCD_PIN_NUM_MOSI;
    buscfg.sclk_io_num = LCD_PIN_NUM_CLK;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = LCD_BYTES_PER_LINE * LCD_SECTION_HEIGHT;

    spi_device_interface_config_t devcfg = {};
    devcfg.flags = SPI_DEVICE_NO_RETURN_RESULT;
    devcfg.clock_speed_hz = LCD_SPI_SPEED;
    devcfg.mode = 0;
    devcfg.spics_io_num = LCD_PIN_NUM_CS;
    devcfg.queue_size = num_transfers;
    devcfg.pre_cb = lcd_spi_pre_transfer_callback;
    devcfg.post_cb = lcd_spi_post_transfer_complete;

    // Initialize the SPI bus
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(spi_bus_add_device(LCD_HOST, &devcfg, &spi));

    // Initialize non-SPI GPIOs
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << LCD_PIN_NUM_DC) | (1ULL << LCD_PIN_NUM_RST);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    // Reset the display
    gpio_set_level(LCD_PIN_NUM_RST, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    gpio_set_level(LCD_PIN_NUM_RST, 1);
    vTaskDelay(100 / portTICK_PERIOD_MS);

    for(int cmd = 0; GC9A01A_initcmds[cmd].databytes != 0xff; ++cmd) {
        lcd_cmd(GC9A01A_initcmds[cmd].cmd, false);
        lcd_data(GC9A01A_initcmds[cmd].data, GC9A01A_initcmds[cmd].databytes & 0x1F);
        if((GC9A01A_initcmds[cmd].databytes & 0x80) != 0) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }

    spi_transaction_t *t = spi_transactions;
    memset(t, 0, sizeof(spi_transaction_t) * num_transfers);

    t[0].tx_data[0] = 0x2A;    // Column Address Set
    t[0].length = 8;
    t[0].user = &spi_callback_cmd;
    t[0].flags = SPI_TRANS_USE_TXDATA;

    t[1].tx_data[0] = 0;                         // start col High
    t[1].tx_data[1] = 0;                         // start col Low
    t[1].tx_data[2] = (LCD_WIDTH - 1) >> 8;      // end col High
    t[1].tx_data[3] = (LCD_WIDTH - 1) & 0xff;    // end col Low
    t[1].length = 8 * 4;
    t[1].user = &spi_callback_data;
    t[1].flags = SPI_TRANS_USE_TXDATA;

    t[2].tx_data[0] = 0x2B;    // Row address set
    t[2].length = 8;
    t[2].user = &spi_callback_cmd;
    t[2].flags = SPI_TRANS_USE_TXDATA;

    t[3].tx_data[0] = 0;                          // start row high
    t[3].tx_data[1] = 0;                          // start row low
    t[3].tx_data[2] = (LCD_HEIGHT - 1) >> 8;      // end row high
    t[3].tx_data[3] = (LCD_HEIGHT - 1) & 0xff;    // end row low
    t[3].length = 8 * 4;
    t[3].user = &spi_callback_data;
    t[3].flags = SPI_TRANS_USE_TXDATA;

    t[4].tx_data[0] = 0x2C;
    t[4].length = 8;
    t[4].user = &spi_callback_cmd;
    t[4].flags = SPI_TRANS_USE_TXDATA;

    int x = 0;
    for(int i = 5; i < num_transfers; i++) {

        t[i].tx_buffer = lcd_buffer[x];
        t[i].length = LCD_WIDTH * LCD_BITS_PER_PIXEL * LCD_SECTION_HEIGHT;
        t[i].user = &spi_callback_dma_data;
        t[i].flags = 0;
        x = 1 - x;
    }

    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

esp_err_t lcd_update()
{
    for(int i = 0; i < 5; ++i) {
        spi_device_queue_trans(spi, spi_transactions + i, portMAX_DELAY);
    }
    // now, then, yes, here we go
    int x = 0;
    int spin = 0;
    for(int i = 0; i < LCD_NUM_SECTIONS; ++i) {

        display_list_draw(i, lcd_buffer[x]);

        while(!dma_buffer_sent) {
            vTaskDelay(0);
        }

        dma_buffer_sent = false;
        spi_device_queue_trans(spi, spi_transactions + i + 5, portMAX_DELAY);
        x = 1 - x;
    }
    ESP_LOGV(TAG, "Waited %d", spin);
    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

esp_err_t lcd_set_backlight(uint32_t brightness_0_8191)
{
    ESP_ERROR_CHECK(ledc_set_duty(LCD_BL_MODE, LCD_BL_CHANNEL, brightness_0_8191));
    ESP_ERROR_CHECK(ledc_update_duty(LCD_BL_MODE, LCD_BL_CHANNEL));
    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

esp_err_t lcd_get_backbuffer(uint16_t **buffer, TickType_t wait_for_ticks)
{
    assert(buffer != nullptr);

    return ESP_FAIL;
}

//////////////////////////////////////////////////////////////////////

esp_err_t lcd_release_backbuffer()
{
    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

esp_err_t lcd_release_backbuffer_and_update()
{
    return lcd_update();
}
