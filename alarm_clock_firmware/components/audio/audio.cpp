//////////////////////////////////////////////////////////////////////

// 1. Send an audio file to VS1053b.
// 2. Read extra parameter value endFillByte (Chapter 10.11).
// 3. Send at least 2052 bytes of endFillByte[7:0].
// 4. Set SCI_MODE bit SM_CANCEL.
// 5. Send at least 32 bytes of endFillByte[7:0].
// 6. Read SCI_MODE. If SM_CANCEL is still set, go to 5. If SM_CANCEL hasn’t cleared
// after sending 2048 bytes, do a software reset (this should be extremely rare).
// 7. The song has now been successfully sent. HDAT0 and HDAT1 should now both contain
// 0 to indicate that no format is being decoded. Return to 1.

#include <memory.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"
#include "portmacro.h"
#include <rom/ets_sys.h>
#include <esp_log.h>
#include "util.h"
#include "audio.h"

LOG_CONTEXT("audio");

#define AUDIO_LOG(...) LOG_I(__VA_ARGS__)
// #define AUDIO_LOG(...)

// #define SWITCH_TO_DECODE_MODE

// SCI Registers           Read/Write    Reset Value     Cycles  Clock   Description

#define SCI_MODE 0x0           // rw            0x4000->0x4800   80      CLKI    MODE Mode control
#define SCI_STATUS 0x1         // rw            0x0C->0x48->40   80      CLKI    STATUS Status of VS1053b
#define SCI_BASS 0x2           // rw            0                80      CLKI    BASS Built-in bass/treble control
#define SCI_CLOCKF 0x3         // rw            0                1200    XTALI   CLOCKF Clock freq + multiplier
#define SCI_DECODE_TIME 0x4    // rw            0                100     CLKI    DECODE_TIME Decode time in seconds
#define SCI_AUDATA 0x5         // rw            0                450     CLKI    AUDATA Misc. audio data
#define SCI_WRAM 0x6           // rw            0                100     CLKI    WRAM RAM write/read
#define SCI_WRAMADDR 0x7       // rw            0                100     CLKI    WRAMADDR Base address for RAM write/read
#define SCI_HDAT0 0x8          // r             0                80      CLKI    HDAT0 Stream header data 0
#define SCI_HDAT1 0x9          // r             0                80      CLKI    HDAT1 Stream header data 1
#define SCI_AIADDR 0xA         // rw            0                210     CLKI    AIADDR Start address of application
#define SCI_VOL 0xB            // rw            0                80      CLKI    VOL Volume control
#define SCI_AICTRL0 0xC        // rw            0                80      CLKI    AICTRL0 Application control register 0
#define SCI_AICTRL1 0xD        // rw            0                80      CLKI    AICTRL1 Application control register 1
#define SCI_AICTRL2 0xE        // rw            0                80      CLKI    AICTRL2 Application control register 2
#define SCI_AICTRL3 0xF        // rw            0                80      CLKI    AICTRL3 Application control register 3

#define SCI_NUM_REGISTERS 16

// SCI_MODE SM_BITS                     Function                          Description

#define SM_DIFF (1 << 0)             // Differential                    0 normal in-phase audio         1 left channel inverted
#define SM_LAYER12 (1 << 1)          // Allow MPEG layers I & II        0 no                            1 yes
#define SM_RESET (1 << 2)            // Soft reset                      0 no reset                      1 reset
#define SM_CANCEL (1 << 3)           // Cancel decoding current file    0 no                            1 yes
#define SM_EARSPEAKER_LO (1 << 4)    // EarSpeaker low setting          0 off                           1 active
#define SM_TESTS (1 << 5)            // Allow SDI tests                 0 not allowed                   1 allowed
#define SM_STREAM (1 << 6)           // Stream mode                     0 no                            1 yes
#define SM_EARSPEAKER_HI (1 << 7)    // EarSpeaker high setting         0 off                           1 active
#define SM_DACT (1 << 8)             // DCLK active edge                0 rising                        1 falling
#define SM_SDIORD (1 << 9)           // SDI bit order                   0 MSb first                     1 MSb last
#define SM_SDISHARE (1 << 10)        // Share SPI chip select           0 no                            1 yes
#define SM_SDINEW (1 << 11)          // VS1002 native SPI modes         0 no                            1 yes
#define SM_ADPCM (1 << 12)           // PCM/ADPCM recording active      0 no                            1 yes
#define SM_UNUSED (1 << 13)          // None                            0 right                         1 wrong
#define SM_LINE1 (1 << 14)           // MIC / LINE1 selector            0 MICP                          1 LINE1
#define SM_CLK_RANGE (1 << 15)       // Input clock range               0 12..13 MHz                    1 24..26 MHz

// SCI_STATUS bit fields

