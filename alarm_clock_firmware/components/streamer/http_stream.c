#include <sys/unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <strings.h>
#include <errno.h>

#include "freertos/FreeRTOS.h"
#include "freertos/ringbuf.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "http_stream.h"
#include "http_playlist.h"
#include "audio_mem.h"
#include "esp_system.h"
#include "esp_http_client.h"
#include "line_reader.h"
#include "hls_playlist.h"
#include "gzip_miniz.h"

#include "aes/esp_aes.h"

static const char *TAG = "HTTP_STREAM";

#define HLS_LOG(...) ESP_LOGI(TAG, __VA_ARGS__)

#define MAX_PLAYLIST_LINE_SIZE (512)
#define HTTP_STREAM_BUFFER_SIZE (2048)
#define HTTP_MAX_CONNECT_TIMES (5)

#define HLS_PREFER_BITRATE (200 * 1024)
#define HLS_KEY_CACHE_SIZE (32)

enum
{
    AEL_IO_OK = ESP_OK,
    AEL_IO_FAIL = ESP_FAIL,
    AEL_IO_DONE = -2,
    AEL_IO_ABORT = -3,
    AEL_IO_TIMEOUT = -4,
    AEL_PROCESS_FAIL = -5,
};

typedef struct
{
    bool key_loaded;
    char *key_url;
    uint8_t key_cache[HLS_KEY_CACHE_SIZE];
    uint8_t key_size;
    hls_stream_key_t key;
    uint64_t sequence_no;
    esp_aes_context aes_ctx;
    bool aes_used;
} http_stream_hls_key_t;

typedef struct http_stream
{
    audio_stream_type_t type;
    bool is_open;
    esp_http_client_handle_t client;
    http_stream_event_handle_t hook;
    audio_stream_type_t stream_type;
    void *user_data;
    bool enable_playlist_parser;
    bool auto_connect_next_track; /* connect next track without open/close */
    bool is_playlist_resolved;
    bool is_valid_playlist;
    bool is_main_playlist;
    http_playlist_t *playlist; /* media playlist */
    int _errno;                /* errno code for http */
    int connect_times;         /* max reconnect times */
    const char *cert_pem;
#if(ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0))
    esp_err_t (*crt_bundle_attach)(void *conf); /*  Function pointer to esp_crt_bundle_attach*/
#endif
    bool gzip_encoding;       /* Content is encoded */
    gzip_miniz_handle_t gzip; /* GZIP instance */
    http_stream_hls_key_t *hls_key;
    hls_handle_t *hls_media;
    int request_range_size;
    int64_t request_range_end;
    bool is_last_range;
    const char *user_agent;
    http_stream_info_t info;
    bool is_stopping;
    char *original_uri;
} http_stream_t;

static esp_err_t http_stream_auto_connect_next_track(http_stream_handle_t el);

// `errno` is not thread safe in multiple HTTP-clients,
// so it is necessary to save the errno number of HTTP clients to avoid reading and writing exceptions of HTTP-clients caused by errno exceptions
int __attribute__((weak)) esp_http_client_get_errno(esp_http_client_handle_t client)
{
    (void)client;
    ESP_LOGE(TAG,
             "Not found right %s.\r\nPlease enter ADF-PATH with \"cd $ADF_PATH/idf_patches\" and apply the ADF patch with \"git apply "
             "$ADF_PATH/idf_patches/idf_%.4s_esp_http_client.patch\" first\r\n",
             __func__, IDF_VER);
    return errno;
}


