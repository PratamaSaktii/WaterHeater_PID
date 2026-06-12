#include <EEPROM.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH1106.h>
#include <ArduinoJson.h>

#include "LM35.h"
#include "Encoder.h"
// #include "BuzzLed.h"
#include "Dimmer.h"

Adafruit_SH1106 lcd(-1, -1);

char buff[99];
int mainmenu = 0;
const char *name[] = {
  "Kp",
  "Ki",
  "Kd"
};

struct DataSetting {
  bool KondisiAlat = 0;
  float SetPoint = 40.0;
  int Ts = 500;
  float Kp = 1.0;
  float Ki = 1.0;
  float Kd = 1.0;
} setting;

typedef struct __attribute__((packed)) {
  uint32_t tick_ms;
  float setpoint;
  float temp;
  float kp;
  float ki;
  float kd;
  int16_t dimming;
} TelemetryData_t;

typedef struct __attribute__((packed)) {
  float kp;
  float ki;
  float kd;
  float setpoint;
} CommandData_t;

float error, prevousError = 0;
float P, I, D;
float output;

void defaultSetting() {
  setting.KondisiAlat = 0;
  setting.SetPoint = 40.0;
  setting.Ts = 1000;

  setting.Kp = 1.0;
  setting.Ki = 1.0;
  setting.Kd = 1.0;
}

void readEEPROM() {
  EEPROM.get(0, setting);
}

void writeEEPROM() {
  EEPROM.put(0, setting);
  EEPROM.commit();
}

template<typename T>
inline void print(int x, int y, T txt, uint16_t c = WHITE, uint8_t s = 1) {
  lcd.setCursor(x, y);
  lcd.setTextColor(c);
  lcd.setTextSize(s);
  lcd.print(txt);
}

void printCenter(const char *text, int y, int size = 1, uint16_t color = WHITE) {
  int textWidth = strlen(text) * 6 * size;  // 6 pixel per karakter * size
  int x = (128 - textWidth) / 2;            // center horizontal
  lcd.setTextSize(size);
  lcd.setTextColor(color);
  lcd.setCursor(x, y);
  lcd.print(text);
}

void scrollText(const char *text, int y, int textSize, bool invert) {

  const int xLeft = 6 + 32;
  const int xRight = 122;

  static int offset = 0;
  static unsigned long lastMs = 0;

  int len = strlen(text);
  int charW = 6 * textSize;
  int textW = len * charW;

  int gap = 15 * charW;
  int loopW = textW + gap;

  // === SPEED ===
  if (millis() - lastMs >= 35) {
    lastMs = millis();
    offset += 2;
    if (offset >= loopW) offset = 0;  // wrap NON-VISUAL
  }

  lcd.setTextSize(textSize);
  lcd.setTextColor(invert ? WHITE : BLACK);

  // === DRAW DENGAN ANCHOR TETAP ===
  for (int copy = 0; copy < 2; copy++) {

    int baseX = xLeft + (copy * loopW) - offset;

    for (int i = 0; i < len; i++) {
      int x = baseX + i * charW;

      if (x + charW < xLeft) continue;
      if (x > xRight) continue;

      lcd.setCursor(x, y);
      lcd.print(text[i]);
    }
  }
}