#define SS_DO_NOT_JUMP_POS 15     // Header in decode, do not fast forward/rewind
#define SS_SWING_POS 12           // Set swing to +0 dB, +0.5 dB, .., or +3.5 dB
#define SS_VCM_OVERLOAD_POS 11    // GBUF overload indicator ’1’ = overload
#define SS_VCM_DISABLE_POS 10     // GBUF overload detection ’1’ = disable
#define SS_VER_POS 4              // Version
#define SS_APDOWN2_POS 3          // Analog driver powerdown
#define SS_APDOWN1_POS 2          // Analog internal powerdown
#define SS_AD_CLOCK_POS 1         // AD clock select, ’0’ = 6 MHz, ’1’ = 3 MHz
#define SS_REFERENCE_SEL_POS 0    // Reference voltage selection, ’0’ = 1.23 V, ’1’ = 1.65 V

#define SS_DO_NOT_JUMP_MASK 1
#define SS_SWING_MASK 7
#define SS_VCM_OVERLOAD_MASK 1
#define SS_VCM_DISABLE_MASK 1
#define SS_VER_MASK 15
#define SS_APDOWN2_MASK 1
#define SS_APDOWN1_MASK 1
#define SS_AD_CLOCK_MASK 1
#define SS_REFERENCE_SEL_MASK 1

#define SS_DO_NOT_JUMP(x) (((x) >> SS_DO_NOT_JUMP_POS) & SS_DO_NOT_JUMP_MASK)
#define SS_SWING(x) (((x) >> SS_SWING_POS) & SS_SWING_MASK)
#define SS_VCM_OVERLOAD(x) (((x) >> SS_VCM_OVERLOAD_POS) & SS_VCM_OVERLOAD_MASK)
#define SS_VCM_DISABLE(x) (((x) >> SS_VCM_DISABLE_POS) & SS_VCM_DISABLE_MASK)
#define SS_VER(x) (((x) >> SS_VER_POS) & SS_VER_MASK)
#define SS_APDOWN2(x) (((x) >> SS_APDOWN2_POS) & SS_APDOWN2_MASK)
#define SS_APDOWN1(x) (((x) >> SS_APDOWN1_POS) & SS_APDOWN1_MASK)
#define SS_AD_CLOCK(x) (((x) >> SS_AD_CLOCK_POS) & SS_AD_CLOCK_MASK)
#define SS_REFERENCE_SEL(x) (((x) >> SS_REFERENCE_SEL_POS) & SS_REFERENCE_SEL_MASK)

#define SCI_CMD_WRITE 2
#define SCI_CMD_READ 3

#define WRAM_GPIO_DIR 0xC017
#define WRAM_GPIO_IN 0xC018
#define WRAM_GPIO_OUT 0xC019

#define WRAM_MONO_MODE 0x1e09    // mono mode requires the plugin to be loaded

#define CLEAR_BITS 1
#define LEAVE_BITS 0

#define WAIT_FOR_ALL 1
#define WAIT_FOR_ANY 0

#define SPI_EVENT_BIT_COMPLETE 1
#define SPI_EVENT_BIT_READ_COMPLETE 2

#define AUDIO_EVENT_BIT_RING_BUFFER_EMPTY (1 << 0)
#define AUDIO_EVENT_BIT_ENDED (1 << 1)
#define AUDIO_EVENT_BIT_INITIALIZED (1 << 2)

namespace
{
    //////////////////////////////////////////////////////////////////////

    typedef struct vs1053_cfg_t
    {
        spi_host_device_t spi_host;
        gpio_num_t pin_num_sclk;
        gpio_num_t pin_num_miso;
        gpio_num_t pin_num_mosi;
        gpio_num_t pin_num_cs;
        gpio_num_t pin_num_dcs;
        gpio_num_t pin_num_dreq;
        gpio_num_t pin_num_reset;
    } vs1053_cfg_t;

    // saved config (GPIOs and SPI device)

    vs1053_cfg_t config;

    // current SPI device

    spi_device_handle_t spi_device;

    // slow SPI device for SCI control stuf

    spi_device_handle_t slow_spi_device;

    // faster SPI device for data streaming

    spi_device_handle_t fast_spi_device;

    // task which streams data to the vs1053

    TaskHandle_t audio_player_task_handle;

    // event group for signalling spi event completions

    EventGroupHandle_t spi_event_group_handle;

    // event group for signalling audio events

    EventGroupHandle_t audio_event_group_handle;

    // queue for sending audio messages to the player

    QueueHandle_t audio_command_queue;

    // ring buffer for compressed audio data

    RingbufHandle_t ring_buffer;

    // filler byte got from the vs1053

    uint8_t fill_byte;

    // how many end_fill_bytes to send

    size_t end_bytes_sent;

    // chunk of compressed data we got from the ring buffer

    uint8_t *current_chunk;

    // current playback details

    uint8_t *playback_ptr;
    size_t playback_remain;

    // which dma buffer to use next

    int dma_buffer_id = 0;

