//////////////////////////////////////////////////////////////////////

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_rom_gpio.h>
#include <audio_mem.h>

#include <esp_peripherals.h>

#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_ble.h>

#include "mywifi.h"

//////////////////////////////////////////////////////////////////////

static char const *TAG = "mywifi";

//////////////////////////////////////////////////////////////////////

struct esp_mywifi
{
    mywifi_show_qr_code on_show_qr_code;
};

// Signal Wi-Fi events on this event-group
const int WIFI_CONNECTED_EVENT = 1;
const int WIFI_DISCONNECTED_EVENT = 2;

const int WIFI_ANY_BIT = 3;

static EventGroupHandle_t wifi_event_group;

#if CONFIG_EXAMPLE_PROV_SECURITY_VERSION_2
#if CONFIG_EXAMPLE_PROV_SEC2_DEV_MODE
#define EXAMPLE_PROV_SEC2_USERNAME "wifiprov"
#define EXAMPLE_PROV_SEC2_PWD "abcd1234"

#define PROV_QR_VERSION "v1"
#define PROV_TRANSPORT_SOFTAP "softap"
#define PROV_TRANSPORT_BLE "ble"
#define QRCODE_BASE_URL "https://espressif.github.io/esp-jumpstart/qrcode.html"

/* This salt,verifier has been generated for username = "wifiprov" and password = "abcd1234"
 * IMPORTANT NOTE: For production cases, this must be unique to every device
 * and should come from device manufacturing partition.*/
static const char sec2_salt[] = { 0x03, 0x6e, 0xe0, 0xc7, 0xbc, 0xb9, 0xed, 0xa8, 0x4c, 0x9e, 0xac, 0x97, 0xd9, 0x3d, 0xec, 0xf4 };

