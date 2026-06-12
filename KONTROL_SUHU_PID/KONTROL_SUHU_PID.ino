#include <EEPROM.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH1106.h>
#include <ArduinoJson.h>

#include "LM35.h"
#include "Encoder.h"
#include "BuzzLed.h"
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
  else {
    if (readButton() == 2) {
      delay(200);
      ESP.restart();
    }
    lcd.fillRoundRect(30, 0, 98, 13, 1, 1);
  }
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

        if (selisih >= 20)
          modeJauh = 0;  // Mode 1
        else if (selisih >= 12)
          modeJauh = 1;  // Mode 2
        else if (selisih >= 5)
          modeJauh = 2;  // Mode 3
        else modeJauh = 3;

        lastTarget = target;
      }

      if ((target - suhu) < 0.35) {
        modeJauh = 3;
      }

      // Hitung error berdasarkan mode
      if (modeJauh == 0) {  // MODE 1
        if (target > 90) error = (target - 3.0) - suhu;
        else if (target > 80) error = (target - 3.5) - suhu;
        else if (target > 60) error = (target - 4.0) - suhu;
        else if (target > 40) error = (target - 4.5) - suhu;
      } else if (modeJauh == 1) {  // MODE 2
        if (target > 90) error = (target - 1.5) - suhu;
        else if (target > 80) error = (target - 1.8) - suhu;
        else if (target > 60) error = (target - 2.3) - suhu;
        else if (target > 40) error = (target - 2.5) - suhu;
      } else if (modeJauh == 2) {  // MODE 3
        if (target > 90) error = target - suhu;
        else if (target > 80) error = (target - 0.5) - suhu;
        else if (target > 60) error = (target - 1.0) - suhu;
        else if (target > 40) error = (target - 1.5) - suhu;
        else error = (target - 1.5) - suhu;
      } else if (modeJauh == 3) {
        error = target - suhu;
      }

      // PID
      P = setting.Kp * error;

      // Integral decay
      if (modeJauh < 3) {
        I += setting.Ki * error * dT;
        if (abs(error) < 0.1) I *= 0.5;
        else if (abs(error) < 0.5) I *= 0.6;
        else if (abs(error) < 1) I *= 0.7;
        else if (abs(error) < 1.5) I *= 0.8;
        else if (abs(error) < 2) I *= 0.9;
      } else {
        I += setting.Ki * error * dT;
        if (abs(error) < 0.1) I *= 0.5;
        else if (abs(error) < 0.5) I *= 0.7;
        else if (abs(error) < 1) I *= 0.9;
      }

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

void terimaParameterGUI() {
  if (Serial.available() > 0) {
    // Baca satu baris data serial hingga menemukan karakter newline '\n'
    String inputStr = Serial.readStringUntil('\n');
    inputStr.trim();  // Hapus spasi kosong di awal dan akhir string

    // Pastikan string tidak kosong sebelum diproses
    if (inputStr.length() == 0) return;

    // Cari posisi index/tag masing-masing parameter
    int spIdx = inputStr.indexOf("SP:");
    int tsIdx = inputStr.indexOf(",TS:");
    int kpIdx = inputStr.indexOf(",KP:");
    int kiIdx = inputStr.indexOf(",KI:");
    int kdIdx = inputStr.indexOf(",KD:");

    // Validasi: pastikan semua tag parameter ditemukan di dalam string
    if (spIdx != -1 && tsIdx != -1 && kpIdx != -1 && kiIdx != -1 && kdIdx != -1) {

      // Ekstrak nilai Setpoint (SP) di antara "SP:" dan ",TS:"
      setting.SetPoint = inputStr.substring(spIdx + 3, tsIdx).toFloat();

      // Ekstrak nilai Sampling Time (Ts) di antara ",TS:" dan ",KP:"
      setting.Ts = inputStr.substring(tsIdx + 4, kpIdx).toFloat();

      // Ekstrak nilai Kp di antara ",KP:" dan ",KI:"
      setting.Kp = inputStr.substring(kpIdx + 4, kiIdx).toFloat();

      // Ekstrak nilai Ki di antara ",KI:" dan ",KD:"
      setting.Ki = inputStr.substring(kiIdx + 4, kdIdx).toFloat();

      // Ekstrak nilai Kd dari posisi setelah ",KD:" hingga akhir string
      setting.Kd = inputStr.substring(kdIdx + 4).toFloat();

      // Proteksi pengaman batas nilai setpoint sesuai spesifikasi (30 - 99 °C)
      if (setting.SetPoint < 30.0) setting.SetPoint = 30.0;
      if (setting.SetPoint > 99.0) setting.SetPoint = 99.0;

      // Proteksi pengaman batas minimum Sampling Time agar sistem tidak crash
      if (setting.SetPoint < 0.1) setting.SetPoint = 0.1;
      writeEEPROM();
    }
  }
}

void kirimTelemetryGUI() {
  // Gunakan Serial.print dengan separator koma
  Serial.print(readSuhu(), 2);  // 1. Mengirim suhu aktual (2 angka desimal)
  Serial.print(",");
  Serial.print(setting.SetPoint, 1);  // 2. Mengirim setpoint aktif (1 angka desimal)
  Serial.print(",");
  Serial.print(P, 2);  // 3. Mengirim komponen P
  Serial.print(",");
  Serial.print(I, 2);  // 4. Mengirim komponen I
  Serial.print(",");
  Serial.print(D, 2);  // 5. Mengirim komponen D
  Serial.print(",");
  Serial.print(output, 1);  // 6. Mengirim % output dimmer
  Serial.print(",");
  Serial.print(setting.Kp, 2);  // 7. Tambahan: Mengirim nilai Kp aktif alat ke GUI
  Serial.print(",");
  Serial.print(setting.Ki, 2);  // 8. Tambahan: Mengirim nilai Ki aktif alat ke GUI
  Serial.print(",");
  Serial.print(setting.Kd, 2);  // 9. Tambahan: Mengirim nilai Kd aktif alat ke GUI
  Serial.print(",");
  Serial.println(setting.Ts, 1);  // 10. Tambahan: Mengirim nilai Ts aktif alat ke GUI (akhiri \n)
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
  if (setting.KondisiAlat) {
    suhuPID(setting.SetPoint);
    blink99x();
  } else suhuPID(0);
  dimmer(output);
  terimaParameterGUI();
  static uint32_t lastSend = 0;

  if (millis() - lastSend >= 100) {
    lastSend = millis();
    kirimTelemetryGUI();
  }

  lcd.clearDisplay();
  Mainmenu();
  lcd.display();
}
