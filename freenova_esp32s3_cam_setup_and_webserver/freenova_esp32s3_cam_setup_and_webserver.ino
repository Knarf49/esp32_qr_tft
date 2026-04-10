#include "esp_camera.h"
#include <WiFi.h>

// ===================
// Select camera model
// ===================
//#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
//#define CAMERA_MODEL_ESP_EYE // Has PSRAM
#define CAMERA_MODEL_ESP32S3_EYE  // Has PSRAM
//#define CAMERA_MODEL_M5STACK_PSRAM // Has PSRAM
//#define CAMERA_MODEL_M5STACK_V2_PSRAM // M5Camera version B Has PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_ESP32CAM // No PSRAM
//#define CAMERA_MODEL_M5STACK_UNITCAM // No PSRAM
//#define CAMERA_MODEL_AI_THINKER // Has PSRAM
//#define CAMERA_MODEL_TTGO_T_JOURNAL // No PSRAM
// ** Espressif Internal Boards **
//#define CAMERA_MODEL_ESP32_CAM_BOARD
//#define CAMERA_MODEL_ESP32S2_CAM_BOARD
//#define CAMERA_MODEL_ESP32S3_CAM_LCD

#include "camera_pins.h"

// ===========================
// Enter your WiFi credentials
// ===========================
const char* ssid = "V30";
const char* password = "Zenzenzense";
const char* PC_IP = "10.124.225.38";
const uint16_t PORT = 5000;

WiFiClient client;
// void startCameraServer();

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000;
  config.frame_size = FRAMESIZE_QVGA;
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  // for larger pre-allocated frame buffer.
  if (psramFound()) {
    config.jpeg_quality = 10;
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
    Serial.println("PSRAM FOUND");
  } else {
    // Limit the frame size when PSRAM is not available
    config.frame_size = FRAMESIZE_HVGA;
    config.fb_location = CAMERA_FB_IN_DRAM;
    Serial.println("NO PSRAM FOUND");
  }

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  // s->set_vflip(s, 1); // flip it back
  // s->set_brightness(s, 1); // up the brightness just a bit
  // s->set_saturation(s, -1); // lower the saturation

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected: " + WiFi.localIP().toString());

  // TCP connect — must be BEFORE loop() starts
  Serial.println("Connecting to server...");
  while (!client.connect(PC_IP, PORT)) {
    Serial.println("Retrying...");
    delay(500);
  }
  client.setNoDelay(true);
  Serial.println("Server connected!");  // ← you should see this
}

void loop() {
  if (!client.connected()) {
    Serial.println("Reconnecting to server...");
    client.connect(PC_IP, PORT);
    client.setNoDelay(true);
    return;
  }

  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Capture failed");
    return;
  }

  // Copy the data first
  size_t len = fb->len;
  uint8_t* buf = (uint8_t*)malloc(len);
  if (!buf) {
    esp_camera_fb_return(fb);  // return immediately if malloc fails
    return;
  }
  memcpy(buf, fb->buf, len);
  esp_camera_fb_return(fb);   // ← return buffer BEFORE sending over WiFi
                               //    this is critical — frees buffer for next capture

  // Now send over TCP
  uint8_t header[4] = {
    (uint8_t)(len >> 24), (uint8_t)(len >> 16),
    (uint8_t)(len >> 8),  (uint8_t)(len)
  };
  client.write(header, 4);
  client.write(buf, len);
  free(buf);

  // Read response
  uint32_t timeout = millis();
  while (client.available() < 4) {
    if (millis() - timeout > 3000) return;
  }
  uint8_t rhdr[4];
  client.read(rhdr, 4);
  uint32_t rlen = ((uint32_t)rhdr[0] << 24) | ((uint32_t)rhdr[1] << 16)
                | ((uint32_t)rhdr[2] << 8)  |  (uint32_t)rhdr[3];

  String result = "";
  timeout = millis();
  while (result.length() < rlen) {
    if (client.available()) result += (char)client.read();
    if (millis() - timeout > 3000) break;
  }

  if (result != "NONE" && result != "" && !result.startsWith("ERROR")) {
    Serial.println("Scanned: " + result);
  }
}