void Mainmenu() {
  blinkLed();
  static bool eSM = 0;
  static byte eSP = 0;
  static byte ePID = 0;
  int depan = 0;
  int belakang = 0;


  int delta = readEncoderBTN();
  if (setting.KondisiAlat == 0 && eSM == 0 && eSP == 0 && ePID == 0) {
    if (delta > 0) {
      mainmenu = (mainmenu + 1) % 7;
    }

    if (delta < 0) {
      mainmenu = (mainmenu == 0) ? 6 : mainmenu - 1;
    }
  }
  //====================
  // ON OFF
  //====================
  bool ON_OFF = mainmenu != 0;

  if (ON_OFF) {
    lcd.drawRoundRect(0, 0, 27, 13, 1, 1);
    print(setting.KondisiAlat ? 8 : 5, 3,
          setting.KondisiAlat ? "ON" : "OFF");
  } else {
    if (setting.KondisiAlat == 0) {
      if (readButton() == 1) setting.KondisiAlat = 1;
    } else {

      if (readButton() == 2) setting.KondisiAlat = 0;
    }


    lcd.fillRoundRect(0, 0, 27, 13, 1, 1);

    print(setting.KondisiAlat ? 8 : 5, 3,
          setting.KondisiAlat ? "ON" : "OFF",
          BLACK);
  }

  //====================
  // JUDUL
  //====================
  bool JD = mainmenu != 1;

  if (JD)
    lcd.drawRoundRect(30, 0, 98, 13, 1, 1);
  else
    lcd.fillRoundRect(30, 0, 98, 13, 1, 1);
  scrollText("Pemanas Air Otomatis Berbasis Kendali PID _ Teknik Elektro K2", 3, 1, JD);


  //====================
  // SETPOINT
  //====================
  bool SPT = mainmenu != 2;

  if (SPT)
    lcd.drawRoundRect(60, 16, 68, 33, 1, 1);
  else {
    byte btn = readButton();
    if (btn == 1) {
      eSP++;
      writeEEPROM();
    }
    eSP = eSP > 2 ? 1 : eSP;
    if (btn == 2) {
      eSP = 0;
      writeEEPROM();
    }
    lcd.fillRoundRect(60, 16, 68, 33, 1, 1);
  }

  print(65, 20, "SET", SPT);
  print(83, 20, ":", SPT);

  // FIX FLOAT → lebih stabil
  depan = (int)setting.SetPoint;
  belakang = abs((int)round(setting.SetPoint * 10) % 10);

  if (eSP == 0) {

    lcd.setCursor(89, 20);
    lcd.setTextColor(SPT);
    lcd.print(setting.SetPoint, 1);

  } else if (eSP == 1) {

    if (delta > 0) {
      setting.SetPoint += 1;
      setting.SetPoint = setting.SetPoint > 99 ? 99 : setting.SetPoint;
      writeEEPROM();
    }
    if (delta < 0) {
      setting.SetPoint -= 1;
      setting.SetPoint = setting.SetPoint < 30 ? 30 : setting.SetPoint;
      writeEEPROM();
    }

    lcd.setCursor(89, 20);
    lcd.setTextColor(WHITE, BLACK);
    lcd.print(depan);
    lcd.setTextColor(BLACK);
    lcd.print(".");
    lcd.print(belakang);

  } else if (eSP == 2) {

    if (delta > 0) {
      setting.SetPoint += 0.1;
      setting.SetPoint = setting.SetPoint > 99 ? 99 : setting.SetPoint;
      writeEEPROM();
    }
    if (delta < 0) {
      setting.SetPoint -= 0.1;
      setting.SetPoint = setting.SetPoint < 30 ? 30 : setting.SetPoint;
      writeEEPROM();
    }

    lcd.setCursor(89, 20);
    lcd.setTextColor(BLACK);
    lcd.print(depan);
    lcd.print(".");
    lcd.setTextColor(WHITE, BLACK);
    lcd.print(belakang);
  }

  lcd.drawCircle(115, 21, 1, SPT);
  print(118, 20, "C", SPT);
  sprintf(buff, "%04.1f", readSuhu());
  print(65, 31, buff, SPT, 2);
  lcd.drawCircle(115, 39, 1, SPT);
  print(118, 38, "C", SPT);


  //====================
  // Ts
  //====================
  bool SM = mainmenu != 3;

  // gambar border
  if (SM)
    lcd.drawRoundRect(60, 51, 68, 13, 1, 1);
  else {
    byte btn = readButton();
    if (btn == 1) {
      eSM = 1;
      writeEEPROM();
    }
    if (btn == 2) {
      eSM = 0;
      writeEEPROM();
    }
    lcd.fillRoundRect(60, 51, 68, 13, 1, 1);
  }

  print(65, 54, "Ts", SM);
  print(78, 54, ":", SM);

  if (!eSM) {

    lcd.setCursor(85, 54);
    lcd.setTextColor(SM);
    sprintf(buff, "%04d", setting.Ts);
    lcd.print(buff);

  } else {

    if (delta > 0) {
      setting.Ts += 20;
      setting.Ts = setting.Ts > 9999 ? 9999 : setting.Ts;
      writeEEPROM();
    }

    if (delta < 0) {
      setting.Ts -= 20;
      setting.Ts = setting.Ts < 20 ? 20 : setting.Ts;
      writeEEPROM();
    }

    lcd.setCursor(85, 54);
    lcd.setTextColor(WHITE, BLACK);
    sprintf(buff, "%04d", setting.Ts);
    lcd.print(buff);
  }

  print(112, 54, "mS", SM);

  //====================
  // PID
  //====================
  bool PD = (mainmenu < 4 || mainmenu > 6);

  if (PD)
    lcd.drawRoundRect(0, 16, 57, 48, 1, 1);
  else {
    byte btn = readButton();

    if (btn == 1) {
      ePID++;
      writeEEPROM();
    }
    ePID = ePID > 2 ? 1 : ePID;

    if (btn == 2) ePID = 0;

    lcd.fillRoundRect(0, 16, 57, 48, 1, 1);
  }

  lcd.drawFastHLine(4, 31, 49, PD);
  lcd.drawFastHLine(4, 47, 49, PD);

  if (mainmenu == 4) {
    lcd.drawRoundRect(2, 50, 53, 11, 2, BLACK);
    lcd.drawRoundRect(1, 49, 55, 13, 2, BLACK);
  }
  if (mainmenu == 5) {
    lcd.drawRoundRect(2, 34, 53, 11, 2, BLACK);
    lcd.drawRoundRect(1, 33, 55, 13, 2, BLACK);
  }
  if (mainmenu == 6) {
    lcd.drawRoundRect(2, 18, 53, 11, 2, BLACK);
    lcd.drawRoundRect(1, 17, 55, 13, 2, BLACK);
  }

  for (byte i = 0; i < 3; i++) {
    print(5, 20 + (16 * i), name[i], PD);
    print(17, 20 + (16 * i), ":", PD);
  }

  // ====================
  // EDIT PID
  // ====================

  float *pid = nullptr;

  if (mainmenu == 4) pid = &setting.Kd;
  if (mainmenu == 5) pid = &setting.Ki;
  if (mainmenu == 6) pid = &setting.Kp;

  if (pid && ePID) {

    if (ePID == 1) {

      if (delta > 0) {
        *pid += 1.0;
        writeEEPROM();
      }
      if (delta < 0) {
        *pid -= 1.0;
        writeEEPROM();
      }


    } else if (ePID == 2) {

      if (delta > 0) {
        *pid += 0.01;
        writeEEPROM();
      }
      if (delta < 0) {
        *pid -= 0.01;
        writeEEPROM();
      }
    }

    *pid = *pid < 0 ? 0 : *pid;
  }

  // ====================
  // TAMPIL Kp
  // ====================

  int kpD = (int)setting.Kp;
  int kpB = abs((int)round(setting.Kp * 100) % 100);

  lcd.setCursor(23, 20);

  if (mainmenu == 6 && ePID == 1) {

    lcd.setTextColor(WHITE, BLACK);
    lcd.print(kpD);

    lcd.setTextColor(BLACK);
    lcd.print(".");
    if (kpB < 10) lcd.print("0");
    lcd.print(kpB);

  } else if (mainmenu == 6 && ePID == 2) {

    lcd.setTextColor(BLACK);
    lcd.print(kpD);
    lcd.print(".");

    lcd.setTextColor(WHITE, BLACK);
    if (kpB < 10) lcd.print("0");
    lcd.print(kpB);

  } else {

    lcd.setTextColor(PD);
    sprintf(buff, "%05.2f", setting.Kp);
    lcd.print(buff);
  }

  // ====================
  // TAMPIL Ki
  // ====================

  int kiD = (int)setting.Ki;
  int kiB = abs((int)round(setting.Ki * 100) % 100);

  lcd.setCursor(23, 36);

  if (mainmenu == 5 && ePID == 1) {

    lcd.setTextColor(WHITE, BLACK);
    lcd.print(kiD);

    lcd.setTextColor(BLACK);
    lcd.print(".");
    if (kiB < 10) lcd.print("0");
    lcd.print(kiB);

  } else if (mainmenu == 5 && ePID == 2) {

    lcd.setTextColor(BLACK);
    lcd.print(kiD);
    lcd.print(".");

    lcd.setTextColor(WHITE, BLACK);
    if (kiB < 10) lcd.print("0");
    lcd.print(kiB);

  } else {

    lcd.setTextColor(PD);
    sprintf(buff, "%05.2f", setting.Ki);
    lcd.print(buff);
  }

  // ====================
  // TAMPIL Kd
  // ====================

  int kdD = (int)setting.Kd;
  int kdB = abs((int)round(setting.Kd * 100) % 100);

  lcd.setCursor(23, 52);

  if (mainmenu == 4 && ePID == 1) {

    lcd.setTextColor(WHITE, BLACK);
    lcd.print(kdD);

    lcd.setTextColor(BLACK);
    lcd.print(".");
    if (kdB < 10) lcd.print("0");
    lcd.print(kdB);

  } else if (mainmenu == 4 && ePID == 2) {

    lcd.setTextColor(BLACK);
    lcd.print(kdD);
    lcd.print(".");

    lcd.setTextColor(WHITE, BLACK);
    if (kdB < 10) lcd.print("0");
    lcd.print(kdB);

  } else {

    lcd.setTextColor(PD);
    sprintf(buff, "%05.2f", setting.Kd);
    lcd.print(buff);
  }
}