static const char sec2_verifier[] = {
    0x7c, 0x7c, 0x85, 0x47, 0x65, 0x08, 0x94, 0x6d, 0xd6, 0x36, 0xaf, 0x37, 0xd7, 0xe8, 0x91, 0x43, 0x78, 0xcf, 0xfd, 0x61, 0x6c, 0x59, 0xd2, 0xf8, 0x39, 0x08,
    0x12, 0x72, 0x38, 0xde, 0x9e, 0x24, 0xa4, 0x70, 0x26, 0x1c, 0xdf, 0xa9, 0x03, 0xc2, 0xb2, 0x70, 0xe7, 0xb1, 0x32, 0x24, 0xda, 0x11, 0x1d, 0x97, 0x18, 0xdc,
    0x60, 0x72, 0x08, 0xcc, 0x9a, 0xc9, 0x0c, 0x48, 0x27, 0xe2, 0xae, 0x89, 0xaa, 0x16, 0x25, 0xb8, 0x04, 0xd2, 0x1a, 0x9b, 0x3a, 0x8f, 0x37, 0xf6, 0xe4, 0x3a,
    0x71, 0x2e, 0xe1, 0x27, 0x86, 0x6e, 0xad, 0xce, 0x28, 0xff, 0x54, 0x46, 0x60, 0x1f, 0xb9, 0x96, 0x87, 0xdc, 0x57, 0x40, 0xa7, 0xd4, 0x6c, 0xc9, 0x77, 0x54,
    0xdc, 0x16, 0x82, 0xf0, 0xed, 0x35, 0x6a, 0xc4, 0x70, 0xad, 0x3d, 0x90, 0xb5, 0x81, 0x94, 0x70, 0xd7, 0xbc, 0x65, 0xb2, 0xd5, 0x18, 0xe0, 0x2e, 0xc3, 0xa5,
    0xf9, 0x68, 0xdd, 0x64, 0x7b, 0xb8, 0xb7, 0x3c, 0x9c, 0xfc, 0x00, 0xd8, 0x71, 0x7e, 0xb7, 0x9a, 0x7c, 0xb1, 0xb7, 0xc2, 0xc3, 0x18, 0x34, 0x29, 0x32, 0x43,
    0x3e, 0x00, 0x99, 0xe9, 0x82, 0x94, 0xe3, 0xd8, 0x2a, 0xb0, 0x96, 0x29, 0xb7, 0xdf, 0x0e, 0x5f, 0x08, 0x33, 0x40, 0x76, 0x52, 0x91, 0x32, 0x00, 0x9f, 0x97,
    0x2c, 0x89, 0x6c, 0x39, 0x1e, 0xc8, 0x28, 0x05, 0x44, 0x17, 0x3f, 0x68, 0x02, 0x8a, 0x9f, 0x44, 0x61, 0xd1, 0xf5, 0xa1, 0x7e, 0x5a, 0x70, 0xd2, 0xc7, 0x23,
    0x81, 0xcb, 0x38, 0x68, 0xe4, 0x2c, 0x20, 0xbc, 0x40, 0x57, 0x76, 0x17, 0xbd, 0x08, 0xb8, 0x96, 0xbc, 0x26, 0xeb, 0x32, 0x46, 0x69, 0x35, 0x05, 0x8c, 0x15,
    0x70, 0xd9, 0x1b, 0xe9, 0xbe, 0xcc, 0xa9, 0x38, 0xa6, 0x67, 0xf0, 0xad, 0x50, 0x13, 0x19, 0x72, 0x64, 0xbf, 0x52, 0xc2, 0x34, 0xe2, 0x1b, 0x11, 0x79, 0x74,
    0x72, 0xbd, 0x34, 0x5b, 0xb1, 0xe2, 0xfd, 0x66, 0x73, 0xfe, 0x71, 0x64, 0x74, 0xd0, 0x4e, 0xbc, 0x51, 0x24, 0x19, 0x40, 0x87, 0x0e, 0x92, 0x40, 0xe6, 0x21,
    0xe7, 0x2d, 0x4e, 0x37, 0x76, 0x2f, 0x2e, 0xe2, 0x68, 0xc7, 0x89, 0xe8, 0x32, 0x13, 0x42, 0x06, 0x84, 0x84, 0x53, 0x4a, 0xb3, 0x0c, 0x1b, 0x4c, 0x8d, 0x1c,
    0x51, 0x97, 0x19, 0xab, 0xae, 0x77, 0xff, 0xdb, 0xec, 0xf0, 0x10, 0x95, 0x34, 0x33, 0x6b, 0xcb, 0x3e, 0x84, 0x0f, 0xb9, 0xd8, 0x5f, 0xb8, 0xa0, 0xb8, 0x55,
    0x53, 0x3e, 0x70, 0xf7, 0x18, 0xf5, 0xce, 0x7b, 0x4e, 0xbf, 0x27, 0xce, 0xce, 0xa8, 0xb3, 0xbe, 0x40, 0xc5, 0xc5, 0x32, 0x29, 0x3e, 0x71, 0x64, 0x9e, 0xde,
    0x8c, 0xf6, 0x75, 0xa1, 0xe6, 0xf6, 0x53, 0xc8, 0x31, 0xa8, 0x78, 0xde, 0x50, 0x40, 0xf7, 0x62, 0xde, 0x36, 0xb2, 0xba
};
#endif

//////////////////////////////////////////////////////////////////////

static esp_err_t example_get_sec2_salt(const char **salt, uint16_t *salt_len)
{
#if CONFIG_EXAMPLE_PROV_SEC2_DEV_MODE
    ESP_LOGI(TAG, "Development mode: using hard coded salt");
    *salt = sec2_salt;
    *salt_len = sizeof(sec2_salt);
    return ESP_OK;
#elif CONFIG_EXAMPLE_PROV_SEC2_PROD_MODE
    ESP_LOGE(TAG, "Not implemented!");
    return ESP_FAIL;
#endif
}

//////////////////////////////////////////////////////////////////////

static esp_err_t example_get_sec2_verifier(const char **verifier, uint16_t *verifier_len)
{
#if CONFIG_EXAMPLE_PROV_SEC2_DEV_MODE
    ESP_LOGI(TAG, "Development mode: using hard coded verifier");
    *verifier = sec2_verifier;
    *verifier_len = sizeof(sec2_verifier);
    return ESP_OK;
#elif CONFIG_EXAMPLE_PROV_SEC2_PROD_MODE
    /* This code needs to be updated with appropriate implementation to provide verifier */
    ESP_LOGE(TAG, "Not implemented!");
    return ESP_FAIL;
#endif
}
#endif

