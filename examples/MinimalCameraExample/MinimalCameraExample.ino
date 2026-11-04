#include <Arduino.h>
#include <esp_camera.h>
#include "config.h"

camera_config_t makeCameraConfig() {
  camera_config_t config = {};
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
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = psramFound() ? FRAMESIZE_UXGA : FRAMESIZE_SVGA;
  config.jpeg_quality = 12;
  config.fb_count = psramFound() ? 2 : 1;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
  return config;
}

void setup() {
  pinMode(BOARD_POWER_ON_PIN, OUTPUT);
  digitalWrite(BOARD_POWER_ON_PIN, HIGH);
  Serial.begin(115200);
  delay(1500);
  Serial.printf("\nT-SimCam camera test - %s\n", T_SIMCAM_BOARD_NAME);

  camera_config_t config = makeCameraConfig();
  const esp_err_t error = esp_camera_init(&config);
  while (error != ESP_OK) {
    Serial.printf("[FAIL] camera initialization error: 0x%x\n", error);
    delay(1000);
  }
  sensor_t *sensor = esp_camera_sensor_get();
  Serial.printf("[PASS] camera detected, PID=0x%04x\n", sensor ? sensor->id.PID : 0);
}

void loop() {
  const uint32_t start = millis();
  camera_fb_t *frame = esp_camera_fb_get();
  if (!frame) {
    Serial.println("[FAIL] frame capture failed");
    delay(1000);
    return;
  }
  Serial.printf("[FRAME] %ux%u, %u bytes, %lu ms\n", frame->width, frame->height,
                frame->len, static_cast<unsigned long>(millis() - start));
  esp_camera_fb_return(frame);
  delay(1000);
}
