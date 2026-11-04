#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <ESP_I2S.h>
#include <SD.h>
#include <SPI.h>
#include <WiFi.h>
#include <esp_camera.h>
#include <esp_mac.h>
#include <math.h>
#include "camera_web_server.h"
#include "config.h"

enum TestStatus : uint8_t { TEST_PASS, TEST_FAIL, TEST_CHECK };

struct TestResult {
  String name;
  TestStatus status;
  String detail;
};

struct AudioLevel {
  uint32_t rms;
  uint32_t peak;
};

#if T_SIMCAM_MIC_TYPE == T_SIMCAM_MIC_I2S
using MicSample = int32_t;
#else
using MicSample = int16_t;
#endif

AudioLevel calculateLevel(const MicSample *data, size_t count, size_t stride,
                          uint8_t rightShift);
void printAudioMeter(const AudioLevel &level);

constexpr size_t MAX_RESULTS = 16;
constexpr size_t BATTERY_ADC_SAMPLES = 16;
constexpr uint32_t BATTERY_MAX_MILLIVOLTS = 4200;
constexpr uint32_t MODEM_START_TIMEOUT_MS = 40000;
constexpr uint32_t SIM_READY_TIMEOUT_MS = 40000;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 30000;
constexpr char CAMERA_AP_PREFIX[] = "T-SimCam";
TestResult results[MAX_RESULTS];
size_t resultCount = 0;
String cameraViewerUrl;
String cameraViewerNetwork;
bool cameraViewerApMode = false;
HardwareSerial ModemSerial(1);
// HardwareSerial AuxSerial(2);
SPIClass sdSpi(FSPI);

void section(const char *name) {
  Serial.printf("\n-------------------- %-20s --------------------\n", name);
}

const char *statusText(TestStatus status) {
  switch (status) {
    case TEST_PASS: return "PASS";
    case TEST_FAIL: return "FAIL";
    default: return "CHECK";
  }
}

void addResult(const char *name, TestStatus status, const String &detail) {
  if (resultCount < MAX_RESULTS) {
    results[resultCount++] = {name, status, detail};
  }
  Serial.printf("[%-5s] %-18s %s\n", statusText(status), name, detail.c_str());
}

void testBoardPower() {
  section("BOARD POWER");
  pinMode(BOARD_POWER_ON_PIN, OUTPUT);
  digitalWrite(BOARD_POWER_ON_PIN, HIGH);
  delay(100);
  addResult("Board power", digitalRead(BOARD_POWER_ON_PIN) == HIGH ? TEST_PASS : TEST_FAIL,
            "enable GPIO is HIGH");
}

void testBatteryVoltage() {
  section("BATTERY ADC");
  pinMode(BATTERY_ADC_PIN, INPUT);
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
  delay(20);

  uint32_t adcMillivoltSum = 0;
  for (size_t i = 0; i < BATTERY_ADC_SAMPLES; ++i) {
    adcMillivoltSum += analogReadMilliVolts(BATTERY_ADC_PIN);
    delay(5);
  }

  const uint32_t adcMillivolts = adcMillivoltSum / BATTERY_ADC_SAMPLES;
  const uint32_t calculatedMillivolts = adcMillivolts * 2;
  const bool limited = calculatedMillivolts > BATTERY_MAX_MILLIVOLTS;
  const uint32_t batteryMillivolts =
      min(calculatedMillivolts, BATTERY_MAX_MILLIVOLTS);
  String detail = String(batteryMillivolts) + " mV";
  if (limited) detail += " (USB limit applied)";
  addResult("Battery ADC", batteryMillivolts > 0 ? TEST_PASS : TEST_FAIL,
            detail);
}

void testMemory() {
  section("MEMORY");
  if (!psramFound()) {
    addResult("PSRAM", TEST_FAIL, "not detected");
    return;
  }
  addResult("PSRAM", TEST_PASS,
            String(static_cast<unsigned long>(ESP.getPsramSize() / (1024 * 1024))) + " MB detected");
}

void testButton() {
  section("USER BUTTON");
  pinMode(USER_BUTTON_PIN, INPUT_PULLUP);
  Serial.println("[ACTION] Press and release BOOT within 8 seconds.");
  bool pressed = false;
  bool released = false;
  const uint32_t started = millis();
  while (millis() - started < 8000) {
    const bool down = digitalRead(USER_BUTTON_PIN) == LOW;
    pressed = pressed || down;
    released = released || (pressed && !down);
    if (pressed && released) break;
    delay(10);
  }
  addResult("User button", pressed && released ? TEST_PASS : TEST_FAIL,
            pressed && released ? "press/release detected" : "no complete press detected");
}