static esp_codec_type_t get_audio_type(const char *content_type)
{
    if(strcasecmp("mp3", content_type) == 0) {
        return ESP_CODEC_TYPE_MP3;
    }
    if(strcasecmp("audio/mp3", content_type) == 0) {
        return ESP_CODEC_TYPE_MP3;
    }
    if(strcasecmp("audio/mpeg", content_type) == 0) {
        return ESP_CODEC_TYPE_MP3;
    }
    if(strcasecmp("binary/octet-stream", content_type) == 0) {
        return ESP_CODEC_TYPE_MP3;
    }
    if(strcasecmp("application/octet-stream", content_type) == 0) {
        return ESP_CODEC_TYPE_MP3;
    }

    if(strcasecmp("audio/aac", content_type) == 0) {
        return ESP_CODEC_TYPE_AAC;
    }
    if(strcasecmp("audio/x-aac", content_type) == 0) {
        return ESP_CODEC_TYPE_AAC;
    }
    if(strcasecmp("audio/mp4", content_type) == 0) {
        return ESP_CODEC_TYPE_AAC;
    }
    if(strcasecmp("audio/aacp", content_type) == 0) {
        return ESP_CODEC_TYPE_AAC;
    }
    if(strcasecmp("video/MP2T", content_type) == 0) {
        return ESP_CODEC_TYPE_AAC;
    }

    if(strcasecmp("audio/wav", content_type) == 0) {
        return ESP_CODEC_TYPE_WAV;
    }

    if(strcasecmp("audio/opus", content_type) == 0) {
        return ESP_CODEC_TYPE_OPUS;
    }

    if(strcasecmp("application/vnd.apple.mpegurl", content_type) == 0) {
        return ESP_AUDIO_TYPE_M3U8;
    }
    if(strcasecmp("vnd.apple.mpegURL", content_type) == 0) {
        return ESP_AUDIO_TYPE_M3U8;
    }

    if(strcasecmp("audio/x-scpls", content_type) == 0) {
        return ESP_AUDIO_TYPE_PLS;
    }

    return ESP_CODEC_TYPE_UNKNOW;
}

static int _gzip_read_data(uint8_t *data, int size, void *ctx)
{
    http_stream_handle_t http = (http_stream_handle_t)ctx;
    return esp_http_client_read(http->client, (char *)data, size);
}

static esp_err_t _http_event_handle(esp_http_client_event_t *evt)
{
    http_stream_handle_t http = (http_stream_handle_t)evt->user_data;
    if(evt->event_id != HTTP_EVENT_ON_HEADER) {
        return ESP_OK;
    }
    HLS_LOG("%s=%s", evt->header_key, evt->header_value);
    if(strcasecmp(evt->header_key, "Content-Type") == 0) {
        http->info.codec_fmt = get_audio_type(evt->header_value);
        HLS_LOG("Content Type is %d", http->info.codec_fmt);
    } else if(strcasecmp(evt->header_key, "Content-Encoding") == 0) {
        http->gzip_encoding = true;
        if(strcasecmp(evt->header_value, "gzip") == 0) {
            gzip_miniz_cfg_t cfg = {
                .chunk_size = 1024,
                .ctx = http,
                .read_cb = _gzip_read_data,
            };
            http->gzip = gzip_miniz_init(&cfg);
        }
        if(http->gzip == NULL) {
            ESP_LOGE(TAG, "Content-Encoding %s not supported", evt->header_value);
            return ESP_FAIL;
        }
    } else if(strcasecmp(evt->header_key, "Content-Range") == 0) {
        if(http->request_range_size) {
            char *end_pos = strchr(evt->header_value, '-');
            http->is_last_range = true;
            if(end_pos) {
                end_pos++;
                int64_t range_end = atoll(end_pos);
                HLS_LOG("RANGE END: %lld", range_end);
                if(range_end == http->request_range_end) {
                    http->is_last_range = false;
                }
                http->info.total_bytes = range_end + 1;
            }
        }
    }
    return ESP_OK;
}

// here is where we would do the things

static int dispatch_hook(http_stream_handle_t self, http_stream_event_id_t type, void *buffer, int buffer_len)
{
    // http_stream_event_msg_t msg;
    // msg.event_id = type;
    // msg.http_client = http_stream->client;
    // msg.user_data = http_stream->user_data;
    // msg.buffer = buffer;
    // msg.buffer_len = buffer_len;
    // msg.el = self;
    // if(http_stream->hook) {
    //     return http_stream->hook(&msg);
    // }
    return ESP_OK;
}

static bool _is_playlist(http_stream_info_t *info, const char *uri)
{
    if(info->codec_fmt == ESP_AUDIO_TYPE_M3U8 || info->codec_fmt == ESP_AUDIO_TYPE_PLS) {
        return true;
    }
    const char *s = uri;
    while(*s) {
        if(*s == '.') {
            if(strncasecmp(s, ".m3u", 3) == 0) {
                return true;
            }
        }
        s++;
    }
    return false;
}

static int _hls_uri_cb(char *uri, void *ctx)
{
    http_stream_handle_t http = (http_stream_handle_t)ctx;
    if(uri) {
        http_playlist_insert(http->playlist, uri);
        http->is_valid_playlist = true;
    }
    return 0;
}

static int _http_read_data(http_stream_handle_t http, char *buffer, int len)
{
    if(http->gzip_encoding == false) {
        return esp_http_client_read(http->client, buffer, len);
    }
    // use gzip to uncompress data
    return gzip_miniz_read(http->gzip, (uint8_t *)buffer, len);
}

