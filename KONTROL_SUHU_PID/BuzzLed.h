#define BUZZER_PIN 15
#define LED_PIN 2

void initBuzzLed() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
}

void blink2x() {
  digitalWrite(LED_PIN, 1);
  delay(50);
  digitalWrite(LED_PIN, 0);
  delay(50);
  digitalWrite(LED_PIN, 1);
  delay(50);
  digitalWrite(LED_PIN, 0);
}

void blink99x() {
  static uint32_t t = 0;
  static byte cnt = 0;

  if (millis() - t >= (cnt < 10 ? 20 : 300)) {

    t = millis();

    if (cnt < 10) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      cnt++;
    } else {
      digitalWrite(LED_PIN, LOW);
      cnt = 0;
    }
  }
}

void blinkLed() {
  static uint32_t t;
  static bool state;

  if (millis() - t >= (state ? 10 : 1000)) {
    t = millis();
    state = !state;
    digitalWrite(LED_PIN, state);
  }
}

void BuzzGreeting() {
  int f[] = { 2794, 3136, 3520 };
  int d[] = { 100, 100, 230 };
  int g[] = {
    5,
    3,
    50,
  };
  blink2x();
  for (int i = 0; i < 3; i++) {
    tone(BUZZER_PIN, f[i]);
    delay(d[i]);
    noTone(BUZZER_PIN);
    delay(g[i]);
  }
  noTone(BUZZER_PIN);
  blink2x();
}