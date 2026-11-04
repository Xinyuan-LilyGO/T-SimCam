#include <Arduino.h>
#include <WiFi.h>
#include <esp_camera.h>
#include <esp_mac.h>

#include "camera_web_server.h"
#include "config.h"

namespace {

constexpr char AP_PREFIX[] = "T-SimCam-Cam";
constexpr uint32_t STATUS_INTERVAL_MS = 1000;

String apSsid;
String cameraUrl;
esp_err_t cameraError = ESP_OK;
bool hotspotReady = false;
bool cameraReady = false;
bool serverReady = false;
uint32_t lastStatus = 0;

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

void startHotspot() {
  uint8_t mac[6] = {};
  const esp_err_t macResult = esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
  bool validMac = macResult == ESP_OK;
  for (uint8_t byte : mac) validMac = validMac && byte != 0;
  if (validMac) {
    char name[33];
    snprintf(name, sizeof(name), "%s-%02X%02X", AP_PREFIX, mac[4], mac[5]);
    apSsid = name;
  } else {
    const uint64_t efuseMac = ESP.getEfuseMac();
    char name[33];
    snprintf(name, sizeof(name), "%s-%04X", AP_PREFIX,
             static_cast<unsigned int>(efuseMac & 0xFFFF));
    apSsid = name;
  }

  WiFi.mode(WIFI_AP);
  hotspotReady = WiFi.softAP(apSsid.c_str());
  if (hotspotReady) {
    cameraUrl = "http://" + WiFi.softAPIP().toString();
    Serial.printf("[PASS] Camera hotspot started: %s (open network)\n",
                  apSsid.c_str());
  }
}

void initializeCamera() {
  const camera_config_t config = makeCameraConfig();
  cameraError = esp_camera_init(&config);
  cameraReady = cameraError == ESP_OK;
  if (cameraReady) {
    sensor_t *sensor = esp_camera_sensor_get();
    Serial.printf("[PASS] Camera initialized, PID=0x%04x\n",
                  sensor ? sensor->id.PID : 0);
  }
}

void printStatus() {
  if (!hotspotReady) {
    Serial.println("[FAIL] Camera hotspot failed to start; check Wi-Fi/AP hardware");
  }
  if (!cameraReady) {
    Serial.printf("[FAIL] Camera initialization failed: 0x%08x (%s)\n",
                  static_cast<unsigned int>(cameraError),
                  esp_err_to_name(cameraError));
  }
  if (cameraReady && !serverReady) {
    Serial.println("[FAIL] Camera web server failed to start");
  }
  if (!hotspotReady || !cameraReady || !serverReady) return;

  Serial.printf("[HOTSPOT] Connect to Wi-Fi: %s (open, no password)\n",
                apSsid.c_str());
  Serial.printf("[HOTSPOT] Open camera viewer: %s\n", cameraUrl.c_str());
}

}  // namespace

void setup() {
  pinMode(BOARD_POWER_ON_PIN, OUTPUT);
  digitalWrite(BOARD_POWER_ON_PIN, HIGH);
  Serial.begin(115200);
  delay(1500);
  Serial.printf("\nT-SimCam camera web server - %s\n", T_SIMCAM_BOARD_NAME);

  startHotspot();
  initializeCamera();
  if (cameraReady) {
    serverReady = startCameraWebServer();
  }
  printStatus();
  lastStatus = millis();
}

void loop() {
  const uint32_t now = millis();
  if (now - lastStatus >= STATUS_INTERVAL_MS) {
    lastStatus = now;
    printStatus();
  }
  delay(10);
}
