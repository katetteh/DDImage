// ============================================================
// DDImage - ESP32-CAM real-time side-by-side viewer (example)
//
// Left half  = original camera frame
// Right half = the same frame after YOUR filter
// Both halves come from ONE capture, so they are always in sync.
//
// Put this file in a folder named esp32_cam_realtime/ (Arduino
// requires the folder name to match the .ino name), e.g.
//   examples/esp32_cam_realtime/esp32_cam_realtime.ino
//
// Needs:
//   - ESP32 board package (includes the esp32-camera driver)
//   - Tools -> PSRAM: Enabled   (the frame buffers need it)
//   - ddimage/ImageProcessor.h  (RGBImage + your filters)
// ============================================================

#include "esp_camera.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include <WiFi.h>
#include <string.h>
#include <stdlib.h>

#include <ddimage/matrix.h>
#include <ddimage/ImageProcessor.h>   // RGBImage + your filters


// ---------------- Settings you will likely tweak ----------------
const char* WIFI_SSID     = "your_wifi_name";
const char* WIFI_PASSWORD = "your_wifi_password";

#define FRAME_SIZE      FRAMESIZE_QQVGA   // 160x120. Try FRAMESIZE_QVGA (320x240) once it works.
#define JPEG_QUALITY    70                // 1-100, lower = smaller/faster
#define SWAP_RGB565_BYTES 0               // set to 1 if colors look wrong (byte order)

// ---------------- AI-Thinker ESP32-CAM pins ----------------
// Other boards (M5Stack, TTGO, etc.) use different pins: change these.
#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM   26
#define SIOC_GPIO_NUM   27
#define Y9_GPIO_NUM     35
#define Y8_GPIO_NUM     34
#define Y7_GPIO_NUM     39
#define Y6_GPIO_NUM     36
#define Y5_GPIO_NUM     21
#define Y4_GPIO_NUM     19
#define Y3_GPIO_NUM     18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM   23
#define PCLK_GPIO_NUM   22

// ---------------- Streaming constants ----------------
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BOUNDARY     = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART         = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

httpd_handle_t server_httpd = NULL;

// ============================================================
// >>> THE ONLY FUNCTION YOU NEED TO EDIT TO TEST A FILTER <<<
// Must return an image the SAME size as the input.
// Swap in boxBlur, threshold, Sobel, etc. as you build them.
// (Adjust to your real API: member function vs free function.)
// ============================================================
RGBImage applyFilter(const RGBImage& in) {
    return in.boxBlur(5);
}

// ============================================================
// Flat RGB565 camera buffer -> RGBImage
// ============================================================
RGBImage fromCameraBufferRGB565(const uint8_t* flat, int width, int height) {
    RGBImage img(width, height);
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            int i = (row * width + col) * 2;
#if SWAP_RGB565_BYTES
            uint16_t p = (flat[i + 1] << 8) | flat[i];
#else
            uint16_t p = (flat[i] << 8) | flat[i + 1];
#endif
            uint8_t r5 = (p >> 11) & 0x1F;
            uint8_t g6 = (p >> 5)  & 0x3F;
            uint8_t b5 =  p        & 0x1F;
            img.setPixel(row, col,
                         (r5 * 255) / 31,
                         (g6 * 255) / 63,
                         (b5 * 255) / 31);
        }
    }
    return img;
}

// ============================================================
// Paste two same-size images side by side into one interleaved
// RGB buffer (R,G,B,R,G,B,...) of size (2*w) x h x 3.
// ============================================================
static bool composeSideBySide(const RGBImage& left, const RGBImage& right, uint8_t* out) {
    int w = left.width(), h = left.height();
    if (right.width() != w || right.height() != h) return false;

    size_t stride = (size_t)w * 2 * 3;
    for (int row = 0; row < h; row++) {
        uint8_t* line = out + row * stride;
        for (int col = 0; col < w; col++) {
            auto a = left.getPixel(row, col);
            auto b = right.getPixel(row, col);
            line[col * 3 + 0] = a.r;
            line[col * 3 + 1] = a.g;
            line[col * 3 + 2] = a.b;
            line[(w + col) * 3 + 0] = b.r;
            line[(w + col) * 3 + 1] = b.g;
            line[(w + col) * 3 + 2] = b.b;
        }
    }
    return true;
}

// ============================================================
// JPEG encoding into a preallocated buffer (no realloc per chunk,
// which matters because stb writes in many tiny pieces).
// ============================================================
struct JpegSink {
    uint8_t* buf;
    size_t   cap;
    size_t   len;
    bool     overflow;
};

static void jpegSinkWrite(void* ctx, void* data, int size) {
    JpegSink* s = (JpegSink*)ctx;
    if (s->len + (size_t)size > s->cap) { s->overflow = true; return; }
    memcpy(s->buf + s->len, data, size);
    s->len += size;
}

static void* allocBig(size_t n) {
    void* p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM);   // prefer PSRAM
    if (!p) p = malloc(n);
    return p;
}