static esp_err_t _resolve_hls_key(http_stream_handle_t http)
{
    int ret = _http_read_data(http, (char *)http->hls_key->key_cache, sizeof(http->hls_key->key_cache));
    if(ret < 0) {
        return ESP_FAIL;
    }
    http->hls_key->key_size = (uint8_t)ret;
    http->hls_key->key_loaded = true;
    return ESP_OK;
}

static esp_err_t _prepare_crypt(http_stream_handle_t http)
{
    http_stream_hls_key_t *hls_key = http->hls_key;
    if(hls_key->aes_used) {
        esp_aes_free(&hls_key->aes_ctx);
        hls_key->aes_used = false;
    }
    int ret = hls_playlist_parse_key(http->hls_media, http->hls_key->key_cache, http->hls_key->key_size);
    if(ret < 0) {
        return ESP_FAIL;
    }
    ret = hls_playlist_get_key(http->hls_media, http->hls_key->sequence_no, &http->hls_key->key);
    if(ret != 0) {
        return ESP_FAIL;
    }
    esp_aes_init(&hls_key->aes_ctx);
    esp_aes_setkey(&hls_key->aes_ctx, (unsigned char *)hls_key->key.key, 128);
    hls_key->aes_used = true;
    http->hls_key->sequence_no++;
    return ESP_OK;
}

static void _free_hls_key(http_stream_handle_t http)
{
    if(http->hls_key == NULL) {
        return;
    }
    if(http->hls_key->aes_used) {
        esp_aes_free(&http->hls_key->aes_ctx);
        http->hls_key->aes_used = false;
    }
    if(http->hls_key->key_url) {
        audio_free(http->hls_key->key_url);
    }
    audio_free(http->hls_key);
    http->hls_key = NULL;
}

static esp_err_t _resolve_playlist(http_stream_handle_t http, const char *uri)
{
    if(http->hls_media) {
        hls_playlist_close(http->hls_media);
        http->hls_media = NULL;
    }
    // backup new uri firstly
    char *new_uri = audio_strdup(uri);
    if(new_uri == NULL) {
        return ESP_FAIL;
    }
    if(http->is_main_playlist) {
        http_playlist_clear(http->playlist);
    }
    if(http->playlist->host_uri) {
        audio_free(http->playlist->host_uri);
    }
    http->playlist->host_uri = new_uri;
    http->is_valid_playlist = false;

    // handle PLS playlist
    if(http->info.codec_fmt == ESP_AUDIO_TYPE_PLS) {
        line_reader_t *reader = line_reader_init(MAX_PLAYLIST_LINE_SIZE);
        if(reader == NULL) {
            return ESP_FAIL;
        }
        int need_read = MAX_PLAYLIST_LINE_SIZE;
        int rlen = need_read;
        while(rlen == need_read) {
            rlen = _http_read_data(http, http->playlist->data, need_read);
            if(rlen < 0) {
                break;
            }
            line_reader_add_buffer(reader, (uint8_t *)http->playlist->data, rlen, (rlen < need_read));
            char *line;
            while((line = line_reader_get_line(reader)) != NULL) {
                if(!strncmp(line, "File", sizeof("File") - 1)) {    // This line contains url
                    int i = 4;
                    while(line[i++] != '=')
                        ;    // Skip till '='
                    http_playlist_insert(http->playlist, line + i);
                    http->is_valid_playlist = true;
                }
            }
        }
        line_reader_deinit(reader);
        return http->is_valid_playlist ? ESP_OK : ESP_FAIL;
    }

    // handle other (normal, modern I guess?) playlist
    http->is_main_playlist = false;
    hls_playlist_cfg_t cfg = {
        .prefer_bitrate = HLS_PREFER_BITRATE,
        .cb = _hls_uri_cb,
        .ctx = http,
        .uri = (char *)new_uri,
    };
    hls_handle_t hls = hls_playlist_open(&cfg);

    do {
        if(hls == NULL) {
            break;
        }
        int need_read = MAX_PLAYLIST_LINE_SIZE;
        int rlen = need_read;
        while(rlen == need_read) {
            rlen = _http_read_data(http, http->playlist->data, need_read);
            if(rlen < 0) {
                break;
            }
            hls_playlist_parse_data(hls, (uint8_t *)http->playlist->data, rlen, (rlen < need_read));
        }
        HLS_LOG("Loaded playlist?");

        if(hls_playlist_is_master(hls)) {
            char *url = hls_playlist_get_prefer_url(hls, HLS_STREAM_TYPE_AUDIO);
            if(url) {
                http_playlist_insert(http->playlist, url);
                ESP_LOGI(TAG, "Add media uri %s\n", url);
                http->is_valid_playlist = true;
                http->is_main_playlist = true;
                audio_free(url);
            }
        } else {
            http->playlist->is_incomplete = !hls_playlist_is_media_end(hls);

            if(http->playlist->is_incomplete) {
                HLS_LOG("PLAYLIST %d items", http->playlist->total_tracks);
                ESP_LOGI(TAG, "Live stream URI. Need to be fetched again!");
            }
        }
    } while(0);

    if(hls) {
        HLS_LOG("hls is encrypted?");
        if(hls_playlist_is_encrypt(hls) == false) {
            _free_hls_key(http);
            hls_playlist_close(hls);
            HLS_LOG("nope, closed it..?");
        } else {
            // When content is encrypted, need keep hls instance
            http->hls_media = hls;
            const char *key_url = hls_playlist_get_key_uri(hls);
            if(key_url == NULL) {
                ESP_LOGE(TAG, "Hls do not have key url");
                return ESP_FAIL;
            }
            if(http->hls_key == NULL) {
                http->hls_key = (http_stream_hls_key_t *)calloc(1, sizeof(http_stream_hls_key_t));
                if(http->hls_key == NULL) {
                    ESP_LOGE(TAG, "No memory for hls key");
                    return ESP_FAIL;
                }
            }
            if(http->hls_key->key_url && strcmp(http->hls_key->key_url, key_url) == 0) {
                http->hls_key->key_loaded = true;
            } else {
                if(http->hls_key->key_url) {
                    audio_free(http->hls_key->key_url);
                }
                http->hls_key->key_loaded = false;
                http->hls_key->key_url = audio_strdup(key_url);
                if(http->hls_key->key_url == NULL) {
                    ESP_LOGE(TAG, "No memory for hls key url");
                    return ESP_FAIL;
                }
            }
            http->hls_key->sequence_no = hls_playlist_get_sequence_no(hls);
        }
    }
    HLS_LOG("IS_VALID_PLAYLIST: %d", http->is_valid_playlist);
    return http->is_valid_playlist ? ESP_OK : ESP_FAIL;
}