void testIrCut() {
  section("IR-CUT");
  pinMode(IR_CUT_PIN, OUTPUT);
  for (int i = 0; i < 4; ++i) {
    const bool level = i % 2 == 0;
    digitalWrite(IR_CUT_PIN, level);
    Serial.printf("[IR CUT] %s\n", level ? "ON" : "OFF");
    delay(500);
  }
  digitalWrite(IR_CUT_PIN, LOW);
  addResult("IR-cut", TEST_CHECK, "output toggled; confirm operation visually/audibly");
}

void testSdCard() {
  section("MICRO SD");
  sdSpi.begin(SD_SCLK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  if (!SD.begin(SD_CS_PIN, sdSpi)) {
    addResult("microSD", TEST_FAIL, "mount failed");
    return;
  }
  if (SD.cardType() == CARD_NONE) {
    addResult("microSD", TEST_FAIL, "card not detected");
    SD.end();
    return;
  }

  const char *path = "/factory_test.txt";
  SD.remove(path);
  File file = SD.open(path, FILE_WRITE);
  const bool writeOk = file && file.print("T-SimCam factory test") > 0;
  if (file) file.close();
  file = SD.open(path, FILE_READ);
  const String content = file ? file.readString() : String();
  if (file) file.close();
  SD.remove(path);
  const uint64_t sizeMb = SD.cardSize() / (1024ULL * 1024ULL);
  SD.end();
  addResult("microSD", writeOk && content == "T-SimCam factory test" ? TEST_PASS : TEST_FAIL,
            writeOk ? String(sizeMb) + " MB, read/write verified" : "file write failed");
}

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
  config.frame_size = FRAMESIZE_SVGA;
  config.jpeg_quality = 12;
  config.fb_count = psramFound() ? 2 : 1;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
  return config;
}

bool startCameraAccessPoint() {
  uint8_t apMac[6] = {};
  char apSsid[33];
  if (esp_read_mac(apMac, ESP_MAC_WIFI_SOFTAP) == ESP_OK) {
    snprintf(apSsid, sizeof(apSsid), "%s-%02X%02X", CAMERA_AP_PREFIX,
             apMac[4], apMac[5]);
  } else {
    snprintf(apSsid, sizeof(apSsid), "%s-Camera", CAMERA_AP_PREFIX);
  }

  WiFi.disconnect(true);
  delay(100);
  if (!WiFi.mode(WIFI_AP) || !WiFi.softAP(apSsid)) return false;

  cameraViewerApMode = true;
  cameraViewerNetwork = apSsid;
  cameraViewerUrl = "http://" + WiFi.softAPIP().toString();
  Serial.printf("[INFO ] Camera hotspot: %s (open network)\n", apSsid);
  Serial.printf("[INFO ] Camera hotspot IP: %s\n", WiFi.softAPIP().toString().c_str());
  return true;
}