// ============================================================
// /stream : capture -> convert -> filter -> side-by-side -> JPEG -> send
// Runs for as long as the browser stays connected.
// ============================================================
static esp_err_t stream_handler(httpd_req_t* req) {
    esp_err_t res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
    if (res != ESP_OK) return res;

    uint8_t* wide = nullptr;
    size_t   wideSize = 0;
    JpegSink sink = { nullptr, 0, 0, false };
    char     part_buf[64];
    uint32_t frames = 0, t0 = millis();

    while (true) {
        // 1. Grab the newest frame
        camera_fb_t* fb = esp_camera_fb_get();
        if (!fb) { res = ESP_FAIL; break; }
        if (fb->format != PIXFORMAT_RGB565) {
            esp_camera_fb_return(fb);
            Serial.println("Camera is not in RGB565 mode");
            res = ESP_FAIL;
            break;
        }
        int w = fb->width, h = fb->height;

        // 2. Raw buffer -> your RGBImage, then give the buffer back ASAP
        RGBImage left = fromCameraBufferRGB565(fb->buf, w, h);
        esp_camera_fb_return(fb);

        // 3. Your filter
        RGBImage right = applyFilter(left);

        // 4. (Re)allocate working buffers only when the frame size changes
        size_t needed = (size_t)w * 2 * h * 3;
        if (needed != wideSize) {
            free(wide);
            free(sink.buf);
            wide = (uint8_t*)allocBig(needed);
            sink.cap = needed / 2;                       // generous upper bound for the JPEG
            sink.buf = (uint8_t*)allocBig(sink.cap);
            wideSize = needed;
            if (!wide || !sink.buf) { Serial.println("Out of memory"); res = ESP_FAIL; break; }
        }

        // 5. Side by side, then JPEG-encode once
        if (!composeSideBySide(left, right, wide)) {
            Serial.println("applyFilter() must return the same size as its input");
            res = ESP_FAIL;
            break;
        }
        sink.len = 0;
        sink.overflow = false;
        stbi_write_jpg_to_func(jpegSinkWrite, &sink, w * 2, h, 3, wide, JPEG_QUALITY);
        if (sink.overflow || sink.len == 0) continue;    // skip a bad frame, keep streaming

        // 6. Send. A failed send means the browser left, so stop and free everything.
        res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
        if (res == ESP_OK) {
            size_t hlen = snprintf(part_buf, sizeof(part_buf), STREAM_PART, (unsigned)sink.len);
            res = httpd_resp_send_chunk(req, part_buf, hlen);
        }
        if (res == ESP_OK) {
            res = httpd_resp_send_chunk(req, (const char*)sink.buf, sink.len);
        }
        if (res != ESP_OK) break;

        // FPS readout every 30 frames
        if (++frames % 30 == 0) {
            float fps = 30000.0f / (millis() - t0);
            Serial.printf("%.1f FPS (%dx%d per side)\n", fps, w, h);
            t0 = millis();
        }
    }

    free(wide);
    free(sink.buf);
    return res;
}

// ============================================================
// / : page with a single image tag pointing at the stream
// ============================================================
static esp_err_t index_handler(httpd_req_t* req) {
    static const char* html =
        "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>DDImage ESP32-CAM</title>"
        "<style>body{font-family:sans-serif;background:#111;color:#eee;text-align:center;margin:0;padding:12px}"
        "img{width:100%;max-width:960px;image-rendering:pixelated}"
        ".labels{display:flex;max-width:960px;margin:0 auto}"
        ".labels div{flex:1}</style></head><body>"
        "<h3>DDImage live view</h3>"
        "<div class='labels'><div>Original</div><div>Filtered</div></div>"
        "<img src='/stream'>"
        "</body></html>";
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, html, strlen(html));
}

void startCameraServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.stack_size  = 16384;   // stb's JPEG encoder + your filters need more than the 4 KB default

    httpd_uri_t index_uri  = { .uri = "/",       .method = HTTP_GET, .handler = index_handler,  .user_ctx = NULL };
    httpd_uri_t stream_uri = { .uri = "/stream", .method = HTTP_GET, .handler = stream_handler, .user_ctx = NULL };

    if (httpd_start(&server_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(server_httpd, &index_uri);
        httpd_register_uri_handler(server_httpd, &stream_uri);
    }
}

// ============================================================
// Setup / loop
// ============================================================
void setup() {
    Serial.begin(115200);
    Serial.println();

    camera_config_t config = {};
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk  = XCLK_GPIO_NUM;
    config.pin_pclk  = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href  = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;   // older board packages call these pin_sscb_sda / pin_sscb_scl
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn  = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_RGB565;   // raw pixels for your pipeline
    config.frame_size   = FRAME_SIZE;
    config.jpeg_quality = 12;                 // unused for RGB565 but must be set

    if (psramFound()) {
        config.fb_count    = 2;                    // capture next frame while we process this one
        config.fb_location = CAMERA_FB_IN_PSRAM;
        config.grab_mode   = CAMERA_GRAB_LATEST;   // always the freshest frame, never a stale queued one
    } else {
        Serial.println("WARNING: no PSRAM found. Enable it in Tools, or stay at QQVGA.");
        config.fb_count    = 1;
        config.fb_location = CAMERA_FB_IN_DRAM;
        config.grab_mode   = CAMERA_GRAB_WHEN_EMPTY;
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed (0x%x)\n", err);
        return;
    }

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    WiFi.setSleep(false);   // Wi-Fi power saving adds latency to streaming
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.print("\nOpen in your browser: http://");
    Serial.println(WiFi.localIP());

    startCameraServer();
}

void loop() {
    delay(10000);   // the HTTP server runs in its own task; nothing to do here
}