//////////////////////////////////////////////////////////////////////
// Event handler for catching system events

void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    static int retries;

    if(event_base == WIFI_PROV_EVENT) {
        switch(event_id) {

        case WIFI_PROV_START:
            ESP_LOGI(TAG, "Provisioning started");
            break;

        case WIFI_PROV_CRED_RECV: {
            wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
            ESP_LOGI(TAG,
                     "Received Wi-Fi credentials"
                     "\n\tSSID     : %s\n\tPassword : %s",
                     (const char *)wifi_sta_cfg->ssid, (const char *)wifi_sta_cfg->password);
            break;
        }
        case WIFI_PROV_CRED_FAIL: {
            wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
            ESP_LOGE(TAG,
                     "Provisioning failed!\n\tReason : %s"
                     "\n\tPlease reset to factory and retry provisioning",
                     (*reason == WIFI_PROV_STA_AUTH_ERROR) ? "Wi-Fi station authentication failed" : "Wi-Fi access-point not found");
            retries++;
            if(retries >= CONFIG_EXAMPLE_PROV_MGR_MAX_RETRY_CNT) {
                ESP_LOGI(TAG, "Failed to connect with provisioned AP, reseting provisioned credentials");
                wifi_prov_mgr_reset_sm_state_on_failure();
                retries = 0;
            }
            break;
        }
        case WIFI_PROV_CRED_SUCCESS:
            ESP_LOGI(TAG, "Provisioning successful");
            retries = 0;
            break;
        case WIFI_PROV_END:
            /* De-initialize manager once provisioning is finished */
            wifi_prov_mgr_deinit();
            break;
        default:
            break;
        }
    } else if(event_base == WIFI_EVENT) {
        switch(event_id) {
        case WIFI_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "Disconnected.");
            xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_EVENT);
            xEventGroupSetBits(wifi_event_group, WIFI_DISCONNECTED_EVENT);
            esp_wifi_connect();
            break;
        default:
            break;
        }
    } else if(event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Connected with IP Address:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVENT);

    } else if(event_base == PROTOCOMM_TRANSPORT_BLE_EVENT) {
        switch(event_id) {
        case PROTOCOMM_TRANSPORT_BLE_CONNECTED:
            ESP_LOGI(TAG, "BLE transport: Connected!");
            break;
        case PROTOCOMM_TRANSPORT_BLE_DISCONNECTED:
            ESP_LOGI(TAG, "BLE transport: Disconnected!");
            break;
        default:
            break;
        }
    } else if(event_base == PROTOCOMM_SECURITY_SESSION_EVENT) {
        switch(event_id) {
        case PROTOCOMM_SECURITY_SESSION_SETUP_OK:
            ESP_LOGI(TAG, "Secured session established!");
            break;
        case PROTOCOMM_SECURITY_SESSION_INVALID_SECURITY_PARAMS:
            ESP_LOGE(TAG, "Received invalid security parameters for establishing secure session!");
            break;
        case PROTOCOMM_SECURITY_SESSION_CREDENTIALS_MISMATCH:
            ESP_LOGE(TAG, "Received incorrect username and/or PoP for establishing secure session!");
            break;
        default:
            break;
        }
    }
}

//////////////////////////////////////////////////////////////////////