void testCamera() {
  section("CAMERA");
  camera_config_t config = makeCameraConfig();
  const esp_err_t error = esp_camera_init(&config);
  if (error != ESP_OK) {
    addResult("Camera", TEST_FAIL, "init error 0x" + String(error, HEX));
    addResult("Camera viewer", TEST_FAIL, "camera is not available");
    return;
  }
  camera_fb_t *frame = esp_camera_fb_get();
  if (!frame) {
    addResult("Camera", TEST_FAIL, "frame capture failed");
    addResult("Camera viewer", TEST_FAIL, "camera is not available");
    esp_camera_deinit();
    return;
  }

  sensor_t *sensor = esp_camera_sensor_get();
  char detail[96];
  snprintf(detail, sizeof(detail), "PID=0x%04x, %ux%u, %u-byte JPEG",
           sensor ? sensor->id.PID : 0, frame->width, frame->height, frame->len);
  addResult("Camera", TEST_PASS, detail);
  esp_camera_fb_return(frame);

  cameraViewerUrl = "";
  cameraViewerNetwork = "";
  cameraViewerApMode = false;
  bool stationConnected = false;
  if (FACTORY_WIFI_SSID[0] != '\0') {
    Serial.printf("[INFO ] Connecting camera viewer to Wi-Fi: %s\n",
                  FACTORY_WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(FACTORY_WIFI_SSID, FACTORY_WIFI_PASSWORD);
    const uint32_t wifiStarted = millis();
    while (WiFi.status() != WL_CONNECTED &&
           millis() - wifiStarted < WIFI_CONNECT_TIMEOUT_MS) {
      Serial.print('.');
      delay(500);
    }
    Serial.println();
    stationConnected = WiFi.status() == WL_CONNECTED;
  }

  if (stationConnected) {
    cameraViewerNetwork = FACTORY_WIFI_SSID;
    cameraViewerUrl = "http://" + WiFi.localIP().toString();
  } else {
    Serial.println(FACTORY_WIFI_SSID[0] == '\0'
                       ? "[INFO ] Camera Wi-Fi is not configured; starting hotspot."
                       : "[INFO ] Wi-Fi connection timed out after 30 seconds; starting hotspot.");
    if (!startCameraAccessPoint()) {
      addResult("Camera viewer", TEST_FAIL, "camera hotspot failed to start");
      esp_camera_deinit();
      return;
    }
  }

  if (!startCameraWebServer()) {
    addResult("Camera viewer", TEST_FAIL, "HTTP server failed to start");
    cameraViewerUrl = "";
    esp_camera_deinit();
    return;
  }
  addResult("Camera viewer", TEST_PASS,
            String(cameraViewerApMode ? "hotspot " : "Wi-Fi ") + cameraViewerUrl);
  Serial.printf("[ACTION] Connect to %s and open %s in a browser.\n",
                cameraViewerNetwork.c_str(), cameraViewerUrl.c_str());
}

AudioLevel calculateLevel(const MicSample *data, size_t count, size_t stride, uint8_t rightShift) {
  if (count == 0) return {0, 0};
  int64_t sum = 0;
  for (size_t i = 0; i < count; ++i) sum += data[i * stride] >> rightShift;
  const int32_t mean = sum / static_cast<int32_t>(count);
  uint64_t squareSum = 0;
  uint32_t peak = 0;
  for (size_t i = 0; i < count; ++i) {
    const int32_t centered = (data[i * stride] >> rightShift) - mean;
    const uint32_t magnitude = centered < 0 ? -centered : centered;
    peak = max(peak, magnitude);
    squareSum += static_cast<int64_t>(centered) * centered;
  }
  return {static_cast<uint32_t>(sqrtf(static_cast<float>(squareSum) / count)), peak};
}

void printAudioMeter(const AudioLevel &level) {
  const size_t bars = min<size_t>(40, level.rms / 80);
  Serial.printf("RMS %6lu |", static_cast<unsigned long>(level.rms));
  for (size_t i = 0; i < 40; ++i) Serial.print(i < bars ? '#' : ' ');
  Serial.printf("| peak %6lu\n", static_cast<unsigned long>(level.peak));
}

void testMicrophone() {
  section("MICROPHONE");
  I2SClass microphone(I2S_NUM_0);
  microphone.setTimeout(1000);
  AudioLevel best = {0, 0};

#if T_SIMCAM_MIC_TYPE == T_SIMCAM_MIC_I2S
  MicSample samples[480 * 2];
  microphone.setPins(MIC_I2S_BCLK_PIN, MIC_I2S_WS_PIN, -1, MIC_DATA_PIN);
  const bool ready = microphone.begin(I2S_MODE_STD, 16000, I2S_DATA_BIT_WIDTH_32BIT,
                                      I2S_SLOT_MODE_STEREO, I2S_STD_SLOT_BOTH);
  Serial.printf("[INFO ] I2S DATA=%d BCLK=%d WS=%d\n",
                MIC_DATA_PIN, MIC_I2S_BCLK_PIN, MIC_I2S_WS_PIN);
#else
  MicSample samples[480];
  microphone.setPinsPdmRx(MIC_PDM_CLK_PIN, MIC_DATA_PIN);
  const bool ready = microphone.begin(I2S_MODE_PDM_RX, 16000, I2S_DATA_BIT_WIDTH_16BIT,
                                      I2S_SLOT_MODE_MONO, I2S_STD_SLOT_RIGHT);
  Serial.printf("[INFO ] PDM DATA=%d CLK=%d\n", MIC_DATA_PIN, MIC_PDM_CLK_PIN);
#endif
  if (!ready) {
    addResult("Microphone", TEST_FAIL,
              "initialization error " + String(microphone.lastError()));
    return;
  }

  pinMode(USER_BUTTON_PIN, INPUT_PULLUP);
  while (digitalRead(USER_BUTTON_PIN) == LOW) delay(10);
  Serial.println("[ACTION] Speak or tap near the microphone.");
  Serial.println("[ACTION] Press and release BOOT to finish the microphone test.");
  while (true) {
    if (digitalRead(USER_BUTTON_PIN) == LOW) {
      delay(20);
      if (digitalRead(USER_BUTTON_PIN) == LOW) {
        while (digitalRead(USER_BUTTON_PIN) == LOW) delay(10);
        Serial.println("[INFO ] BOOT released, microphone test finished.");
        break;
      }
    }

    const size_t bytesRead = microphone.readBytes(
        reinterpret_cast<char *>(samples), sizeof(samples));
    if (bytesRead == 0) continue;
#if T_SIMCAM_MIC_TYPE == T_SIMCAM_MIC_I2S
    const size_t frames = bytesRead / (2 * sizeof(samples[0]));
    const AudioLevel left = calculateLevel(samples, frames, 2, 14);
    const AudioLevel right = calculateLevel(samples + 1, frames, 2, 14);
    const AudioLevel level = right.rms > left.rms ? right : left;
#else
    const AudioLevel level = calculateLevel(samples, bytesRead / sizeof(samples[0]), 1, 0);
#endif
    if (level.rms > best.rms) best = level;
    printAudioMeter(level);
  }
  microphone.end();
  addResult("Microphone", best.peak > 0 ? TEST_PASS : TEST_FAIL,
            "max RMS=" + String(best.rms) + ", peak=" + String(best.peak));
}

void testWireless() {
  section("WIRELESS");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  const int wifiCount = WiFi.scanNetworks();
  addResult("Wi-Fi", wifiCount >= 0 ? TEST_PASS : TEST_FAIL,
            wifiCount >= 0 ? String(wifiCount) + " network(s) found" : "scan failed");
  WiFi.scanDelete();
  WiFi.mode(WIFI_OFF);

  if (!BLEDevice::init("")) {
    addResult("Bluetooth LE", TEST_FAIL, "initialization failed");
    return;
  }
  BLEScan *scan = BLEDevice::getScan();
  scan->setActiveScan(true);
  BLEScanResults *bleResults = scan->start(3, false);
  addResult("Bluetooth LE", bleResults ? TEST_PASS : TEST_FAIL,
            bleResults ? String(bleResults->getCount()) + " device(s) found" : "scan failed");
  if (bleResults) scan->clearResults();
  BLEDevice::deinit(true);
}

void printModemResponse(const char *command, const String &response) {
  int start = 0;
  while (start < response.length()) {
    int end = response.indexOf('\n', start);
    if (end < 0) end = response.length();

    String line = response.substring(start, end);
    line.trim();
    if (line.length() > 0 && line != command) Serial.println(line);
    start = end + 1;
  }
}

String sendAt(const char *command, uint32_t timeoutMs) {
  while (ModemSerial.available()) ModemSerial.read();
  ModemSerial.println(command);
  String response;
  const uint32_t started = millis();
  while (millis() - started < timeoutMs) {
    while (ModemSerial.available()) response += static_cast<char>(ModemSerial.read());
    if (response.indexOf("\r\nOK\r\n") >= 0 || response.indexOf("ERROR") >= 0) break;
    delay(10);
  }
  printModemResponse(command, response);
  return response;
}

void testModem() {
  section("MODEM + SIM");
  pinMode(MODEM_STATUS_PIN, INPUT);
  pinMode(MODEM_POWER_KEY_PIN, OUTPUT);
  ModemSerial.begin(115200, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
  digitalWrite(MODEM_POWER_KEY_PIN, HIGH);
  delay(500);
  digitalWrite(MODEM_POWER_KEY_PIN, LOW);
  Serial.println("[INFO ] Waiting up to 40 seconds for modem AT response...");

  bool responds = false;
  const uint32_t started = millis();
  while (millis() - started < MODEM_START_TIMEOUT_MS) {
    const String response = sendAt("AT", 800);
    if (response.indexOf("OK") >= 0) {
      responds = true;
      break;
    }
    delay(500);
  }
  if (!responds) {
    addResult("Modem AT", TEST_FAIL, "no response");
    addResult("SIM card", TEST_FAIL, "not tested because modem is offline");
    ModemSerial.end();
    return;
  }
  addResult("Modem AT", TEST_PASS,
            "responding, status GPIO=" + String(digitalRead(MODEM_STATUS_PIN)));
  sendAt("ATI", 2000);

  Serial.println("[INFO ] Waiting up to 40 seconds for SIM card readiness...");
  bool simReady = false;
  String simStatus;
  const uint32_t simStarted = millis();
  while (millis() - simStarted < SIM_READY_TIMEOUT_MS) {
    const uint32_t elapsed = millis() - simStarted;
    const uint32_t remaining = SIM_READY_TIMEOUT_MS - elapsed;
    simStatus = sendAt("AT+CPIN?", min<uint32_t>(2000, remaining));
    if (simStatus.indexOf("READY") >= 0) {
      simReady = true;
      break;
    }
    delay(1000);
  }
  if (!simReady) {
    addResult("SIM card", TEST_FAIL, "CPIN is not READY after 40 seconds");
    ModemSerial.end();
    return;
  }

  String iccid = sendAt("AT+CCID", 3000);
  if (iccid.indexOf("OK") < 0) iccid = sendAt("AT+ICCID", 3000);
  size_t digitCount = 0;
  for (size_t i = 0; i < iccid.length(); ++i) {
    if (isDigit(static_cast<unsigned char>(iccid[i]))) ++digitCount;
  }
  const bool iccidOk = digitCount >= 10 && iccid.indexOf("OK") >= 0;
  addResult("SIM card", iccidOk ? TEST_PASS : TEST_FAIL,
            iccidOk ? "READY and ICCID read" : "ICCID read failed");
  ModemSerial.end();
}

void printSummary() {
  section("FINAL SUMMARY");
  size_t passed = 0;
  size_t failed = 0;
  size_t checks = 0;
  Serial.println("+--------------------+---------+------------------------------------------+");
  Serial.println("| TEST               | RESULT  | DETAIL                                   |");
  Serial.println("+--------------------+---------+------------------------------------------+");
  for (size_t i = 0; i < resultCount; ++i) {
    passed += results[i].status == TEST_PASS;
    failed += results[i].status == TEST_FAIL;
    checks += results[i].status == TEST_CHECK;
    Serial.printf("| %-18.18s | %-7s | %-40.40s |\n", results[i].name.c_str(),
                  statusText(results[i].status), results[i].detail.c_str());
  }
  Serial.println("+--------------------+---------+------------------------------------------+");
  Serial.printf("PASS: %u   FAIL: %u   CHECK: %u\n", passed, failed, checks);
  Serial.printf("FACTORY RESULT: %s\n", failed == 0 ? "PASS" : "FAIL");
  if (cameraViewerUrl.length() > 0) {
    Serial.println("\n===============================================================");
    Serial.println("                 CAMERA VISUAL INSPECTION");
    Serial.println("===============================================================");
    Serial.printf("Connect the test device to %s: %s\n",
                  cameraViewerApMode ? "open hotspot" : "Wi-Fi",
                  cameraViewerNetwork.c_str());
    Serial.printf("Open in browser: %s\n", cameraViewerUrl.c_str());
    Serial.println("Confirm that the live image is clear and updates normally.");
  }
  Serial.println("Press BOOT to restart all tests.");
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n===============================================================");
  Serial.println("                    T-SimCam FACTORY TEST");
  Serial.println("===============================================================");
  Serial.printf("Board: %s\n", T_SIMCAM_BOARD_NAME);
  Serial.printf("Microphone: %s\n", T_SIMCAM_MIC_TYPE == T_SIMCAM_MIC_I2S ? "I2S" : "PDM");

  testBoardPower();
  testBatteryVoltage();
  testModem();
  testMemory();
  testSdCard();
  testButton();
  testIrCut();
  testMicrophone();
  testWireless();
  testCamera();
  printSummary();
}

void loop() {
  if (digitalRead(USER_BUTTON_PIN) == LOW) {
    delay(30);
    if (digitalRead(USER_BUTTON_PIN) == LOW) ESP.restart();
  }
  delay(20);
}
