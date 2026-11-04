#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <WiFi.h>
#include "config.h"

void setup() {
  pinMode(BOARD_POWER_ON_PIN, OUTPUT);
  digitalWrite(BOARD_POWER_ON_PIN, HIGH);
  Serial.begin(115200);
  delay(1500);
  Serial.printf("\nT-SimCam wireless scan - %s\n", T_SIMCAM_BOARD_NAME);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  const int wifiCount = WiFi.scanNetworks();
  if (wifiCount < 0) {
    Serial.printf("[FAIL] Wi-Fi scan error: %d\n", wifiCount);
  } else {
    Serial.printf("[PASS] Wi-Fi scan completed: %d network(s)\n", wifiCount);
    for (int i = 0; i < wifiCount; ++i) {
      Serial.printf("  %2d. %-24s %4d dBm\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
    }
  }
  WiFi.scanDelete();
  WiFi.mode(WIFI_OFF);

  if (!BLEDevice::init("")) {
    Serial.println("[FAIL] BLE initialization failed");
    return;
  }
  BLEScan *scan = BLEDevice::getScan();
  scan->setActiveScan(true);
  BLEScanResults *results = scan->start(5, false);
  if (!results) {
    Serial.println("[FAIL] BLE scan failed");
  } else {
    Serial.printf("[PASS] BLE scan completed: %d device(s)\n", results->getCount());
    scan->clearResults();
  }
}

void loop() {
  delay(1000);
}
