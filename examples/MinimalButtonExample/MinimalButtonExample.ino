#include <Arduino.h>
#include "config.h"

bool lastButtonState = HIGH;

void setup() {
  pinMode(BOARD_POWER_ON_PIN, OUTPUT);
  digitalWrite(BOARD_POWER_ON_PIN, HIGH);
  pinMode(USER_BUTTON_PIN, INPUT_PULLUP);

  Serial.begin(115200);
  delay(1500);
  Serial.printf("\nT-SimCam button test - %s\n", T_SIMCAM_BOARD_NAME);
  Serial.println("Press BOOT to see state changes.");
  lastButtonState = digitalRead(USER_BUTTON_PIN);
}

void loop() {
  const bool buttonState = digitalRead(USER_BUTTON_PIN);
  if (buttonState != lastButtonState) {
    delay(20);
    if (buttonState == digitalRead(USER_BUTTON_PIN)) {
      lastButtonState = buttonState;
      Serial.printf("[BUTTON] %s\n", buttonState == LOW ? "PRESSED" : "RELEASED");
    }
  }
  delay(5);
}