    // dma buffers for sending compressed audio

    DMA_ATTR uint8_t dma_buffer[2][32];

    // types of message to send to the audio task

    typedef enum
    {
        amc_null = 0,
        amc_volume,
        amc_play,
        amc_stop,
        amc_pause

    } audio_message_code_t;

    //////////////////////////////////////////////////////////////////////

    typedef enum audio_state
    {
        audio_state_idle = 0,
        audio_state_playing = 1,
        audio_state_paused = 2,
        audio_state_ending = 3,

    } audio_state_t;

    char const *audio_state_name[4] = { "idle", "playing", "paused", "ending" };

    audio_state_t audio_current_state;

    //////////////////////////////////////////////////////////////////////

    // audio task message

    typedef struct audio_message
    {
        uint8_t code;

        union
        {
            uint8_t volume;
        };

    } audio_message_t;

    // spi callback admin

    struct spi_callback_user_data_s
    {
        typedef void (*spi_callback)(void);

        spi_callback pre_callback;
        spi_callback post_callback;
    };

    typedef struct spi_callback_user_data_s spi_callback_user_data_t;

    IRAM_ATTR void cs_low();
    IRAM_ATTR void cs_high_read();
    IRAM_ATTR void cs_high_write();

    IRAM_ATTR void dcs_low();
    IRAM_ATTR void dcs_high();

    spi_callback_user_data_t spi_callback_cmd_read = { .pre_callback = cs_low, .post_callback = cs_high_read };

    spi_callback_user_data_t spi_callback_cmd_write = { .pre_callback = cs_low, .post_callback = cs_high_write };

    spi_callback_user_data_t spi_callback_data = { .pre_callback = dcs_low, .post_callback = dcs_high };

    // this structure copied from the VS1053 datasheet

#define PARAMETER_VERSION 0x0003

#define PARAMETER_LOCATION_STRUCT 0x1e02
#define PARAMETER_LOCATION_VERSION 0x1e02
#define PARAMETER_LOCATION_END_FILL_BYTE 0x1e06

    struct vs1053_parameters
    {
        /* configs are not cleared between files */
        uint16_t version;       /*1e02 - structure version */
        uint16_t config1;       /*1e03 ---- ---- ppss RRRR PS mode, SBR mode, Reverb */
        uint16_t playSpeed;     /*1e04 0,1 = normal speed, 2 = twice, 3 = three times etc. */
        uint16_t byteRate;      /*1e05 average byterate */
        uint16_t endFillByte;   /*1e06 byte value to send after file sent */
        uint16_t reserved[16];  /*1e07..15 file byte offsets */
        uint32_t jumpPoints[8]; /*1e16..25 file byte offsets */
        uint16_t latestJump;    /*1e26 index to lastly updated jumpPoint */
        uint32_t positionMsec;  /*1e27-28 play position, if known (WMA, Ogg Vorbis) */
        int16_t resync;         /*1e29 > 0 for automatic m4a, ADIF, WMA resyncs */
        union
        {
            struct
            {
                uint32_t curPacketSize;
                uint32_t packetSize;
            } wma;
            struct
            {
                uint16_t sceFoundMask;   /*1e2a SCE's found since last clear */
                uint16_t cpeFoundMask;   /*1e2b CPE's found since last clear */
                uint16_t lfeFoundMask;   /*1e2c LFE's found since last clear */
                uint16_t playSelect;     /*1e2d 0 = first any, initialized at aac init */
                int16_t dynCompress;     /*1e2e -8192=1.0, initialized at aac init */
                int16_t dynBoost;        /*1e2f 8192=1.0, initialized at aac init */
                uint16_t sbrAndPsStatus; /*0x1e30 1=SBR, 2=upsample, 4=PS, 8=PS active */
            } aac;
            struct
            {
                uint32_t bytesLeft;
            } midi;
            struct
            {
                int16_t gain; /* 0x1e2a proposed gain offset in 0.5dB steps, default = -12 */
            } vorbis;
        } i;
    };

    //////////////////////////////////////////////////////////////////////

    void set_audio_state(audio_state_t new_state)
    {
        LOG_I("Set audio state to %d (%s)", new_state, audio_state_name[new_state]);
        audio_current_state = new_state;
    }

    // vs1053_parameters parameters;

    //////////////////////////////////////////////////////////////////////
    // VS1053b plugin data

#define SKIP_PLUGIN_VARNAME
    uint16_t const plugin_data[] = {
#include "vs1053b-patches.plg.inc"
    };

    //////////////////////////////////////////////////////////////////////
    // set CS low (active)

    void cs_low()
    {
        gpio_set_level(config.pin_num_cs, 0);
    }

    //////////////////////////////////////////////////////////////////////
    // set CS high (inactive)

