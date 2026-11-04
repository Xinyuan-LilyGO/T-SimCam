#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include "config.h"

SPIClass sdSpi(FSPI);

void setup() {
  pinMode(BOARD_POWER_ON_PIN, OUTPUT);
  digitalWrite(BOARD_POWER_ON_PIN, HIGH);
  Serial.begin(115200);
  delay(1500);
  Serial.printf("\nT-SimCam microSD test - %s\n", T_SIMCAM_BOARD_NAME);

  sdSpi.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  if (!SD.begin(SD_CS_PIN, sdSpi)) {
    Serial.println("[FAIL] microSD mount failed");
    return;
  }
  if (SD.cardType() == CARD_NONE) {
    Serial.println("[FAIL] no microSD card detected");
    return;
  }

  const char *path = "/t_simcam_test.txt";
  File file = SD.open(path, FILE_WRITE);
  if (!file || file.println("T-SimCam microSD test") == 0) {
    Serial.println("[FAIL] file write failed");
    return;
  }
  file.close();

  file = SD.open(path, FILE_READ);
  String content = file ? file.readString() : String();
  file.close();
  SD.remove(path);

  Serial.printf("[INFO] card size: %llu MB\n", SD.cardSize() / (1024ULL * 1024ULL));
  Serial.println(content.indexOf("T-SimCam") >= 0
                   ? "[PASS] microSD read/write verified"
                   : "[FAIL] file content verification failed");
}

void loop() {
  delay(1000);
}