static char *_playlist_get_next_track(http_stream_handle_t http)
{
    HLS_LOG("_playlist_get_next_track, http->enable_playlist_parser = %d, http->is_playlist_resolved = %d", http->enable_playlist_parser,
            http->is_playlist_resolved);
    if(http->enable_playlist_parser && http->is_playlist_resolved) {
        char *next_track = http_playlist_get_next_track(http->playlist);
        HLS_LOG("http_playlist_get_next_track GOT: %s", next_track);
        return next_track;
    }
    return NULL;
}

static void _prepare_range(http_stream_handle_t http, int64_t pos)
{
    if(http->request_range_size > 0 || pos != 0) {
        char range_header[64] = { 0 };
        if(http->request_range_size == 0) {
            snprintf(range_header, sizeof(range_header), "bytes=%lld-", pos);
        } else {
            int64_t end_pos = pos + http->request_range_size - 1;
            if(pos < 0 && end_pos > 0) {
                end_pos = 0;
            }
            snprintf(range_header, sizeof(range_header), "bytes=%lld-%lld", pos, end_pos);
            http->request_range_end = end_pos;
        }
        esp_http_client_set_header(http->client, "Range", range_header);
    } else {
        esp_http_client_delete_header(http->client, "Range");
    }
}

static esp_err_t _http_load_uri(http_stream_handle_t http)
{
    esp_err_t err;

    HLS_LOG("LOAD URI: uri = %s", http->info.uri);

    esp_http_client_close(http->client);

    // if(dispatch_hook(self, HTTP_STREAM_PRE_REQUEST, NULL, 0) != ESP_OK) {
    //     ESP_LOGE(TAG, "Failed to process user callback");
    //     return ESP_FAIL;
    // }

    _prepare_range(http, http->info.byte_pos);

    char *buffer = NULL;
    int post_len = esp_http_client_get_post_field(http->client, &buffer);

_stream_redirect:

    if(http->gzip_encoding) {
        gzip_miniz_deinit(http->gzip);
        http->gzip = NULL;
        http->gzip_encoding = false;
    }
    if((err = esp_http_client_open(http->client, post_len)) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open http stream");
        return err;
    }

    // int wrlen = dispatch_hook(self, HTTP_STREAM_ON_REQUEST, buffer, post_len);
    // if(wrlen < 0) {
    //     ESP_LOGE(TAG, "Failed to process user callback");
    //     return ESP_FAIL;
    // }
    int wrlen = 0;

    if(post_len && buffer && wrlen == 0) {
        if(esp_http_client_write(http->client, buffer, post_len) <= 0) {
            ESP_LOGE(TAG, "Failed to write data to http stream");
            return ESP_FAIL;
        }
        ESP_LOGD(TAG, "len=%d, data=%s", post_len, buffer);
    }

    // if(dispatch_hook(self, HTTP_STREAM_POST_REQUEST, NULL, 0) < 0) {
    //     esp_http_client_close(http->client);
    //     return ESP_FAIL;
    // }
    /*
     * Due to the total byte of content has been changed after seek, set info.total_bytes at beginning only.
     */
    int64_t cur_pos = esp_http_client_fetch_headers(http->client);

    if(http->info.byte_pos <= 0) {
        http->info.total_bytes = cur_pos;
    }
    int status_code = esp_http_client_get_status_code(http->client);
    if(status_code == 301 || status_code == 302) {
        esp_http_client_set_redirection(http->client);
        goto _stream_redirect;
    }

    if(status_code != 200 && (esp_http_client_get_status_code(http->client) != 206) && (esp_http_client_get_status_code(http->client) != 416)) {
        ESP_LOGE(TAG, "Invalid HTTP stream, status code = %d", status_code);
        if(http->enable_playlist_parser) {
            http_playlist_clear(http->playlist);
            http->is_playlist_resolved = false;
        }
        return ESP_FAIL;
    }
    return err;
}

