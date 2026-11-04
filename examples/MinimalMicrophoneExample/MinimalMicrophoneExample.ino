#include <Arduino.h>
#include <ESP_I2S.h>
#include <math.h>
#include "config.h"

constexpr uint32_t SAMPLE_RATE = 16000;
constexpr size_t FRAME_SAMPLES = 480;
I2SClass microphone(I2S_NUM_0);

#if T_SIMCAM_MIC_TYPE == T_SIMCAM_MIC_I2S
using SampleType = int32_t;
SampleType samples[FRAME_SAMPLES * 2];
#else
using SampleType = int16_t;
SampleType samples[FRAME_SAMPLES];
#endif

struct AudioLevel {
  uint32_t rms;
  uint32_t peak;
};

AudioLevel calculateLevel(const SampleType *data, size_t count,
                          size_t stride, uint8_t rightShift);
void printMeter(const AudioLevel &level);

AudioLevel calculateLevel(const SampleType *data, size_t count,
                          size_t stride, uint8_t rightShift) {
  if (count == 0) {
    return {0, 0};
  }
  int64_t sum = 0;
  for (size_t i = 0; i < count; ++i) {
    sum += data[i * stride] >> rightShift;
  }
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

void printMeter(const AudioLevel &level) {
  const size_t bars = min<size_t>(50, level.rms / 80);
  Serial.printf("RMS %6lu |", static_cast<unsigned long>(level.rms));
  for (size_t i = 0; i < 50; ++i) {
    Serial.print(i < bars ? '#' : ' ');
  }
  Serial.printf("| peak %6lu\n", static_cast<unsigned long>(level.peak));
}

void setup() {
  pinMode(BOARD_POWER_ON_PIN, OUTPUT);
  digitalWrite(BOARD_POWER_ON_PIN, HIGH);
  Serial.begin(115200);
  delay(1500);
  microphone.setTimeout(1000);

#if T_SIMCAM_MIC_TYPE == T_SIMCAM_MIC_I2S
  microphone.setPins(MIC_I2S_BCLK_PIN, MIC_I2S_WS_PIN, -1, MIC_DATA_PIN);
  const bool ready = microphone.begin(I2S_MODE_STD, SAMPLE_RATE,
                                      I2S_DATA_BIT_WIDTH_32BIT,
                                      I2S_SLOT_MODE_STEREO,
                                      I2S_STD_SLOT_BOTH);
  Serial.printf("\n%s I2S microphone: DATA=%d BCLK=%d WS=%d\n",
                T_SIMCAM_BOARD_NAME, MIC_DATA_PIN, MIC_I2S_BCLK_PIN, MIC_I2S_WS_PIN);
#else
  microphone.setPinsPdmRx(MIC_PDM_CLK_PIN, MIC_DATA_PIN);
  const bool ready = microphone.begin(I2S_MODE_PDM_RX, SAMPLE_RATE,
                                      I2S_DATA_BIT_WIDTH_16BIT,
                                      I2S_SLOT_MODE_MONO,
                                      I2S_STD_SLOT_RIGHT);
  Serial.printf("\n%s PDM microphone: DATA=%d CLK=%d\n",
                T_SIMCAM_BOARD_NAME, MIC_DATA_PIN, MIC_PDM_CLK_PIN);
#endif
  if (!ready) {
    Serial.printf("[FAIL] microphone initialization error: %d\n", microphone.lastError());
    while (true) delay(1000);
  }
  Serial.println("[PASS] microphone initialized; live level follows");
}

void loop() {
  const size_t bytesRead = microphone.readBytes(
      reinterpret_cast<char *>(samples), sizeof(samples));
  if (bytesRead == 0) {
    Serial.printf("[FAIL] microphone read error: %d\n", microphone.lastError());
    delay(100);
    return;
  }

#if T_SIMCAM_MIC_TYPE == T_SIMCAM_MIC_I2S
  const size_t frames = bytesRead / (2 * sizeof(samples[0]));
  const AudioLevel left = calculateLevel(samples, frames, 2, 14);
  const AudioLevel right = calculateLevel(samples + 1, frames, 2, 14);
  const AudioLevel level = right.rms > left.rms ? right : left;
#else
  const AudioLevel level = calculateLevel(samples, bytesRead / sizeof(samples[0]), 1, 0);
#endif
  printMeter(level);
}