    void cs_high_write()
    {
        gpio_set_level(config.pin_num_cs, 1);

        BaseType_t woken = pdFALSE;
        xEventGroupSetBitsFromISR(spi_event_group_handle, SPI_EVENT_BIT_COMPLETE, &woken);
        portYIELD_FROM_ISR(woken);
    }

    //////////////////////////////////////////////////////////////////////
    // set CS high (inactive)

    void cs_high_read()
    {
        gpio_set_level(config.pin_num_cs, 1);

        BaseType_t woken = pdFALSE;
        xEventGroupSetBitsFromISR(spi_event_group_handle, SPI_EVENT_BIT_COMPLETE | SPI_EVENT_BIT_READ_COMPLETE, &woken);
        portYIELD_FROM_ISR(woken);
    }

    //////////////////////////////////////////////////////////////////////

    void dcs_low()
    {
        gpio_set_level(config.pin_num_dcs, 0);
    }

    //////////////////////////////////////////////////////////////////////

    void dcs_high()
    {
        gpio_set_level(config.pin_num_dcs, 1);

        BaseType_t woken = 0;
        xEventGroupSetBitsFromISR(spi_event_group_handle, SPI_EVENT_BIT_COMPLETE, &woken);
        portYIELD_FROM_ISR(woken);
    }

    //////////////////////////////////////////////////////////////////////

    void spi_pre_transfer_callback(spi_transaction_t *t)
    {
        spi_callback_user_data_t *p = (spi_callback_user_data_t *)t->user;
        if(p != nullptr && p->pre_callback != nullptr) {
            p->pre_callback();
        }
    }

    //////////////////////////////////////////////////////////////////////

    IRAM_ATTR void spi_post_transfer_complete(spi_transaction_t *t)
    {
        spi_callback_user_data_t *p = (spi_callback_user_data_t *)t->user;
        if(p != nullptr && p->post_callback != nullptr) {
            p->post_callback();
        }
    }

    //////////////////////////////////////////////////////////////////////
    // wait for DREQ to go high

    void wait_for_dreq()
    {
        while(gpio_get_level(config.pin_num_dreq) == 0) {
            taskYIELD();
        }
    }

    //////////////////////////////////////////////////////////////////////
    // read from a register