esp_err_t http_stream_open(http_stream_handle_t http, char const *url)
{
    char *uri = NULL;

    // this is the root uri which might be a playlist or a stream
    // if it's a playlist we'll play all the streams
    // if it's a stream we'll just play it

    http->original_uri =
        url;    //"http://as-hls-ww-live.akamaized.net/pool_904/live/ww/bbc_radio_fourfm/bbc_radio_fourfm.isml/bbc_radio_fourfm-audio=320000.m3u8";

    ESP_LOGD(TAG, "_http_open");

    if(http->is_open) {
        ESP_LOGE(TAG, "already opened");
        return ESP_OK;
    }
    http->_errno = 0;

_stream_open_begin:

    HLS_LOG("STREAM OPEN BEGIN");

    if(http->hls_key && http->hls_key->key_loaded == false) {
        HLS_LOG("1");
        uri = http->hls_key->key_url;
    } else if(http->info.byte_pos == 0) {
        HLS_LOG("2");
        uri = _playlist_get_next_track(http);
    } else if(http->is_playlist_resolved) {
        HLS_LOG("3");
        uri = http_playlist_get_last_track(http->playlist);
    }

    if(uri == NULL) {
        uri = http->original_uri;
        HLS_LOG("4");
    }

    // if(uri == NULL) {
    //     if(http->is_playlist_resolved && http->enable_playlist_parser) {
    //         // if(dispatch_hook(self, HTTP_STREAM_FINISH_PLAYLIST, NULL, 0) != ESP_OK) {
    //         //     ESP_LOGE(TAG, "Failed to process user callback");
    //         //     return ESP_FAIL;
    //         // }
    //         goto _stream_open_begin;
    //     }
    //     // uri = audio_element_get_uri(self);
    // }

    if(uri == NULL) {
        ESP_LOGE(TAG, "Error open connection, uri = NULL");
        return ESP_FAIL;
    }

    HLS_LOG("URI=%s, client = %p", uri, http->client);

    // if not initialize http client, initial it
    if(http->client == NULL) {
        HLS_LOG("HERE WE GO?");
        esp_http_client_config_t http_cfg = {
            .url = uri,
            .event_handler = _http_event_handle,
            .user_data = http,
            .timeout_ms = 30 * 1000,
            .buffer_size = HTTP_STREAM_BUFFER_SIZE,
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 1, 0)
            .buffer_size_tx = 1024,
#endif
            .cert_pem = http->cert_pem,
#if(ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0)) && defined CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
            .crt_bundle_attach = http->crt_bundle_attach,
#endif    //  (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0)) && defined CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
            .user_agent = http->user_agent,
        };
        HLS_LOG("INIT HTTP CLIENT!!!!");
        http->client = esp_http_client_init(&http_cfg);
        AUDIO_MEM_CHECK(TAG, http->client, return ESP_ERR_NO_MEM);
    } else {
        esp_http_client_set_url(http->client, uri);
    }

    // http->info.uri = "http://as-hls-ww-live.akamaized.net/pool_904/live/ww/bbc_radio_fourfm/bbc_radio_fourfm.isml/bbc_radio_fourfm-audio=320000.m3u8";

    http->info.uri = uri;

    if(_http_load_uri(http) != ESP_OK) {
        return ESP_FAIL;
    }

    if(_is_playlist(&http->info, uri)) {
        /**
         * `goto _stream_open_begin` blocks on http_open until it gets valid URL.
         * Ensure that the stop command is processed
         */
        // if(audio_element_is_stopping(self)) {
        if(http->is_stopping) {
            ESP_LOGW(TAG, "Http_open got stop cmd at opening");
            return ESP_OK;
        }

        if(_resolve_playlist(http, uri) == ESP_OK) {
            HLS_LOG("PLAYLIST RESOLVED!");
            http->is_playlist_resolved = true;
            goto _stream_open_begin;
        }
    }
    // Load key and parse key
    if(http->hls_key) {
        if(http->hls_key->key_loaded == false) {
            if(_resolve_hls_key(http) != ESP_OK) {
                return ESP_FAIL;
            }
            // Load media url after key loaded
            goto _stream_open_begin;
        } else {
            if(_prepare_crypt(http) != ESP_OK) {
                return ESP_FAIL;
            }
        }
    }
    http->is_open = true;
    HLS_LOG("we are good.....");
    // audio_element_report_codec_fmt(self);
    return ESP_OK;
}

