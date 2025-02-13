#include "esp_camera.h"
#include "esp_http_server.h"
#include <WiFi.h>
#include <ESPmDNS.h>
// WiFi credentials remain the same
const char* ssid = "World Wide Web";
const char* password = "ablecapital114";

// Camera pins remain the same
#define PWDN_GPIO_NUM    32
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM     0
#define SIOD_GPIO_NUM    26
#define SIOC_GPIO_NUM    27
#define Y9_GPIO_NUM      35
#define Y8_GPIO_NUM      34
#define Y7_GPIO_NUM      39
#define Y6_GPIO_NUM      36
#define Y5_GPIO_NUM      21
#define Y4_GPIO_NUM      19
#define Y3_GPIO_NUM      18
#define Y2_GPIO_NUM       5
#define VSYNC_GPIO_NUM   25
#define HREF_GPIO_NUM    23
#define PCLK_GPIO_NUM    22

// Define stream boundaries
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t stream_httpd = NULL;

static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    char *part_buf[64];

    res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
    if (res != ESP_OK) {
        return res;
    }

    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.println("Camera capture failed");
            res = ESP_FAIL;
        } else {
            _jpg_buf_len = fb->len;
            _jpg_buf = fb->buf;
        }

        if (res == ESP_OK) {
            size_t hlen = snprintf((char *)part_buf, 64, STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
            if (res == ESP_OK) {
                res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
            }
            if (res == ESP_OK) {
                res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
            }
        }

        if (fb) {
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        }

        if (res != ESP_OK) {
            break;
        }
    }

    return res;
}

void startCameraServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    httpd_uri_t stream_uri = {
        .uri       = "/stream",
        .method    = HTTP_GET,
        .handler   = stream_handler,
        .user_ctx  = NULL
    };

    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(stream_httpd, &stream_uri);
    }

    // Also serve a simple HTML page
    httpd_uri_t index_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = [](httpd_req_t *req) {
            const char* html = "<html><head><title>ESP32-CAM Stream</title>"
                             "<style>body{background-color:black;text-align:center;} "
                             "h1{color:white;}</style></head>"
                             "<body><h1>ESP32-CAM Stream</h1>"
                             "<img src='/stream' style='width:640px;height:480px;'/>"
                             "</body></html>";
            httpd_resp_set_type(req, "text/html");
            httpd_resp_send(req, html, strlen(html));
            return ESP_OK;
        },
        .user_ctx  = NULL
    };
    httpd_register_uri_handler(stream_httpd, &index_uri);
}

void setup() {
    Serial.begin(115200);
    
    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("WiFi connected");

    if (!MDNS.begin()) {
      Serial.println("Error starting mDNS responder");
    } else {
      Serial.println("mDNS responder started!");
      Serial.println("Now ready at http://esp32cam.local");
    }


    // Camera configuration
    camera_config_t config;
    config.ledc_channel   = LEDC_CHANNEL_0;
    config.ledc_timer     = LEDC_TIMER_0;
    config.pin_d0         = Y2_GPIO_NUM;
    config.pin_d1         = Y3_GPIO_NUM;
    config.pin_d2         = Y4_GPIO_NUM;
    config.pin_d3         = Y5_GPIO_NUM;
    config.pin_d4         = Y6_GPIO_NUM;
    config.pin_d5         = Y7_GPIO_NUM;
    config.pin_d6         = Y8_GPIO_NUM;
    config.pin_d7         = Y9_GPIO_NUM;
    config.pin_xclk       = XCLK_GPIO_NUM;
    config.pin_pclk       = PCLK_GPIO_NUM;
    config.pin_vsync      = VSYNC_GPIO_NUM;
    config.pin_href       = HREF_GPIO_NUM;
    config.pin_sscb_sda   = SIOD_GPIO_NUM;
    config.pin_sscb_scl   = SIOC_GPIO_NUM;
    config.pin_pwdn       = PWDN_GPIO_NUM;
    config.pin_reset      = RESET_GPIO_NUM;
    config.xclk_freq_hz   = 20000000;
    config.pixel_format   = PIXFORMAT_JPEG;
    
    if (psramFound()) {
        config.frame_size = FRAMESIZE_VGA;
        config.jpeg_quality = 24;  // Adjust based on your bandwidth needs
        config.fb_count = 2;
    } else {
        config.frame_size = FRAMESIZE_SVGA;
        config.jpeg_quality = 12;
        config.fb_count = 1;
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x", err);
        return;
    }

    startCameraServer();
    Serial.print("Camera Stream Ready! Go to: http://");
    Serial.println(WiFi.localIP());
}

void loop() {
    // Nothing needed in loop for streaming
    delay(1);
}