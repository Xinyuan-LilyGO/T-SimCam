#include <Arduino.h>
#include "config.h"

HardwareSerial AuxSerial(2);

void setup() {
  pinMode(BOARD_POWER_ON_PIN, OUTPUT);
  digitalWrite(BOARD_POWER_ON_PIN, HIGH);
  Serial.begin(115200);
  AuxSerial.begin(115200, SERIAL_8N1, AUX_UART_RX_PIN, AUX_UART_TX_PIN);
  delay(1500);
  Serial.printf("\nT-SimCam auxiliary UART bridge - %s\n", T_SIMCAM_BOARD_NAME);
  Serial.printf("USB <-> UART2, TX GPIO %d, RX GPIO %d, 115200 8N1\n",
                AUX_UART_TX_PIN, AUX_UART_RX_PIN);
}

void loop() {
  while (Serial.available()) {
    AuxSerial.write(Serial.read());
  }
  while (AuxSerial.available()) {
    Serial.write(AuxSerial.read());
  }
  delay(1);
}