void wifi_init_sta(void)
{
    /* Start Wi-Fi in station mode */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

//////////////////////////////////////////////////////////////////////

void get_device_service_name(char *service_name, size_t max)
{
    uint8_t eth_mac[6];
    const char *ssid_prefix = "PROV_";
    esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
    snprintf(service_name, max, "%s%02X%02X%02X", ssid_prefix, eth_mac[3], eth_mac[4], eth_mac[5]);
}

//////////////////////////////////////////////////////////////////////
// Handler for the optional provisioning endpoint registered by the application.
// The data format can be chosen by applications. Here, we are using plain ascii text.
// Applications can choose to use other formats like protobuf, JSON, XML, etc.

esp_err_t custom_prov_data_handler(uint32_t session_id, const uint8_t *inbuf, ssize_t inlen, uint8_t **outbuf, ssize_t *outlen, void *priv_data)
{
    if(inbuf) {
        ESP_LOGI(TAG, "Received data: %.*s", inlen, (char *)inbuf);
    }
    char response[] = "SUCCESS";
    *outbuf = (uint8_t *)strdup(response);
    if(*outbuf == NULL) {
        ESP_LOGE(TAG, "System out of memory");
        return ESP_ERR_NO_MEM;
    }
    *outlen = strlen(response) + 1; /* +1 for NULL terminating byte */

    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

// void lcd_qrcode_display(esp_qrcode_handle_t qrcode)
// {
//     ESP_LOGI(TAG, "lcd_qrcode_display");

//     int border = 2;
//     int max_size = LCD_WIDTH;
//     if(max_size > LCD_HEIGHT) {
//         max_size = LCD_HEIGHT;
//     }
//     max_size = static_cast<int>(max_size * (M_SQRT2 / 2.0f));

//     int qr_size = qrcodegen_getSize(qrcode) + border * 2;
//     int dot_size = max_size / qr_size;    // the divide rounds down
//     int image_size = qr_size * dot_size;

//     int xorg = (LCD_WIDTH - image_size) / 2;
//     int yorg = (LCD_HEIGHT - image_size) / 2;

//     uint16_t *lcd_buffer;
//     if(lcd_get_backbuffer(&lcd_buffer, portMAX_DELAY) == ESP_OK) {

//         ESP_LOGI(TAG, "Got backbuffer: %p", lcd_buffer);

//         memset(lcd_buffer, 0x0f, LCD_WIDTH * LCD_HEIGHT * sizeof(uint16_t));

//         uint16_t *row = lcd_buffer + xorg * LCD_WIDTH + yorg;

//         ESP_LOGI(TAG, "qr_size = %d", qr_size);

//         for(int y = 0; y < qr_size; ++y) {
//             uint16_t *col = row;
//             for(int x = 0; x < qr_size; ++x) {
//                 uint16_t p = qrcodegen_getModule(qrcode, x - border, y - border) ? 0x0000 : 0xffff;
//                 uint16_t *d = col;
//                 for(int iy = 0; iy < dot_size; ++iy) {
//                     for(int ix = 0; ix < dot_size; ++ix) {
//                         d[ix] = p;
//                     }
//                     d += LCD_WIDTH;
//                 }
//                 col += dot_size;
//             }
//             row += LCD_WIDTH * dot_size;
//         }
//         lcd_release_backbuffer_and_update();
//         lcd_set_backlight(8191);
//     }
//     ESP_LOGI(TAG, "done QR display");
// }

//////////////////////////////////////////////////////////////////////

static void wifi_prov_print_qr(const char *name, const char *username, const char *pop, const char *transport)
{
    if(!name || !transport) {
        ESP_LOGW(TAG, "Cannot generate QR code payload. Data missing.");
        return;
    }
    char payload[150] = { 0 };
    if(pop) {
        snprintf(payload, sizeof(payload),
                 "{\"ver\":\"%s\",\"name\":\"%s\""
                 ",\"username\":\"%s\",\"pop\":\"%s\",\"transport\":\"%s\"}",
                 PROV_QR_VERSION, name, username, pop, transport);
    } else {
        snprintf(payload, sizeof(payload),
                 "{\"ver\":\"%s\",\"name\":\"%s\""
                 ",\"transport\":\"%s\"}",
                 PROV_QR_VERSION, name, transport);
    }
    // esp_qrcode_config_t cfg = ESP_QRCODE_CONFIG_DEFAULT();
    // cfg.display_func = lcd_qrcode_display;
    // esp_qrcode_generate(&cfg, payload);
    ESP_LOGI(TAG, "Would show QR Code here...");
}

//////////////////////////////////////////////////////////////////////

void do_disconnect()
{
}

//////////////////////////////////////////////////////////////////////

void do_factory_reset()
{
    wifi_prov_mgr_reset_provisioning();
}

//////////////////////////////////////////////////////////////////////

void do_connect()
{
    wifi_prov_mgr_config_t config = {};

    /* What is the Provisioning Scheme that we want ?
     * wifi_prov_scheme_softap or wifi_prov_scheme_ble */
    config.scheme = wifi_prov_scheme_ble;

    // Any default scheme specific event handler that you would
    // like to choose. Since our example application requires
    // neither BT nor BLE, we can choose to release the associated
    // memory once provisioning is complete, or not needed
    // (in case when device is already provisioned). Choosing
    // appropriate scheme specific event handler allows the manager
    // to take care of this automatically. This can be set to
    // WIFI_PROV_EVENT_HANDLER_NONE when using wifi_prov_scheme_softap
    config.scheme_event_handler.event_cb = wifi_prov_scheme_ble_event_cb_free_btdm;
    config.scheme_event_handler.user_data = NULL;

    /* Initialize provisioning manager with the
     * configuration parameters set above */
    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

    ESP_LOGI(TAG, "wifi_prov_mgr_init complete");

    bool provisioned = false;

    ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));

    // If device is not yet provisioned start provisioning service
    if(!provisioned) {

        ESP_LOGI(TAG, "Starting provisioning");

        // What is the Device Service Name that we want
        // This translates to :
        //     - Wi-Fi SSID when scheme is wifi_prov_scheme_softap
        //     - device name when scheme is wifi_prov_scheme_ble

        char service_name[12];
        get_device_service_name(service_name, sizeof(service_name));

        wifi_prov_security_t security = WIFI_PROV_SECURITY_2;
        // The username must be the same one, which has been used in the generation of salt and verifier

#if CONFIG_EXAMPLE_PROV_SEC2_DEV_MODE

        // This pop field represents the password that will be used to generate salt and verifier.
        // The field is present here in order to generate the QR code containing password.
        // In production this password field shall not be stored on the device

        const char *username = EXAMPLE_PROV_SEC2_USERNAME;
        const char *pop = EXAMPLE_PROV_SEC2_PWD;

#elif CONFIG_EXAMPLE_PROV_SEC2_PROD_MODE
        /* The username and password shall not be embedded in the firmware,
         * they should be provided to the user by other means.
         * e.g. QR code sticker */
        const char *username = NULL;
        const char *pop = NULL;
#endif
        // This is the structure for passing security parameters
        // for the protocomm security 2.
        // If dynamically allocated, sec2_params pointer and its content
        // must be valid till WIFI_PROV_END event is triggered.

        wifi_prov_security2_params_t sec2_params = {};

        ESP_ERROR_CHECK(example_get_sec2_salt(&sec2_params.salt, &sec2_params.salt_len));
        ESP_ERROR_CHECK(example_get_sec2_verifier(&sec2_params.verifier, &sec2_params.verifier_len));

        wifi_prov_security2_params_t *sec_params = &sec2_params;
        /* What is the service key (could be NULL)
         * This translates to :
         *     - Wi-Fi password when scheme is wifi_prov_scheme_softap
         *          (Minimum expected length: 8, maximum 64 for WPA2-PSK)
         *     - simply ignored when scheme is wifi_prov_scheme_ble
         */
        const char *service_key = NULL;

        /* This step is only useful when scheme is wifi_prov_scheme_ble. This will
         * set a custom 128 bit UUID which will be included in the BLE advertisement
         * and will correspond to the primary GATT service that provides provisioning
         * endpoints as GATT characteristics. Each GATT characteristic will be
         * formed using the primary service UUID as base, with different auto assigned
         * 12th and 13th bytes (assume counting starts from 0th byte). The client side
         * applications must identify the endpoints by reading the User Characteristic
         * Description descriptor (0x2901) for each characteristic, which contains the
         * endpoint name of the characteristic */
        uint8_t custom_service_uuid[] = {
            /* LSB <---------------------------------------
             * ---------------------------------------> MSB */
            0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf, 0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02,
        };

        /* If your build fails with linker errors at this point, then you may have
         * forgotten to enable the BT stack or BTDM BLE settings in the SDK (e.g. see
         * the sdkconfig.defaults in the example project) */
        wifi_prov_scheme_ble_set_service_uuid(custom_service_uuid);

        /* An optional endpoint that applications can create if they expect to
         * get some additional custom data during provisioning workflow.
         * The endpoint name can be anything of your choice.
         * This call must be made before starting the provisioning.
         */
        wifi_prov_mgr_endpoint_create("custom-data");

        /* Do not stop and de-init provisioning even after success,
         * so that we can restart it later. */
#ifdef CONFIG_EXAMPLE_REPROVISIONING
        wifi_prov_mgr_disable_auto_stop(1000);
#endif
        /* Start provisioning service */
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, (const void *)sec_params, service_name, service_key));

        // The handler for the optional endpoint created above.
        // This call must be made after starting the provisioning, and only if the endpoint
        // has already been created above.

        wifi_prov_mgr_endpoint_register("custom-data", custom_prov_data_handler, NULL);

        // Print QR code for provisioning
        wifi_prov_print_qr(service_name, username, pop, PROV_TRANSPORT_BLE);

    } else {

        ESP_LOGI(TAG, "Already provisioned, starting Wi-Fi STA");

        // device is already provisioned, release prov_mgr resources
        wifi_prov_mgr_deinit();

        // Start Wi-Fi station (i.e. client) mode
        wifi_init_sta();
    }

    /* Start main application now */
