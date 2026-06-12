#define PIN_ROT_CLK 18
#define PIN_ROT_DT 19
#define PIN_ROT_SW 5
#define BTN PIN_ROT_SW

volatile int rotDelta = 0;
volatile uint8_t rotStatePrev = 0;

static const int8_t QEM[4][4] = {
  { 0, -1, +1, 0 },
  { +1, 0, 0, -1 },
  { -1, 0, 0, +1 },
  { 0, +1, -1, 0 }
};

void IRAM_ATTR rotQuad_ISR() {
  uint8_t stateCurr = (digitalRead(PIN_ROT_CLK) << 1) | digitalRead(PIN_ROT_DT);
  rotDelta += QEM[rotStatePrev][stateCurr];
  rotStatePrev = stateCurr;
}

void initEncoder() {
  pinMode(PIN_ROT_CLK, INPUT_PULLUP);
  pinMode(PIN_ROT_DT, INPUT_PULLUP);
  pinMode(PIN_ROT_SW, INPUT_PULLUP);
  rotStatePrev = (digitalRead(PIN_ROT_CLK) << 1) | digitalRead(PIN_ROT_DT);
  attachInterrupt(digitalPinToInterrupt(PIN_ROT_CLK), rotQuad_ISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ROT_DT), rotQuad_ISR, CHANGE);
  Serial.println("Rotary Encoder Ready");
}

int readEncoderBTN(bool enable = true) {
  if (!enable) {
    rotDelta = 0;
    return 0;
  }
  if (abs(rotDelta) < 4) return 0;
  int dir = (rotDelta > 0) ? 1 : -1;
  rotDelta -= dir * 4;
  return dir;
}

byte readButton() {
  static uint32_t t = 0, db = 0;
  static bool lp = false, last = HIGH;

  bool b = digitalRead(BTN);

  if (b != last) {
    if (millis() - db < 30) return 0;
    db = millis();
    last = b;
  }

  if (!b && !t) t = millis();

  if (!b && !lp && millis() - t >= 300) {
    lp = true;
    return 2;
  }

  if (b && t) {
    bool wasLong = lp;
    t = 0;
    lp = false;
    if (!wasLong) return 1;
  }

  return 0;
}