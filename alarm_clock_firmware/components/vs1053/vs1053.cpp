//////////////////////////////////////////////////////////////////////

#include <memory.h>
#include <stdio.h>
#include <esp_log.h>
#include "util.h"
#include "vs1053.h"

LOG_TAG("vs1053");

#define SWITCH_TO_DECODE_MODE

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

#define GPIO_DIR 0xC017
#define GPIO_IN 0xC018
#define GPIO_OUT 0xC019

namespace
{
    // saved config (GPIOs and SPI device)

    vs1053_cfg_t config;

    // slow SPI device for SCI control stuf

    spi_device_handle_t spi_handle_control;

    // faster SPI device for data streaming

    // TODO (chs): use fast SPI for both and dealloc the slow one after increasing clock speed

    spi_device_handle_t spi_handle_stream;

    // this structure copied from the VS1053 datasheet

#define PARAMETRIC_VERSION 0x0003

#define PARAMETER_LOCATION 0x1e02

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
    // set CS low (active)

    void cs_low()
    {
        gpio_set_level(config.pin_num_cs, 0);
    }

    //////////////////////////////////////////////////////////////////////
    // set CS high (inactive)

    void cs_high()
    {
        gpio_set_level(config.pin_num_cs, 1);
    }

    //////////////////////////////////////////////////////////////////////

    // void dcs_low()
    // {
    //     gpio_set_level(config.pin_num_dcs, 0);
    // }

    // //////////////////////////////////////////////////////////////////////

    // void dcs_high()
    // {
    //     gpio_set_level(config.pin_num_dcs, 1);
    // }

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
        assert(result != nullptr);

        spi_transaction_t tx = {};
        tx.flags = SPI_TRANS_USE_RXDATA | SPI_TRANS_USE_TXDATA;
        tx.cmd = SCI_CMD_READ;
        tx.addr = reg_num;
        tx.length = 16;

        wait_for_dreq();

        cs_low();

        ESP_RETURN_IF_FAILED(spi_device_transmit(spi_handle_control, &tx));

        cs_high();

        *result = (tx.rx_data[0] << 8) | tx.rx_data[1];

        return ESP_OK;
    }

    //////////////////////////////////////////////////////////////////////
    // write to a register

    esp_err_t sci_write(uint8_t reg_num, uint16_t value)
    {
        spi_transaction_t tx = {};
        tx.flags = SPI_TRANS_USE_TXDATA;
        tx.cmd = SCI_CMD_WRITE;
        tx.addr = reg_num;
        tx.length = 16;
        tx.tx_data[0] = value >> 8;
        tx.tx_data[1] = value & 0xff;

        wait_for_dreq();

        cs_low();

        ESP_RETURN_IF_FAILED(spi_device_transmit(spi_handle_control, &tx));

        cs_high();

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
            ESP_LOGI(TAG, "verify failed for register %d: wrote %d (%04x) read back %d (%04x), mask = %04x", reg_num, value, value, check, check, mask);
            return ESP_FAIL;
        }
        return ESP_OK;
    }

    //////////////////////////////////////////////////////////////////////

    [[maybe_unused]] esp_err_t wram_read_value(uint16_t addr, uint16_t *value)
    {
        ESP_RETURN_IF_FAILED(sci_write(SCI_WRAMADDR, addr));
        ESP_RETURN_IF_FAILED(sci_read(SCI_WRAM, value));
        return ESP_OK;
    }

    //////////////////////////////////////////////////////////////////////

    esp_err_t wram_write_value(uint16_t addr, uint16_t value)
    {
        ESP_RETURN_IF_FAILED(sci_write(SCI_WRAMADDR, addr));
        ESP_RETURN_IF_FAILED(sci_write(SCI_WRAM, value));
        return ESP_OK;
    }

    //////////////////////////////////////////////////////////////////////

    [[maybe_unused]] esp_err_t wram_write_data(uint16_t addr, size_t length, uint16_t const *data)
    {
        for(; length != 0; --length) {
            ESP_RETURN_IF_FAILED(sci_write(SCI_WRAMADDR, addr++));
            ESP_RETURN_IF_FAILED(sci_write(SCI_WRAM, *data++));
        }
        return ESP_OK;
    }

    //////////////////////////////////////////////////////////////////////

    [[maybe_unused]] esp_err_t wram_read_data(uint16_t addr, size_t length, uint16_t *data)
    {
        for(; length != 0; --length) {
            ESP_RETURN_IF_FAILED(sci_write(SCI_WRAMADDR, addr++));
            ESP_RETURN_IF_FAILED(sci_read(SCI_WRAM, data++));
        }
        return ESP_OK;
    }

    //////////////////////////////////////////////////////////////////////

}    // namespace