static esp_err_t _http_close(http_stream_handle_t http)
{
    ESP_LOGD(TAG, "_http_close");
    if(http->is_open) {
        http->is_open = false;
        do {
            // if(http->stream_type != AUDIO_STREAM_WRITER) {
            //     break;
            // }
            // if(dispatch_hook(self, HTTP_STREAM_POST_REQUEST, NULL, 0) < 0) {
            //     break;
            // }
            esp_http_client_fetch_headers(http->client);
            // if(dispatch_hook(self, HTTP_STREAM_FINISH_REQUEST, NULL, 0) < 0) {
            //     break;
            // }
        } while(0);
    }

    // if(AEL_STATE_PAUSED != audio_element_get_state(self)) {
    //     if(http->enable_playlist_parser) {
    //         http_playlist_clear(http->playlist);
    //         http->is_playlist_resolved = false;
    //     }
    //     audio_element_report_pos(self);
    //     audio_element_set_byte_pos(self, 0);
    // }
    _free_hls_key(http);
    if(http->hls_media) {
        hls_playlist_close(http->hls_media);
        http->hls_media = NULL;
    }
    if(http->gzip) {
        gzip_miniz_deinit(http->gzip);
        http->gzip = NULL;
    }
    if(http->client) {
        esp_http_client_close(http->client);
        esp_http_client_cleanup(http->client);
        http->client = NULL;
    }
    return ESP_OK;
}

static esp_err_t _http_reconnect(http_stream_handle_t http)
{
    esp_err_t err = ESP_OK;
    AUDIO_NULL_CHECK(TAG, http, return ESP_FAIL);
    err |= _http_close(http);
    err |= http->info.byte_pos = 0;
    err |= http_stream_open(http, http->original_uri);
    return err;
}

static bool _check_range_done(http_stream_handle_t http)
{
    bool last_range = http->is_last_range;
    // If not last range need reload uri from last position
    if(last_range == false && _http_load_uri(http) != ESP_OK) {
        return true;
    }
    return last_range;
}

