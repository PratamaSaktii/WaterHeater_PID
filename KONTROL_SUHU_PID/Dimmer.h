#define ZC_PIN  33
#define DIM_PIN 32

volatile uint8_t  _power    = 0;
volatile bool     zcFlag    = false;
volatile uint32_t zcTime    = 0;

hw_timer_t* dimTimer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

// Dipanggil timer → saatnya fire triac
void IRAM_ATTR onDimTimer() {
  portENTER_CRITICAL_ISR(&timerMux);
  if (_power > 0 && _power < 100) {
    digitalWrite(DIM_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(DIM_PIN, LOW);
  }
  portEXIT_CRITICAL_ISR(&timerMux);
}

// Dipanggil setiap zero cross
void IRAM_ATTR zeroCross() {
  zcTime = micros();
  zcFlag = true;

  if (_power == 0) {
    digitalWrite(DIM_PIN, LOW);
    return;
  }

  if (_power == 100) {
    digitalWrite(DIM_PIN, HIGH);
    return;
  }

  // Set timer delay sesuai power
  uint32_t delayTime = (uint32_t)(100 - _power) * 97UL;
  delayTime = constrain(delayTime, 400UL, 9600UL);

  // Reset dan start timer
  timerRestart(dimTimer);
  timerAlarmWrite(dimTimer, delayTime, false);  // false = one-shot
  timerAlarmEnable(dimTimer);
}

void initDimmer() {
  pinMode(DIM_PIN, OUTPUT);
  digitalWrite(DIM_PIN, LOW);

  // Timer 1MHz (1 tick = 1µs), one-shot
  dimTimer = timerBegin(0, 80, true);
  timerAttachInterrupt(dimTimer, &onDimTimer, true);
  timerAlarmWrite(dimTimer, 5000, false);
  // Jangan enable dulu, nanti di zeroCross

  attachInterrupt(digitalPinToInterrupt(ZC_PIN), zeroCross, RISING);
  Serial.println("Dimmer Ready");
}

// Dipanggil dari loop atau PID
void dimmer(uint8_t pwr) {
  _power = constrain(pwr, 0, 100);

  // Jika 0 atau 100, langsung set pin
  if (_power == 0)   digitalWrite(DIM_PIN, LOW);
  if (_power == 100) digitalWrite(DIM_PIN, HIGH);
}