void suhuPID(float target) {
  if (target > 0) {

    static uint32_t lastTime = 0;
    static float lastTarget = -999;
    static byte modeJauh = 0;

    uint32_t now = millis();
    uint32_t deltaTime = now - lastTime;

    if (deltaTime >= setting.Ts) {

      float suhu = readSuhu();
      float dT = deltaTime / 1000.0;

      // Jika target berubah, tentukan ulang mode
      if (target != lastTarget) {

        float selisih = target - suhu;

        if (selisih > 20)
          modeJauh = 0;  // Mode 1
        else if (selisih > 10)
          modeJauh = 1;  // Mode 2
        else
          modeJauh = 2;  // Mode 3

        lastTarget = target;
      }

      // Hitung error berdasarkan mode
      if (modeJauh == 0) {

        // MODE 1
        if (target > 90) error = (target - 3.0) - suhu;
        else if (target > 80) error = (target - 4.0) - suhu;
        else if (target > 60) error = (target - 4.5) - suhu;
        else if (target > 40) error = (target - 5.0) - suhu;

      } else if (modeJauh == 1) {

        // MODE 2
        if (target > 90) error = (target - 2.0) - suhu;
        else if (target > 80) error = (target - 3.5) - suhu;
        else if (target > 60) error = (target - 4.5) - suhu;
        else if (target > 40) error = (target - 5.0) - suhu;

      } else if (modeJauh == 2) {

        // MODE 3
        if (target > 90) error = target - suhu;
        else if (target > 80) error = (target - 1) - suhu;
        else if (target > 60) error = (target - 1.5) - suhu;
        else if (target > 40) error = (target - 2) - suhu;
        else error = (target - 2.5) - suhu;
      }

      if (abs(error) < 0.5) {
        modeJauh = 2;
      }

      // PID
      P = setting.Kp * error;

      // Integral decay
      if (abs(error) < 0.2) I *= 0.6;
      else if (abs(error) < 0.6) I *= 0.7;
      else if (abs(error) < 1) I *= 0.8;
      else I += setting.Ki * error * dT;

      I = constrain(I, 0, 100);

      if (dT > 0)
        D = setting.Kd * (error - prevousError) / dT;
      else
        D = 0;

      output = constrain(P + I + D, 0, 100);

      prevousError = error;
      lastTime = now;
    }

  } else {

    output = 0;
    P = 0;
    I = 0;
    D = 0;
    error = 0;
    prevousError = 0;
  }
}