int http_stream_read(http_stream_handle_t http, char *buffer, int len, TickType_t ticks_to_wait, void *context)
{
    //    int wrlen = dispatch_hook(self, HTTP_STREAM_ON_RESPONSE, buffer, len);
    int wrlen = 0;
    int rlen = wrlen;
    if(rlen == 0) {
        rlen = _http_read_data(http, buffer, len);
    }
    if(rlen <= 0 && http->request_range_size) {
        if(_check_range_done(http) == false) {
            rlen = _http_read_data(http, buffer, len);
        }
    }
    if(rlen <= 0 && http->auto_connect_next_track) {
        if(http_stream_auto_connect_next_track(http) == ESP_OK) {
            rlen = _http_read_data(http, buffer, len);
        }
    }
    if(rlen <= 0) {
        http->_errno = esp_http_client_get_errno(http->client);
        ESP_LOGW(TAG, "No more data,errno:%d, total_bytes:%llu, rlen = %d", http->_errno, http->info.byte_pos, rlen);
        if(http->_errno != 0) {    // Error occuered, reset connection
            ESP_LOGW(TAG, "Got %d errno(%s)", http->_errno, strerror(http->_errno));
            return http->_errno;
        }
        if(http->auto_connect_next_track) {
            // if(dispatch_hook(self, HTTP_STREAM_FINISH_PLAYLIST, NULL, 0) != ESP_OK) {
            //     ESP_LOGE(TAG, "Failed to process user callback");
            //     return ESP_FAIL;
            // }
        } else {
            // if(dispatch_hook(self, HTTP_STREAM_FINISH_TRACK, NULL, 0) != ESP_OK) {
            //     ESP_LOGE(TAG, "Failed to process user callback");
            //     return ESP_FAIL;
            // }
        }
        return ESP_OK;
    } else {
        if(http->hls_key) {
            int ret = esp_aes_crypt_cbc(&http->hls_key->aes_ctx, ESP_AES_DECRYPT, rlen, (unsigned char *)http->hls_key->key.iv, (unsigned char *)buffer,
                                        (unsigned char *)buffer);
            if(rlen % 16 != 0) {
                ESP_LOGE(TAG, "Data length %d not aligned", rlen);
            }
            if(ret != 0) {
                ESP_LOGE(TAG, "Fail to decrypt aes ret %d", ret);
                return ESP_FAIL;
            }
            if((http->info.total_bytes && rlen + http->info.byte_pos >= http->info.total_bytes) || rlen < len) {
                // Remove padding according PKCS#7
                uint8_t padding = buffer[rlen - 1];
                if(padding && padding <= rlen) {
                    int idx = rlen - padding;
                    int paddin_n = padding - 1;
                    while(paddin_n) {
                        if(buffer[idx++] != padding) {
                            break;
                        }
                        paddin_n--;
                    }
                    if(paddin_n == 0) {
                        rlen -= padding;
                    }
                }
            }
        }
        http->info.byte_pos += rlen;
    }
    ESP_LOGI(TAG, "req lengh=%d, read=%d, pos=%d/%d", len, rlen, (int)http->info.byte_pos, (int)http->info.total_bytes);
    return rlen;
}

static int _http_process(http_stream_handle_t http, char *in_buffer, int in_len)
{
    // read from http stream
    int r_size = 0;    // !!! READ FROM HTTP // audio_element_input(self, in_buffer, in_len);

    if(http->is_stopping) {
        ESP_LOGW(TAG, "No output due to stopping");
        return AEL_IO_ABORT;
    }
    int w_size = 0;
    if(r_size > 0) {
        if(http->_errno != 0) {
            esp_err_t ret = ESP_OK;
            if(http->connect_times > HTTP_MAX_CONNECT_TIMES) {
                ESP_LOGE(TAG, "reconnect times more than %d, disconnect http stream", HTTP_MAX_CONNECT_TIMES);
                return ESP_FAIL;
            };
            http->connect_times++;
            ret = _http_reconnect(http);
            if(ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to reset connection");
                return ret;
            }
            ESP_LOGW(TAG, "reconnect to peer successful");
            return ESP_ERR_INVALID_STATE;
        } else {
            http->connect_times = 0;

            // write to ring buffer
            w_size = 0;    // WRITE TO RING BUFFER // audio_element_output(self, in_buffer, r_size);
            // audio_element_multi_output(self, in_buffer, r_size, 0);
        }
    } else {
        w_size = r_size;
    }
    return w_size;
}

static esp_err_t _http_destroy(http_stream_handle_t http)
{
    if(http != NULL) {
        if(http->playlist) {
            audio_free(http->playlist->data);
            audio_free(http->playlist);
        }
        audio_free(http);
    }
    return ESP_OK;
}

