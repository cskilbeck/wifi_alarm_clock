//////////////////////////////////////////////////////////////////////

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_peripherals.h"
#include "esp_decoder.h"

#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_mem.h"
#include "audio_common.h"
#include "audio_alc.h"

#include "i2s_stream.h"
#include "http_stream.h"
#include "mp3_decoder.h"

#include "periph_touch.h"
#include "periph_adc_button.h"
#include "periph_button.h"
#include "periph_mywifi.h"
#include "periph_encoder.h"

#include "board.h"

#include "led.h"
#include "lcd_gc9a01.h"
#include "image.h"
#include "font.h"
#include "stream.h"
#include "user_main.h"
#include "assets.h"

static char const *TAG = "stream";

//////////////////////////////////////////////////////////////////////

static TaskHandle_t stream_task_handle;

static void stream_task(void *)
{
    audio_board_handle_t board_handle = audio_board_init();
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    audio_pipeline_handle_t pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    http_stream_cfg_t http_cfg = HTTP_STREAM_CFG_DEFAULT();
    http_cfg.enable_playlist_parser = true;
    http_cfg.auto_connect_next_track = true;
    audio_element_handle_t http_stream_reader = http_stream_init(&http_cfg);
    audio_pipeline_register(pipeline, http_stream_reader, "http");

    audio_decoder_t auto_decode[] = {
        DEFAULT_ESP_WAV_DECODER_CONFIG(),      //
        DEFAULT_ESP_MP3_DECODER_CONFIG(),      //
        DEFAULT_ESP_AMRNB_DECODER_CONFIG(),    //
        DEFAULT_ESP_AMRWB_DECODER_CONFIG(),    //
        DEFAULT_ESP_AAC_DECODER_CONFIG(),      //
        DEFAULT_ESP_M4A_DECODER_CONFIG(),      //
        DEFAULT_ESP_TS_DECODER_CONFIG(),       //
        DEFAULT_ESP_OGG_DECODER_CONFIG(),      //
        DEFAULT_ESP_OPUS_DECODER_CONFIG(),     //
        DEFAULT_ESP_FLAC_DECODER_CONFIG(),     //
        DEFAULT_ESP_PCM_DECODER_CONFIG(),      //
    };

    esp_decoder_cfg_t auto_dec_cfg = DEFAULT_ESP_DECODER_CONFIG();
    audio_element_handle_t audio_decoder = esp_decoder_init(&auto_dec_cfg, auto_decode, 11);
    audio_pipeline_register(pipeline, audio_decoder, "decode");

    alc_volume_setup_cfg_t alc_cfg = DEFAULT_ALC_VOLUME_SETUP_CONFIG();
    audio_element_handle_t volume_control = alc_volume_setup_init(&alc_cfg);
    audio_pipeline_register(pipeline, volume_control, "alc");

    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    audio_element_handle_t i2s_stream_writer = i2s_stream_init(&i2s_cfg);
    audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

    const char *link_tag[4] = { "http", "decode", "alc", "i2s" };
    audio_pipeline_link(pipeline, link_tag, 4);

    ESP_LOGI(TAG, "1");

    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    periph_cfg.task_core = 1;
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "init encoder?");

    audio_board_encoder_init(set);

    audio_board_mywifi_init(set);

    // vTaskDelay(10);

    // ESP_LOGI(TAG, "[ 3 ] Start and wait for Wi-Fi network");
    // vTaskDelay(10);

    //     periph_wifi_cfg_t wifi_cfg = {};
    //     esp_periph_handle_t wifi_handle = periph_wifi_init(&wifi_cfg);

    // #if defined(CONFIG_BTDM_CTRL_MODE_BLE_ONLY)
    //     ESP_LOGI(TAG, "CONFIG_BTDM_CTRL_MODE_BLE_ONLY");
    //     vTaskDelay(10);
    // #endif

    //     ESP_LOGI(TAG, "[ 3.2 ] Start and wait for Wi-Fi network");
    //     vTaskDelay(10);

    //     esp_periph_start(set, wifi_handle);

    //     ESP_LOGI(TAG, "[ 3.3 ] Start and wait for Wi-Fi network");
    //     vTaskDelay(10);

    //     periph_wifi_config_start(wifi_handle, WIFI_CONFIG_BLUEFI);

    //     ESP_LOGI(TAG, "[ 3.4 ] Start and wait for Wi-Fi network");
    //     vTaskDelay(10);

    //     periph_wifi_wait_for_connected(wifi_handle, portMAX_DELAY);

    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    audio_pipeline_set_listener(pipeline, evt);

    int volume = -25;
    alc_volume_setup_set_volume(volume_control, volume);

    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    // audio_element_set_uri(http_stream_reader, "http://stream.live.vc.bbcmedia.co.uk/bbc_world_service");
    // audio_element_set_uri(http_stream_reader, "https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.mp3");
    // audio_element_set_uri(http_stream_reader, "http://media-ice.musicradio.com/LBCLondon.m3u");
    // audio_element_set_uri(http_stream_reader, "http://media-ice.musicradio.com/LBCUK.m3u");
    // audio_element_set_uri(http_stream_reader, "http://media-ice.musicradio.com/LBCUKMP3Low");
    // audio_element_set_uri(
    //     http_stream_reader,
    //     "http://as-hls-ww-live.akamaized.net/pool_904/live/ww/bbc_radio_fourlw/bbc_radio_fourlw.sml/bbc_radio_fourlw-audio%3d96000.norewind.m3u8");

    // audio_element_set_uri(http_stream_reader,
    //                       "http://as-hls-ww-live.akamaized.net/pool_904/live/ww/bbc_radio_fourfm/bbc_radio_fourfm.isml/bbc_radio_fourfm-audio=320000.m3u8");

    // audio_pipeline_run(pipeline);

    // uint16_t *buffer;
    // if(lcd_get_backbuffer(&buffer, portMAX_DELAY) == ESP_OK) {
    //     memset(buffer, 0xff, LCD_WIDTH * LCD_HEIGHT * 2);
    //     lcd_release_backbuffer_and_update();
    // }

    while(1) {
        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, 1);

        if(ret != ESP_OK) {
            continue;
        }

        // ESP_LOGI(TAG, "MSG: %d", msg.source_type);

        if(msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *)audio_decoder && msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO) {
            audio_element_info_t music_info = { 0 };
            audio_element_getinfo(audio_decoder, &music_info);
            ESP_LOGI(TAG, "[ * ] Receive music info from mp3 decoder, sample_rates=%d, bits=%d, ch=%d", music_info.sample_rates, music_info.bits,
                     music_info.channels);
            i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
            continue;
        }

        /* restart stream when the first pipeline element (http_stream_reader in this case) receives stop event (caused by reading errors) */
        if(msg.source_type == AUDIO_ELEMENT_TYPE_ELEMENT && msg.source == (void *)http_stream_reader && msg.cmd == AEL_MSG_CMD_REPORT_STATUS &&
           (int)msg.data == AEL_STATUS_STATE_FINISHED) {
            ESP_LOGW(TAG, "[ * ] Restart stream");
            audio_pipeline_stop(pipeline);
            audio_pipeline_wait_for_stop(pipeline);
            audio_element_reset_state(audio_decoder);
            audio_element_reset_state(i2s_stream_writer);
            audio_pipeline_reset_ringbuffer(pipeline);
            audio_pipeline_reset_items_state(pipeline);
            audio_pipeline_run(pipeline);
            continue;
        }

        if(msg.source_type == PERIPH_ID_MYWIFI) {
            ESP_LOGI(TAG, "MYWIFI says %d", msg.cmd);
        }

        if(msg.source_type == PERIPH_ID_ENCODER) {
            int direction = 0;
            switch(msg.cmd) {
            case PERIPH_ENCODER_CLOCKWISE:
                direction = 4;
                break;
            case PERIPH_ENCODER_COUNTER_CLOCKWISE:
                direction = -4;
                break;
            }
            // int old_volume = volume;
            if(direction != 0) {
                volume += direction;
                if(volume > 0) {
                    volume = 0;
                } else if(volume < -64) {
                    volume = -64;
                }
                alc_volume_setup_set_volume(volume_control, volume);
            }
        }

        if((msg.source_type == PERIPH_ID_TOUCH || msg.source_type == PERIPH_ID_BUTTON || msg.source_type == PERIPH_ID_ADC_BTN) &&
           (msg.cmd == PERIPH_TOUCH_TAP || msg.cmd == PERIPH_BUTTON_PRESSED || msg.cmd == PERIPH_ADC_BUTTON_PRESSED)) {
            if((int)msg.data == get_input_play_id()) {
                ESP_LOGI(TAG, "[ * ] [Play] touch tap event");
                audio_element_state_t el_state = audio_element_get_state(i2s_stream_writer);
                switch(el_state) {
                case AEL_STATE_INIT:
                    ESP_LOGI(TAG, "[ * ] Starting audio pipeline");
                    audio_pipeline_run(pipeline);
                    break;
                case AEL_STATE_RUNNING:
                    ESP_LOGI(TAG, "[ * ] Pausing audio pipeline");
                    audio_pipeline_pause(pipeline);
                    break;
                case AEL_STATE_PAUSED:
                    ESP_LOGI(TAG, "[ * ] Resuming audio pipeline");
                    audio_pipeline_resume(pipeline);
                    break;
                case AEL_STATE_FINISHED:
                    ESP_LOGI(TAG, "[ * ] Rewinding audio pipeline");
                    audio_pipeline_reset_ringbuffer(pipeline);
                    audio_pipeline_reset_elements(pipeline);
                    audio_pipeline_change_state(pipeline, AEL_STATE_INIT);
                    audio_pipeline_run(pipeline);
                    break;
                default:
                    ESP_LOGI(TAG, "[ * ] Not supported state %d", el_state);
                }
            } else if((int)msg.data == get_input_set_id()) {
                ESP_LOGI(TAG, "[ * ] [Set] touch tap event");
                ESP_LOGI(TAG, "[ * ] Stopping audio pipeline");
                break;
            } else if((int)msg.data == get_input_mode_id()) {
                ESP_LOGI(TAG, "[ * ] [mode] tap event");
                audio_pipeline_stop(pipeline);
                audio_pipeline_wait_for_stop(pipeline);
                audio_pipeline_terminate(pipeline);
                audio_pipeline_reset_ringbuffer(pipeline);
                audio_pipeline_reset_elements(pipeline);
                audio_pipeline_run(pipeline);
            } else if((int)msg.data == get_input_volup_id()) {
            } else if((int)msg.data == get_input_voldown_id()) {
            }
        }
    }

    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);
    audio_pipeline_unregister(pipeline, audio_decoder);
    audio_pipeline_unregister(pipeline, i2s_stream_writer);

    // Terminate the pipeline before removing the listener
    audio_pipeline_remove_listener(pipeline);

    // Make sure audio_pipeline_remove_listener is called before destroying event_iface
    audio_event_iface_destroy(evt);

    // Release all resources
    audio_pipeline_deinit(pipeline);
    audio_element_deinit(i2s_stream_writer);
    audio_element_deinit(audio_decoder);
}

//////////////////////////////////////////////////////////////////////

esp_err_t stream_init(void)
{
    if(xTaskCreatePinnedToCore(stream_task, "stream", 4096, NULL, 20, &stream_task_handle, 1) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

esp_err_t stream_play(char const *url)
{
    // audio_element_set_uri(http_stream_reader,
    //                       "http://as-hls-ww-live.akamaized.net/pool_904/live/ww/bbc_radio_fourfm/bbc_radio_fourfm.isml/bbc_radio_fourfm-audio=320000.m3u8");

    // audio_pipeline_run(pipeline);

    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

esp_err_t stream_stop()
{
    return ESP_OK;
}