void serialPlotter() {
  static uint32_t lastPlot = 0;
  if (millis() - lastPlot < 50) return;
  lastPlot = millis();
  Serial.print("BaseLine:");
  Serial.print(0);

  Serial.print(" ");

  Serial.print("Suhu:");
  Serial.print(readSuhu());

  Serial.print(" ");

  Serial.print("SetSuhu:");
  Serial.print(setting.SetPoint);

  Serial.print(" ");

  Serial.print("Error:");
  Serial.print(error);

  Serial.print(" ");

  Serial.print("Dimmer:");
  Serial.println(output / 2);
}

void sendTelemetry() {
  TelemetryData_t data;

  data.tick_ms = millis();
  data.setpoint = setting.SetPoint;
  data.temp = readSuhu();
  data.kp = setting.Kp;
  data.ki = setting.Ki;
  data.kd = setting.Kd;
  data.dimming = output;

  Serial.write("SOF", 3);

  uint8_t len = sizeof(data);
  Serial.write(&len, 1);

  Serial.write((uint8_t *)&data, sizeof(data));

  uint8_t crc = 0;
  Serial.write(&crc, 1);
}

bool receiveCommand() {
  static uint8_t buffer[32];

  if (Serial.available() < 21)
    return false;

  Serial.readBytes((char *)buffer, 21);

  if (memcmp(buffer, "CMD", 3) != 0)
    return false;

  uint8_t len = buffer[3];

  if (len != sizeof(CommandData_t))
    return false;

  CommandData_t cmd;
  memcpy(&cmd, &buffer[4], sizeof(cmd));

  setting.Kp = cmd.kp;
  setting.Ki = cmd.ki;
  setting.Kd = cmd.kd;
  setting.SetPoint = cmd.setpoint;

  return true;
}

void setup() {
  Serial.begin(115200);
  EEPROM.begin(512);
  // defaultSetting();
  // writeEEPROM();
  readEEPROM();

  Wire.begin();
  Wire.setClock(800000);
  lcd.begin(SH1106_SWITCHCAPVCC, 0x3C);
  lcd.setRotation(2);

  initLM35();
  initEncoder();
  initBuzzLed();
  initDimmer();

  lcd.clearDisplay();
  delay(500);
  lcd.display();

  BuzzGreeting();
}

void loop() {
  receiveCommand();

  if (setting.KondisiAlat) {
    suhuPID(setting.SetPoint);
    blink99x();
  } else suhuPID(0);
  dimmer(output);

  static uint32_t lastSend = 0;

  if (millis() - lastSend >= 100) {
    lastSend = millis();
    sendTelemetry();
  }

  lcd.clearDisplay();
  Mainmenu();
  lcd.display();
}