http_stream_handle_t http_stream_init(http_stream_cfg_t *config)
{
    HLS_LOG("INIT");

    http_stream_handle_t http = audio_calloc(1, sizeof(http_stream_t));
    AUDIO_MEM_CHECK(TAG, http, return NULL);

    // audio_element_cfg_t cfg = DEFAULT_AUDIO_ELEMENT_CONFIG();
    // cfg.open = _http_open;
    // cfg.close = _http_close;
    // cfg.process = _http_process;
    // cfg.destroy = _http_destroy;
    // cfg.task_stack = config->task_stack;
    // cfg.task_prio = config->task_prio;
    // cfg.task_core = config->task_core;
    // cfg.stack_in_ext = config->stack_in_ext;
    // cfg.out_rb_size = config->out_rb_size;
    // cfg.multi_out_rb_num = config->multi_out_num;
    // cfg.tag = "http";

    http->type = config->type;
    http->enable_playlist_parser = config->enable_playlist_parser;
    http->auto_connect_next_track = config->auto_connect_next_track;
    http->hook = config->event_handle;
    http->stream_type = config->type;
    http->user_data = config->user_data;
    http->cert_pem = config->cert_pem;
    http->user_agent = config->user_agent;

    if(config->crt_bundle_attach) {
#if(ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0))
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
        http->crt_bundle_attach = config->crt_bundle_attach;
#else
        ESP_LOGW(TAG, "Please enbale CONFIG_MBEDTLS_CERTIFICATE_BUNDLE configuration in menuconfig");
#endif
#else
        ESP_LOGW(TAG, "Just support MBEDTLS_CERTIFICATE_BUNDLE on esp-idf to v4.3 or later");
#endif    // ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 3, 0)
    }

    if(http->enable_playlist_parser) {
        http->playlist = audio_calloc(1, sizeof(http_playlist_t));
        AUDIO_MEM_CHECK(TAG, http->playlist, {
            audio_free(http);
            return NULL;
        });
        http->playlist->data = audio_calloc(1, MAX_PLAYLIST_LINE_SIZE + 1);
        AUDIO_MEM_CHECK(TAG, http->playlist->data, {
            audio_free(http->playlist);
            audio_free(http);
            return NULL;
        });
        STAILQ_INIT(&http->playlist->tracks);
    }

    http->request_range_size = config->request_range_size;
    if(config->request_size) {
        // http->buffer_len = config->request_size;
    }
    HLS_LOG("INIT COMPLETE: %p", http);

    return http;
}

esp_err_t http_stream_next_track(http_stream_handle_t http)
{
    if(!(http->enable_playlist_parser && http->is_playlist_resolved)) {
        /**
         * This is not a playlist!
         * Do not reset states for restart element.
         * Just return.
         */
        ESP_LOGD(TAG, "Direct URI. Stream will be stopped");
        return ESP_OK;
    }

    http->info.byte_pos = 0;
    http->info.total_bytes = 0;
    http->is_open = false;
    return ESP_OK;
}

esp_err_t http_stream_auto_connect_next_track(http_stream_handle_t http)
{
    char *track = _playlist_get_next_track(http);
    if(track) {
        esp_http_client_set_url(http->client, track);
        char *buffer = NULL;
        int post_len = esp_http_client_get_post_field(http->client, &buffer);
    redirection:
        if((esp_http_client_open(http->client, post_len)) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to open http stream");
            return ESP_FAIL;
        }
        // if(dispatch_hook(el, HTTP_STREAM_POST_REQUEST, NULL, 0) < 0) {
        //     esp_http_client_close(http->client);
        //     return ESP_FAIL;
        // }
        http->info.total_bytes = esp_http_client_fetch_headers(http->client);
        ESP_LOGI(TAG, "total_bytes=%lld", http->info.total_bytes);
        int status_code = esp_http_client_get_status_code(http->client);
        if(status_code == 301 || status_code == 302) {
            esp_http_client_set_redirection(http->client);
            goto redirection;
        }
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t http_stream_fetch_again(http_stream_handle_t http)
{
    if(!http->playlist->is_incomplete) {
        ESP_LOGI(TAG, "Finished playing.");
        return ESP_ERR_NOT_SUPPORTED;
    } else {
        ESP_LOGI(TAG, "Fetching again %s %p", http->playlist->host_uri, http->playlist->host_uri);
        http->info.uri = http->playlist->host_uri;
        http->is_playlist_resolved = false;
    }
    return ESP_OK;
}

esp_err_t http_stream_restart(http_stream_handle_t http)
{
    http->is_playlist_resolved = false;
    return ESP_OK;
}

esp_err_t http_stream_set_server_cert(http_stream_handle_t http, const char *cert)
{
    http->cert_pem = cert;
    return ESP_OK;
}
