#include <Arduino.h>
#include "config.h"

bool irCutEnabled = false;

void setup() {
  pinMode(BOARD_POWER_ON_PIN, OUTPUT);
  digitalWrite(BOARD_POWER_ON_PIN, HIGH);
  pinMode(IR_CUT_PIN, OUTPUT);

  Serial.begin(115200);
  delay(1500);
  Serial.printf("\nT-SimCam IR-cut test - %s\n", T_SIMCAM_BOARD_NAME);
  Serial.println("The IR-cut output changes every two seconds.");
}

void loop() {
  irCutEnabled = !irCutEnabled;
  digitalWrite(IR_CUT_PIN, irCutEnabled);
  Serial.printf("[IR CUT] %s (GPIO %d = %s)\n", irCutEnabled ? "ON" : "OFF",
                IR_CUT_PIN, irCutEnabled ? "HIGH" : "LOW");
  delay(2000);
}
