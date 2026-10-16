#include <string.h>
#include "esp_camera.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "secrets.h"

static const char *TAG = "cam_test";

/* Pins: LilyGO T-Camera S3 (OV2640). Verify against your board revision. */
#define PIN_PWDN  -1
#define PIN_RESET 39
#define PIN_XCLK  38
#define PIN_SIOD  5
#define PIN_SIOC  4
#define PIN_D0    14  /* Y2 */
#define PIN_D1    47  /* Y3 */
#define PIN_D2    48  /* Y4 */
#define PIN_D3    21  /* Y5 */
#define PIN_D4    13  /* Y6 */
#define PIN_D5    11  /* Y7 */
#define PIN_D6    10  /* Y8 */
#define PIN_D7    9   /* Y9 */
#define PIN_VSYNC 8
#define PIN_HREF  18
#define PIN_PCLK  12

#define GOT_IP_BIT BIT0
static EventGroupHandle_t s_events;

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "Wi-Fi disconnected, retrying");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "Open http://" IPSTR "/", IP2STR(&e->ip_info.ip));
        xEventGroupSetBits(s_events, GOT_IP_BIT);
    }
}

static void wifi_start(void)
{
    s_events = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL));

    wifi_config_t wc = {0};
    strncpy((char *)wc.sta.ssid, WIFI_SSID, sizeof(wc.sta.ssid) - 1);
    strncpy((char *)wc.sta.password, WIFI_PASS, sizeof(wc.sta.password) - 1);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
    xEventGroupWaitBits(s_events, GOT_IP_BIT, pdFALSE, pdTRUE, portMAX_DELAY);
}

static void camera_start(void)
{
    camera_config_t c = {
        .pin_pwdn = PIN_PWDN, .pin_reset = PIN_RESET, .pin_xclk = PIN_XCLK,
        .pin_sccb_sda = PIN_SIOD, .pin_sccb_scl = PIN_SIOC,
        .pin_d0 = PIN_D0, .pin_d1 = PIN_D1, .pin_d2 = PIN_D2, .pin_d3 = PIN_D3,
        .pin_d4 = PIN_D4, .pin_d5 = PIN_D5, .pin_d6 = PIN_D6, .pin_d7 = PIN_D7,
        .pin_vsync = PIN_VSYNC, .pin_href = PIN_HREF, .pin_pclk = PIN_PCLK,
        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_VGA,
        .jpeg_quality = 12,
        .fb_count = 2,
        .fb_location = CAMERA_FB_IN_PSRAM,
        .grab_mode = CAMERA_GRAB_LATEST,
    };
    ESP_ERROR_CHECK(esp_camera_init(&c));
    ESP_LOGI(TAG, "Camera initialised");
}

static esp_err_t index_handler(httpd_req_t *req)
{
    const char *html = "<html><body style='margin:0;background:#111'>"
                       "<img src='/capture' style='max-width:100%'>"
                       "<p style='color:#ccc'><a href='/' style='color:#8cf'>refresh</a></p>"
                       "</body></html>";
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t capture_handler(httpd_req_t *req)
{
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    httpd_resp_set_type(req, "image/jpeg");
    esp_err_t r = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    ESP_LOGI(TAG, "Sent %dx%d, %u bytes", fb->width, fb->height, (unsigned)fb->len);
    esp_camera_fb_return(fb);
    return r;
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    camera_start();
    wifi_start();

    httpd_handle_t server = NULL;
    httpd_config_t hc = HTTPD_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(httpd_start(&server, &hc));
    httpd_uri_t idx = { .uri = "/", .method = HTTP_GET, .handler = index_handler };
    httpd_uri_t cap = { .uri = "/capture", .method = HTTP_GET, .handler = capture_handler };
    httpd_register_uri_handler(server, &idx);
    httpd_register_uri_handler(server, &cap);
}