    esp_err_t sci_read(uint8_t reg_num, uint16_t *result)
    {
        static spi_transaction_ext_t tx = {};

        assert(result != nullptr);

        xEventGroupWaitBits(spi_event_group_handle, SPI_EVENT_BIT_COMPLETE, CLEAR_BITS, WAIT_FOR_ALL, portMAX_DELAY);

        tx.base.flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_DUMMY;
        tx.address_bits = 8;
        tx.command_bits = 8;
        tx.base.cmd = SCI_CMD_READ;
        tx.base.addr = reg_num;
        tx.base.length = 16;
        tx.base.user = &spi_callback_cmd_read;

        wait_for_dreq();

        ESP_RETURN_IF_FAILED(spi_device_queue_trans(spi_device, &tx.base, portMAX_DELAY));

        // wait for read to complete. SPI_EVENT_BIT_COMPLETE is left in the set state

        xEventGroupWaitBits(spi_event_group_handle, SPI_EVENT_BIT_READ_COMPLETE, CLEAR_BITS, WAIT_FOR_ALL, portMAX_DELAY);

        *result = (tx.base.rx_data[0] << 8) | tx.base.rx_data[1];

        AUDIO_LOG("SCI_READ[%d] = 0x%04x", reg_num, *result);

        return ESP_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // write to a register

    esp_err_t sci_write(uint8_t reg_num, uint16_t value)
    {
        static spi_transaction_ext_t tx = {};

        xEventGroupWaitBits(spi_event_group_handle, SPI_EVENT_BIT_COMPLETE, CLEAR_BITS, WAIT_FOR_ALL, portMAX_DELAY);

        tx.base.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_DUMMY;
        tx.address_bits = 8;
        tx.command_bits = 8;
        tx.dummy_bits = 0;
        tx.base.cmd = SCI_CMD_WRITE;
        tx.base.addr = reg_num;
        tx.base.length = 16;
        tx.base.tx_data[0] = value >> 8;
        tx.base.tx_data[1] = value & 0xff;
        tx.base.user = &spi_callback_cmd_write;

        wait_for_dreq();

        ESP_RETURN_IF_FAILED(spi_device_queue_trans(spi_device, &tx.base, portMAX_DELAY));

        return ESP_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // write to a register and check it was written

    esp_err_t sci_write_and_verify(uint8_t reg_num, uint16_t value, uint16_t mask)
    {
        ESP_RETURN_IF_FAILED(sci_write(reg_num, value));
        uint16_t check;
        ESP_RETURN_IF_FAILED(sci_read(reg_num, &check));
        if((value & mask) != (check & mask)) {
            LOG_E("verify failed for register %d: wrote %d (%04x) read back %d (%04x), mask = %04x", reg_num, value, value, check, check, mask);
            return ESP_FAIL;
        }
        return ESP_OK;
    }

    //////////////////////////////////////////////////////////////////////

    esp_err_t wram_read_data(uint16_t addr, uint16_t *buffer, size_t length)
    {
        ESP_RETURN_IF_FAILED(sci_write(SCI_WRAMADDR, addr));
        for(size_t i = 0; i < length; ++i) {
            ESP_RETURN_IF_FAILED(sci_read(SCI_WRAM, buffer++));
        }
        return ESP_OK;
    }

    //////////////////////////////////////////////////////////////////////

    esp_err_t wram_read_value(uint16_t addr, uint16_t *value)
    {
        return wram_read_data(addr, value, 1);
    }

    //////////////////////////////////////////////////////////////////////

    esp_err_t wram_write(uint16_t addr, uint16_t value)
    {
        AUDIO_LOG("WRAM WRITE %04x = %04x", addr, value);
        ESP_RETURN_IF_FAILED(sci_write(SCI_WRAMADDR, addr));
        ESP_RETURN_IF_FAILED(sci_write(SCI_WRAM, value));
        return ESP_OK;
    }

    //////////////////////////////////////////////////////////////////////

    esp_err_t load_plugin()
    {
        AUDIO_LOG("load plugin");
        int i = 0;
        while(i < PLUGIN_SIZE) {
            uint16_t addr = plugin_data[i++];
            uint16_t n = plugin_data[i++];
            if((n & 0x8000U) != 0) {
                n &= 0x7FFF;
                uint16_t val = plugin_data[i++];
                while(n-- != 0) {
                    ESP_RETURN_IF_FAILED(sci_write(addr, val));
                }
            } else {
                while(n-- != 0) {
                    uint16_t val = plugin_data[i++];
                    ESP_RETURN_IF_FAILED(sci_write(addr, val));
                }
            }
        }
        AUDIO_LOG("plugin load complete");

        wait_for_dreq();

        return ESP_OK;
    }

    //////////////////////////////////////////////////////////////////////

    esp_err_t get_fill_byte()
    {
        uint16_t fill_word;
        ESP_RETURN_IF_FAILED(wram_read_value(PARAMETER_LOCATION_END_FILL_BYTE, &fill_word));
        fill_byte = fill_word & 0xff;
        AUDIO_LOG("Fill word: %04x", fill_word);
        return ESP_OK;
    }

    //////////////////////////////////////////////////////////////////////

    esp_err_t startup()
    {
        LOG_I("startup");

        ESP_RETURN_IF_NULL(spi_event_group_handle = xEventGroupCreate());

        xEventGroupSetBits(spi_event_group_handle, SPI_EVENT_BIT_COMPLETE);

        // set up the GPIOs

        gpio_config_t gpio_conf = {};

        gpio_conf.mode = GPIO_MODE_INPUT;
        gpio_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
        gpio_conf.pin_bit_mask = 1llu << config.pin_num_dreq;
        gpio_conf.intr_type = GPIO_INTR_POSEDGE;
        ESP_RETURN_IF_FAILED(gpio_config(&gpio_conf));

        gpio_conf.mode = GPIO_MODE_OUTPUT;
        gpio_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        gpio_conf.pin_bit_mask = 1llu << config.pin_num_dcs;
        ESP_RETURN_IF_FAILED(gpio_config(&gpio_conf));

        gpio_conf.mode = GPIO_MODE_OUTPUT;
        gpio_conf.pin_bit_mask = 1llu << config.pin_num_cs;
        ESP_RETURN_IF_FAILED(gpio_config(&gpio_conf));

        gpio_conf.mode = GPIO_MODE_OUTPUT;
        gpio_conf.pin_bit_mask = 1llu << config.pin_num_reset;
        ESP_RETURN_IF_FAILED(gpio_config(&gpio_conf));

        // set CS, DCS inactive

        gpio_set_level(config.pin_num_cs, 1);
        gpio_set_level(config.pin_num_dcs, 1);

        // SPI bus init

        spi_bus_config_t buscfg = {};
        buscfg.flags = SPICOMMON_BUSFLAG_MASTER | SPICOMMON_BUSFLAG_GPIO_PINS;
        buscfg.mosi_io_num = config.pin_num_mosi;
        buscfg.miso_io_num = config.pin_num_miso;
        buscfg.sclk_io_num = config.pin_num_sclk;
        buscfg.quadwp_io_num = -1;
        buscfg.quadhd_io_num = -1;
        buscfg.max_transfer_sz = 128;

        ESP_RETURN_IF_FAILED(spi_bus_initialize(config.spi_host, &buscfg, SPI_DMA_CH_AUTO));

        // low speed SPI device init

        spi_device_interface_config_t devcfg = {};
        devcfg.flags = SPI_DEVICE_NO_DUMMY;
        devcfg.clock_speed_hz = 2000000;
        devcfg.mode = 0;
        devcfg.spics_io_num = -1;
        devcfg.queue_size = 1;
        devcfg.post_cb = spi_post_transfer_complete;
        devcfg.pre_cb = spi_pre_transfer_callback;

        ESP_RETURN_IF_FAILED(spi_bus_add_device(config.spi_host, &devcfg, &slow_spi_device));

        spi_device = slow_spi_device;

        int low_speed_spi_frequency;
        ESP_RETURN_IF_FAILED(spi_device_get_actual_freq(spi_device, &low_speed_spi_frequency));
        AUDIO_LOG("LOW speed SPI Frequency %d KHz", low_speed_spi_frequency);

        // hard reset VS1053

        gpio_set_level(config.pin_num_reset, 0);

        for(int i = 500; i != 0; --i) {
            __asm__ __volatile__("nop");
        }

        gpio_set_level(config.pin_num_reset, 1);

        for(int i = 500; i != 0; --i) {
            __asm__ __volatile__("nop");
        }

        // check version == 4

        AUDIO_LOG("Check version");

        int version;
        for(int i = 0; i < 10; ++i) {
            uint16_t status = 0;
            ESP_RETURN_IF_FAILED(sci_read(SCI_STATUS, &status));
            version = SS_VER(status);
            AUDIO_LOG("Got version %d on try %d", version, i);
            if(version == 4) {
                break;
            }
        }

        if(version != 4) {
            LOG_E("1053 version should be 4, got %d", version);
            return ESP_FAIL;
        }

        LOG_I("Audio version = %d", version);

#if defined(SWITCH_TO_DECODE_MODE)

        AUDIO_LOG("Switch to decode mode");

        wram_write(WRAM_GPIO_DIR, 3);
        wram_write(WRAM_GPIO_OUT, 0);

        // pull GPIO0 and GPIO1 low to enable decoder mode (as opposed to MIDI mode)
        // required if pins 33/34 are not pulled low on the board.
        // From the VS1053 datasheet: "If GPIO0 is low and GPIO1 is high during boot, real-time MIDI mode is activated"

        AUDIO_LOG("Resetting VS1053");

        vTaskDelay(pdMS_TO_TICKS(100));

        // loading the plugin will do a software reset so this is unnecessary and in fact
        // causes problems because we want the WRAM_GPIO_XXX values to be retained

        // sci_write(SCI_MODE, SM_SDINEW | SM_RESET);

        // vTaskDelay(pdMS_TO_TICKS(100));

        // wait_for_dreq();

        // AUDIO_LOG("VS1053 reset complete");

#endif

        // load the plugin before doing anything else, it resets a bunch of stuff

        ESP_RETURN_IF_FAILED(load_plugin());

        // set clock to 49.152 MHz

        ESP_RETURN_IF_FAILED(sci_write_and_verify(SCI_CLOCKF, 0x8800, 0xffff));

        // LC Breakout board needs this to enable audio decode mode

        AUDIO_LOG("set default volume");

        ESP_RETURN_IF_FAILED(sci_write_and_verify(SCI_VOL, 0x4040, 0xffff));

        // set 44.1 KHz sample rate

        ESP_RETURN_IF_FAILED(sci_write_and_verify(SCI_AUDATA, 44100, 0xfffe));

        // activate mono mode (requires plugin)

        ESP_RETURN_IF_FAILED(wram_write(WRAM_MONO_MODE, 0x0001));

        // create faster SPI device now that the clock speed is increased
        // max SPI speed is CLKI (49.152 MHz) / 4 = 12.288 MHz, use 12

        spi_device_interface_config_t stream_spi_devcfg = {};
        stream_spi_devcfg.flags = SPI_DEVICE_NO_DUMMY;
        stream_spi_devcfg.clock_speed_hz = 8000000;
        stream_spi_devcfg.spics_io_num = -1;
        stream_spi_devcfg.queue_size = 1;
        stream_spi_devcfg.post_cb = spi_post_transfer_complete;
        stream_spi_devcfg.pre_cb = spi_pre_transfer_callback;

        ESP_RETURN_IF_FAILED(spi_bus_add_device(config.spi_host, &stream_spi_devcfg, &fast_spi_device));

        int high_speed_spi_frequency;
        ESP_RETURN_IF_FAILED(spi_device_get_actual_freq(fast_spi_device, &high_speed_spi_frequency));
        AUDIO_LOG("HIGH speed SPI Frequency %d KHz", high_speed_spi_frequency);

        spi_device = fast_spi_device;

        // create the queue which is used to send commands to the audio task

        ESP_RETURN_IF_NULL(audio_command_queue = xQueueCreate(16, sizeof(audio_message_t)));

        // create the ring buffer which is used to send compressed audio data to the audio task

        ESP_RETURN_IF_NULL(ring_buffer = xRingbufferCreate(65536, RINGBUF_TYPE_NOSPLIT));

        return ESP_OK;
    }

    //////////////////////////////////////////////////////////////////////

    esp_err_t send_buffer(uint8_t const *buffer, size_t length_bytes)
    {
        static spi_transaction_ext_t dma_tx = {};

        dma_tx.base.flags = SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_DUMMY;
        dma_tx.base.tx_buffer = buffer;
        dma_tx.base.rxlength = 0;
        dma_tx.base.length = length_bytes * 8;
        dma_tx.base.user = &spi_callback_data;

        xEventGroupWaitBits(spi_event_group_handle, SPI_EVENT_BIT_COMPLETE, CLEAR_BITS, WAIT_FOR_ALL, portMAX_DELAY);

        wait_for_dreq();

        ESP_RETURN_IF_FAILED(spi_device_queue_trans(spi_device, &dma_tx.base, portMAX_DELAY));
        return ESP_OK;
    }

    //////////////////////////////////////////////////////////////////////

    void stop_decoding()
    {
        AUDIO_LOG("Stopping...");

        get_fill_byte();
        end_bytes_sent = 0;
        playback_ptr = nullptr;
        playback_remain = 0;
        set_audio_state(audio_state_ending);
    }

    //////////////////////////////////////////////////////////////////////

    void audio_player(void *)
    {
        audio_event_group_handle = xEventGroupCreate();

        if(audio_event_group_handle == nullptr) {
            LOG_E("ERROR Creaing audio event group handle!?");
            vTaskDelete(nullptr);
        }

        esp_err_t ret = startup();

        xEventGroupSetBits(audio_event_group_handle, AUDIO_EVENT_BIT_INITIALIZED);

        if(ret != ESP_OK) {
            vTaskDelete(nullptr);
        }

        // main audio task loop

        AUDIO_LOG("MAIN LOOP");

        while(true) {

            bool busy = false;

            audio_message_t msg = { .code = amc_null, .volume = 0 };

            if(xQueueReceive(audio_command_queue, &msg, 0)) {

                busy = true;

                LOG_V("got msg %d", msg.code);

                switch(msg.code) {

                case amc_play: {

                    set_audio_state(audio_state_playing);
                    uint16_t status;
                    sci_read(SCI_STATUS, &status);
                    AUDIO_LOG("PLAY: STATUS = 0x%04x", status);
                } break;

                case amc_stop: {
                    if(audio_current_state == audio_state_playing) {
                        stop_decoding();
                    }
                } break;

                case amc_volume: {

                    LOG_I("Set volume to %d", msg.volume);
                    sci_write(SCI_VOL, (msg.volume << 8) | msg.volume);
                } break;

                case amc_pause: {

                    if(audio_current_state == audio_state_playing) {
                        set_audio_state(audio_state_paused);
                    }
                } break;

                default:

                    LOG_E("BAD audio msg code: %d", msg.code);
                    break;
                }
            }

            switch(audio_current_state) {

            case audio_state_playing: {

                busy = true;

                if(playback_remain == 0) {

                    current_chunk = reinterpret_cast<uint8_t *>(xRingbufferReceive(ring_buffer, &playback_remain, 0));

                    playback_ptr = current_chunk;

                    if(playback_ptr != nullptr) {
                        busy = true;
                        xEventGroupClearBits(audio_event_group_handle, AUDIO_EVENT_BIT_RING_BUFFER_EMPTY);
                    } else {
                        xEventGroupSetBits(audio_event_group_handle, AUDIO_EVENT_BIT_RING_BUFFER_EMPTY);
                    }
                }

                if(playback_ptr != nullptr && playback_remain != 0) {

                    size_t fragment_length = min(32u, playback_remain);

                    uint8_t *buffer = dma_buffer[dma_buffer_id];
                    memcpy(buffer, playback_ptr, fragment_length);
                    send_buffer(buffer, fragment_length);

                    dma_buffer_id = 1 - dma_buffer_id;

                    playback_ptr += fragment_length;
                    playback_remain -= fragment_length;

                    if(playback_remain == 0) {
                        vRingbufferReturnItem(ring_buffer, current_chunk);
                        current_chunk = nullptr;
                    }
                }
            } break;

            case audio_state_ending: {

                busy = true;
                uint8_t *buffer = dma_buffer[dma_buffer_id];
                memset(buffer, fill_byte, 32);
                send_buffer(buffer, 32);
                dma_buffer_id = 1 - dma_buffer_id;

                end_bytes_sent += 32;    // track how much end_fill_byte has been sent

                if(end_bytes_sent >= (2052 + 32 + 2048)) {    // apparently this condition is 'very rare'

                    AUDIO_LOG("********** SOFTWARE RESET THE VS1052 BECAUSE VERY RARE EVENT! **********");
                    sci_write(SCI_AIADDR, 0x50);
                    vTaskDelay(pdMS_TO_TICKS(100));
                    xEventGroupSetBits(audio_event_group_handle, AUDIO_EVENT_BIT_ENDED);
                    set_audio_state(audio_state_idle);

                } else {

                    uint16_t mode;
                    sci_read(SCI_MODE, &mode);

                    if(end_bytes_sent >= (2052 + 64)) {    // if cancel was set, keep going until it's cleared by the vs1053

                        if((mode & SM_CANCEL) == 0) {
                            AUDIO_LOG("finished ending");
                            playback_ptr = nullptr;
                            set_audio_state(audio_state_idle);
                            xEventGroupSetBits(audio_event_group_handle, AUDIO_EVENT_BIT_ENDED);
                        }

                    } else if(end_bytes_sent >= 2052 + 32) {    // cancel not yet set, have we sent enough to set it?

                        AUDIO_LOG("Set CANCEL");
                        mode |= SM_CANCEL | SM_SDINEW;    // seems you must set SDINEW whenever you write to SCI_MODE
                        sci_write(SCI_MODE, mode);
                    }
                }
            } break;

            default:
                break;
            }

            if(!busy) {
                portYIELD();
            }
        }
    }

    //////////////////////////////////////////////////////////////////////

    esp_err_t send_audio_msg(audio_message const &msg)
    {
        if(audio_command_queue == nullptr) {
            return ESP_ERR_INVALID_STATE;
        }
        if(!xQueueSend(audio_command_queue, &msg, portMAX_DELAY)) {
            LOG_E("audio_set_volume: xQueueSend failed");
            return ESP_FAIL;
        }
        return ESP_OK;
    }

    //////////////////////////////////////////////////////////////////////

}    // namespace

//////////////////////////////////////////////////////////////////////

esp_err_t audio_init()
{
    LOG_I("init");

    // config

    config.pin_num_cs = GPIO_NUM_21;
    config.pin_num_dcs = GPIO_NUM_38;
    config.pin_num_dreq = GPIO_NUM_47;
    config.pin_num_miso = GPIO_NUM_41;
    config.pin_num_mosi = GPIO_NUM_48;
    config.pin_num_reset = GPIO_NUM_39;
    config.pin_num_sclk = GPIO_NUM_40;
    config.spi_host = SPI3_HOST;

    // kick off the audio task

    BaseType_t r = xTaskCreatePinnedToCore(audio_player, "audio", 4096, nullptr, 5, &audio_player_task_handle, 1);
    if(r != pdPASS) {
        LOG_E("xTaskCreate failed: returned %d", r);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

esp_err_t audio_set_volume(uint8_t volume)
{
    LOG_V("audio_set_volume %d", volume);
    audio_message_t msg = {};
    msg.code = amc_volume;
    msg.volume = volume;
    return send_audio_msg(msg);
}

//////////////////////////////////////////////////////////////////////

esp_err_t audio_play()
{
    LOG_I("audio_play");
    audio_message_t msg = {};
    msg.code = amc_play;
    return send_audio_msg(msg);
}

//////////////////////////////////////////////////////////////////////

esp_err_t audio_stop()
{
    LOG_I("audio_stop");
    audio_message_t msg = {};
    msg.code = amc_stop;
    return send_audio_msg(msg);
}

//////////////////////////////////////////////////////////////////////

esp_err_t audio_acquire_buffer(size_t required, uint8_t **ptr, TickType_t ticks_to_wait)
{
    if(ring_buffer == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if(xRingbufferSendAcquire(ring_buffer, reinterpret_cast<void **>(ptr), required, ticks_to_wait) == pdFALSE) {
        LOG_E("can't acquire %u bytes", required);
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

esp_err_t audio_send_buffer(uint8_t *ptr)
{
    if(ring_buffer == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    if(xRingbufferSendComplete(ring_buffer, ptr) != pdTRUE) {
        return ESP_FAIL;
    }
    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

esp_err_t audio_wait_for_send_complete(TickType_t wait_for_ticks)
{
    if(audio_event_group_handle == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    xEventGroupWaitBits(audio_event_group_handle, AUDIO_EVENT_BIT_RING_BUFFER_EMPTY, pdTRUE, pdTRUE, wait_for_ticks);
    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

esp_err_t audio_wait_for_sound_complete(TickType_t wait_for_ticks)
{
    if(audio_event_group_handle == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    xEventGroupWaitBits(audio_event_group_handle, AUDIO_EVENT_BIT_ENDED, pdTRUE, pdTRUE, wait_for_ticks);
    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

esp_err_t audio_wait_for_initialization_complete(TickType_t wait_for_ticks)
{
    if(audio_event_group_handle == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    xEventGroupWaitBits(audio_event_group_handle, AUDIO_EVENT_BIT_INITIALIZED, LEAVE_BITS, WAIT_FOR_ALL, wait_for_ticks);
    return ESP_OK;
}