//////////////////////////////////////////////////////////////////////

esp_err_t vs1053_init(vs1053_cfg_t const *cfg)
{
    ESP_LOGI(TAG, "init");

    // copy the config

    memcpy(&config, cfg, sizeof(config));

    // set up the GPIOs

    gpio_config_t gpio_conf = {};

    gpio_conf.mode = GPIO_MODE_INPUT;
    gpio_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    gpio_conf.pin_bit_mask = 1llu << config.pin_num_dreq;
    ESP_RETURN_IF_FAILED(gpio_config(&gpio_conf));

    gpio_conf.mode = GPIO_MODE_OUTPUT;
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
    devcfg.clock_speed_hz = 1000000;
    devcfg.address_bits = 8;
    devcfg.command_bits = 8;
    devcfg.mode = 0;
    devcfg.spics_io_num = -1;
    devcfg.queue_size = 1;

    ESP_RETURN_IF_FAILED(spi_bus_add_device(config.spi_host, &devcfg, &spi_handle_control));

    int spi_frequency;
    ESP_RETURN_IF_FAILED(spi_device_get_actual_freq(spi_handle_control, &spi_frequency));

    ESP_LOGI(TAG, "LOW speed SPI Frequency %d KHz", spi_frequency);

    // reset VS1052

    gpio_set_level(config.pin_num_reset, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(config.pin_num_reset, 1);

    // check version == 4

    int version;
    for(int i = 0; i < 10; ++i) {
        uint16_t status = 0;
        ESP_RETURN_IF_FAILED(sci_read(SCI_STATUS, &status));
        version = (status >> 4) & 0xf;
        ESP_LOGI(TAG, "Got version %d on try %d", version, i);
        if(version == 4) {
            break;
        }
    }

    if(version != 4) {
        ESP_LOGE(TAG, "1053 version should be 4, got %d", version);
        return ESP_FAIL;
    }

    // LC Breakout board needs this to enable audio decode mode

#if defined(SWITCH_TO_DECODE_MODE)

    // pull GPIO0 and GPIO1 low to enable decoder mode (as opposed to MIDI mode)
    // required if pins 33/34 are not pulled low on the board.
    // From the VS1053 datasheet: "If GPIO0 is low and GPIO1 is high during boot, real-time MIDI mode is activated"

    wram_write_value(GPIO_DIR, 3);
    wram_write_value(GPIO_OUT, 0);

    ESP_LOGI(TAG, "Resetting VS1053");

    vTaskDelay(pdMS_TO_TICKS(100));

    sci_write(SCI_MODE, SM_SDINEW | SM_RESET);

    vTaskDelay(pdMS_TO_TICKS(100));

    wait_for_dreq();

    ESP_LOGI(TAG, "VS1053 reset complete");

#endif

    // set clock to 49.152 MHz

    ESP_RETURN_IF_FAILED(sci_write_and_verify(SCI_CLOCKF, 0xA000, 0xffff));

    // set 44.1 KHz sample rate

    ESP_RETURN_IF_FAILED(sci_write_and_verify(SCI_AUDATA, 44100, 0xfffe));

    // create faster SPI device for streaming data - max SPI speed is CLKI (49.152 MHz) / 4 = 12.288 MHz, use 12

    spi_device_interface_config_t stream_spi_devcfg = {};
    stream_spi_devcfg.flags = SPI_DEVICE_NO_DUMMY;
    stream_spi_devcfg.clock_speed_hz = 12000000;
    stream_spi_devcfg.spics_io_num = -1;
    stream_spi_devcfg.queue_size = 1;

    ESP_RETURN_IF_FAILED(spi_bus_add_device(config.spi_host, &stream_spi_devcfg, &spi_handle_stream));

    ESP_RETURN_IF_FAILED(spi_device_get_actual_freq(spi_handle_stream, &spi_frequency));

    ESP_LOGI(TAG, "HIGH speed SPI Frequency %d KHz", spi_frequency);

    return ESP_OK;
}
