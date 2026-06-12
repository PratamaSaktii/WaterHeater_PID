#include <esp_adc_cal.h>
#include <driver/adc.h>

#define PIN_SENSOR ADC1_CHANNEL_6  // GPIO33
#define SAMPLE_COUNT 128
#define OFFSET -0.0  // sesuaikan dari hasil kalibrasi

esp_adc_cal_characteristics_t adcChar;

void initLM35() {
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(PIN_SENSOR, ADC_ATTEN_DB_0);
  esp_adc_cal_characterize(
    ADC_UNIT_1,
    ADC_ATTEN_DB_0,
    ADC_WIDTH_BIT_12,
    1100,
    &adcChar);
}

float readSuhuRaw() {
  uint32_t total = 0;
  for (int i = 0; i < SAMPLE_COUNT; i++) {
    total += adc1_get_raw(PIN_SENSOR);
  }
  uint32_t raw = total / SAMPLE_COUNT;
  uint32_t mV = esp_adc_cal_raw_to_voltage(raw, &adcChar);
  return (mV / 10.0) + OFFSET;
}

float readSuhu() {
  static float suhu = readSuhuRaw();

  float a = readSuhuRaw();
  float b = readSuhuRaw();
  float c = readSuhuRaw();

  // Median of 3
  float median;
  if (a > b) {
    if (b > c) median = b;
    else if (a > c) median = c;
    else median = a;
  } else {
    if (a > c) median = a;
    else if (b > c) median = c;
    else median = b;
  }

  // EMA
  suhu += (median - suhu) * 0.05;
  // Serial.println(suhu);

  return suhu;
}
