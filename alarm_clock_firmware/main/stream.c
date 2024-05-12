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
#include "filter_resample.h"

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
#include "image.h"
#include "font.h"
#include "stream.h"
#include "assets.h"
#include "util.h"

LOG_CONTEXT("stream");

//////////////////////////////////////////////////////////////////////

#define STREAM_RATE 44100
#define STREAM_CHANNEL 1
#define STREAM_BITS 8

#define PLAYBACK_RATE 22050
#define PLAYBACK_CHANNEL 1
#define PLAYBACK_BITS 16

#define DEFAULT_STREAM_URI "http://as-hls-ww-live.akamaized.net/pool_904/live/ww/bbc_radio_fourfm/bbc_radio_fourfm.isml/bbc_radio_fourfm-audio=320000.m3u8"
// #define DEFAULT_STREAM_URI "http://stream.live.vc.bbcmedia.co.uk/bbc_world_service"
// #define DEFAULT_STREAM_URI "https://dl.espressif.com/dl/audio/ff-16b-2c-44100hz.mp3"
// #define DEFAULT_STREAM_URI "http://media-ice.musicradio.com/LBCLondon.m3u"
// #define DEFAULT_STREAM_URI "http://media-ice.musicradio.com/LBCUK.m3u"
// #define DEFAULT_STREAM_URI "http://media-ice.musicradio.com/LBCUKMP3Low"

//////////////////////////////////////////////////////////////////////

static TaskHandle_t stream_task_handle;

//////////////////////////////////////////////////////////////////////

static audio_element_handle_t create_resample_filter(int source_rate, int source_channel, int dest_rate, int dest_channel)
{
    rsp_filter_cfg_t rsp_cfg = DEFAULT_RESAMPLE_FILTER_CONFIG();
    rsp_cfg.src_rate = source_rate;
    rsp_cfg.src_ch = source_channel;
    rsp_cfg.dest_rate = dest_rate;
    rsp_cfg.dest_ch = dest_channel;
    return rsp_filter_init(&rsp_cfg);
}

//////////////////////////////////////////////////////////////////////

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

    audio_element_handle_t rsp_filter = create_resample_filter(STREAM_RATE, STREAM_CHANNEL, PLAYBACK_RATE, PLAYBACK_CHANNEL);
    audio_pipeline_register(pipeline, rsp_filter, "downsample");

    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    audio_element_handle_t i2s_stream_writer = i2s_stream_init(&i2s_cfg);
    audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

    const char *link_tag[] = { "http", "decode", "downsample", "alc", "i2s" };
    audio_pipeline_link(pipeline, link_tag, ARRAY_SIZE(link_tag));

    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    periph_cfg.task_core = 1;
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    audio_board_encoder_init(set);

    audio_board_mywifi_init(set);

    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    audio_event_iface_handle_t evt = audio_event_iface_init(&evt_cfg);

    audio_pipeline_set_listener(pipeline, evt);

    int volume = -25;
    alc_volume_setup_set_volume(volume_control, volume);

    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    while(1) {

        audio_event_iface_msg_t msg;
        esp_err_t ret = audio_event_iface_listen(evt, &msg, 1);

        if(ret != ESP_OK) {
            continue;
        }

        switch(msg.source_type) {

        case AUDIO_ELEMENT_TYPE_ELEMENT:

            if(msg.source == (void *)audio_decoder) {

                switch(msg.cmd) {

                case AEL_MSG_CMD_REPORT_MUSIC_INFO: {

                    audio_element_info_t info;
                    audio_element_getinfo(audio_decoder, &info);
                    LOG_I("STREAM: sample_rate %d, bits=%d, ch=%d", info.sample_rates, info.bits, info.channels);
                    rsp_filter_set_src_info(rsp_filter, info.sample_rates, info.channels);
                    i2s_stream_set_clk(i2s_stream_writer, PLAYBACK_RATE, PLAYBACK_BITS, PLAYBACK_CHANNEL);
                } break;

                default:
                    break;
                }

            } else if(msg.source == (void *)http_stream_reader) {

                switch(msg.cmd) {

                case AEL_MSG_CMD_REPORT_STATUS: {
                    if((int)msg.data == AEL_STATUS_STATE_FINISHED) {
                        LOG_W("Restart stream");
                        audio_pipeline_stop(pipeline);
                        audio_pipeline_wait_for_stop(pipeline);
                        audio_element_reset_state(audio_decoder);
                        audio_element_reset_state(i2s_stream_writer);
                        audio_pipeline_reset_ringbuffer(pipeline);
                        audio_pipeline_reset_items_state(pipeline);
                        audio_pipeline_run(pipeline);
                    }
                } break;

                default:
                    break;
                }
            }
            break;

        case PERIPH_ID_MYWIFI: {

            LOG_I("MYWIFI says %d", msg.cmd);

            switch(msg.cmd) {

            case PERIPH_MYWIFI_CONNECTED:

                audio_element_set_uri(http_stream_reader, DEFAULT_STREAM_URI);
                audio_pipeline_run(pipeline);
                break;

            case PERIPH_MYWIFI_DISCONNECTED:

                audio_pipeline_stop(pipeline);
                audio_pipeline_wait_for_stop(pipeline);
                audio_pipeline_terminate(pipeline);
                audio_pipeline_reset_ringbuffer(pipeline);
                audio_pipeline_reset_elements(pipeline);
                break;
            }

        } break;

        case PERIPH_ID_ENCODER: {

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
        } break;

        default:
            break;
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
    if(xTaskCreatePinnedToCore(stream_task, "stream", 4096, NULL, 10, &stream_task_handle, 1) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