#if CONFIG_EXAMPLE_REPROVISIONING
    while(1) {
        for(int i = 0; i < 10; i++) {
            ESP_LOGI(TAG, "Hello World!");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }

        /* Resetting provisioning state machine to enable re-provisioning */
        wifi_prov_mgr_reset_sm_state_for_reprovision();

        /* Wait for Wi-Fi connection */
        xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_EVENT, true, true, portMAX_DELAY);
    }
#else

#endif
}

//////////////////////////////////////////////////////////////////////

static void mywifi_task(void *param)
{
    static char const *TAG = "mywifi_task";
    esp_periph_handle_t periph = (esp_periph_handle_t)param;
    ESP_LOGI(TAG, "here we go...");
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_event_group = xEventGroupCreate();

    ESP_LOGI(TAG, "wifi init 2");

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(PROTOCOMM_TRANSPORT_BLE_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    ESP_LOGI(TAG, "wifi init 3");

    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_LOGI(TAG, "wifi init 4");

    ESP_LOGI(TAG, "wifi init complete");

    do_connect();

    while(true) {

        uint32_t got_bits = xEventGroupWaitBits(wifi_event_group, WIFI_ANY_BIT, true, false, portMAX_DELAY);

        ESP_LOGI(TAG, "GOT:%08lx", got_bits);

        if(got_bits & WIFI_CONNECTED_EVENT) {
            esp_periph_send_cmd(periph, PERIPH_MYWIFI_CONNECTED, NULL, 0);
        }

        if(got_bits & WIFI_DISCONNECTED_EVENT) {
            esp_periph_send_cmd(periph, PERIPH_MYWIFI_DISCONNECTED, NULL, 0);
        }
    }
}

//////////////////////////////////////////////////////////////////////

esp_err_t mywifi_init(mywifi_config_t *cfg, esp_mywifi_handle_t *handle)
{
    ESP_LOGI(TAG, "init");

    esp_mywifi_handle_t mywifi = (esp_mywifi_handle_t)audio_calloc(1, sizeof(struct esp_mywifi));

    xTaskCreate(mywifi_task, "mywifi", 4096, mywifi, 10, NULL);

    *handle = mywifi;

    ESP_LOGI(TAG, "init done");

    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

esp_err_t mywifi_destroy(esp_mywifi_handle_t mywifi)
{
    ESP_LOGI(TAG, "destroy");
    audio_free(mywifi);
    return ESP_OK;
}
