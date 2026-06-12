#include "BuzzLed.h"

#define ZC_PIN 33
#define DIM_PIN 32

volatile bool zcFlag = false;
volatile uint32_t zcTime = 0;
uint8_t power = 0;

void IRAM_ATTR zeroCross() {
  zcTime = micros();
  zcFlag = true;
}

void initDimmer() {
  pinMode(DIM_PIN, OUTPUT);
  digitalWrite(DIM_PIN, LOW);
  attachInterrupt(digitalPinToInterrupt(ZC_PIN), zeroCross, RISING);
  Serial.println("Dimmer Ready");
}

void dimmer(uint8_t pwr) {
  pwr = constrain(pwr, 0, 100);

  if (pwr == 0) {
    digitalWrite(DIM_PIN, LOW);
    zcFlag = false;
    return;
  }

  // power 100 → bypass ZC, langsung HIGH terus
  if (pwr == 100) {
    digitalWrite(DIM_PIN, HIGH);
    zcFlag = false;
    return;
  }

  if (zcFlag && micros() - zcTime > 9000UL) {
    zcFlag = false;
    return;
  }

  uint32_t delayTime = (100 - pwr) * 100UL;
  delayTime = constrain(delayTime, 300UL, 9500UL);

  if (zcFlag && micros() - zcTime >= delayTime) {
    zcFlag = false;
    digitalWrite(DIM_PIN, HIGH);
    digitalWrite(LED_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(DIM_PIN, LOW);
    digitalWrite(LED_PIN, LOW);
  }
}
