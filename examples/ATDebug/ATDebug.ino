#include <Arduino.h>
#include "config.h"

HardwareSerial ModemSerial(1);
int lastStatus = -1;

void powerOnModem() {
  pinMode(MODEM_POWER_KEY_PIN, OUTPUT);
  digitalWrite(MODEM_POWER_KEY_PIN, HIGH);
  delay(500);
  digitalWrite(MODEM_POWER_KEY_PIN, LOW);
}

void setup() {
  pinMode(BOARD_POWER_ON_PIN, OUTPUT);
  digitalWrite(BOARD_POWER_ON_PIN, HIGH);
  pinMode(MODEM_STATUS_PIN, INPUT);
  Serial.begin(115200);
  ModemSerial.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
  delay(1500);

  Serial.printf("\nT-SimCam modem AT debugger - %s\n", T_SIMCAM_BOARD_NAME);
  Serial.printf("Modem UART: TX GPIO %d, RX GPIO %d, 115200 8N1\n",
                MODEM_TX_PIN, MODEM_RX_PIN);
  Serial.println("Powering on modem. Type AT commands in the serial monitor.");
  powerOnModem();
  lastStatus = digitalRead(MODEM_STATUS_PIN);
  Serial.printf("[MODEM STATUS] GPIO %d = %s\n", MODEM_STATUS_PIN,
                lastStatus ? "HIGH" : "LOW");
}

void loop() {
  while (Serial.available()) {
    ModemSerial.write(Serial.read());
  }
  while (ModemSerial.available()) {
    Serial.write(ModemSerial.read());
  }
  const int status = digitalRead(MODEM_STATUS_PIN);
  if (status != lastStatus) {
    lastStatus = status;
    Serial.printf("\n[MODEM STATUS] %s\n", status ? "HIGH" : "LOW");
  }
  delay(1);
}
