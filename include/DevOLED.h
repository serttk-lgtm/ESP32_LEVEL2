#ifndef DEV_OLED_H
#define DEV_OLED_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_WIDTH    128
#define OLED_HEIGHT    64
#define OLED_ADDR     0x3C
#define PAGE_INTERVAL 5000   // ms ต่อหน้า

class DevOLED {
private:
  Adafruit_SSD1306 display;
  bool ready;

  // ── data cache ────────────────────────────────────────────────
  // DS18B20
  float _ds18Temp = 0;  bool _ds18Sim = true;
  // XYMD
  float _xymdTemp = 0;  float _xymdHum = 0;  bool _xymdSim = true;
  // OWM
  float _owmTemp  = 0;  int   _owmHum  = 0;
  int   _rainPct  = 0;  float _pm25    = 0;
  int   _aqi      = 0;  char  _aqiStr[10] = "";
  // Relay + network
  bool  _r1 = false, _r2 = false, _r3 = false;
  char  _ip[20] = "";

  // ── page ──────────────────────────────────────────────────────
  uint8_t       _page   = 0;
  unsigned long _pageAt = 0;

  // ── helper: relay icon bar ────────────────────────────────────
  void _relayBar(int y) {
    // "R1:■  R2:□  R3:■" ที่ row y
    const int rx[3] = {0, 44, 88};
    for (int i = 0; i < 3; i++) {
      display.setCursor(rx[i], y);
      display.print(i == 0 ? "R1" : i == 1 ? "R2" : "R3");
      display.print(":");
      bool on = (i == 0) ? _r1 : (i == 1) ? _r2 : _r3;
      if (on) display.fillRect(rx[i] + 18, y, 8, 8, SSD1306_WHITE);
      else    display.drawRect(rx[i] + 18, y, 8, 8, SSD1306_WHITE);
    }
  }

  // ── PAGE A: DS18B20 + XYMD ───────────────────────────────────
  //
  //  ┌──────────────────────────────┐
  //  │< Sensors            1/2 >    │  row 0   header
  //  ├──────────────────────────────┤  line 9
  //  │ DS18B20              [SIM]   │  row 11  label + tag
  //  │   32.65°C                    │  row 19  size2
  //  ├──────────────────────────────┤  line 35
  //  │ XYMD (ID:2)          [LIVE]  │  row 37  label + tag
  //  │   T:28.4°C  H:68.2%          │  row 45  size1
  //  ├──────────────────────────────┤  line 54
  //  │ R1:■  R2:□  R3:■             │  row 56  relay
  //  └──────────────────────────────┘
  void _drawPageA() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    char buf[24];

    // header
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("< Sensors            1/2>");
    display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

    // DS18B20 label + SIM/LIVE tag
    display.setCursor(0, 11);
    display.print("DS18B20");
    display.setCursor(_ds18Sim ? 86 : 92, 11);
    display.print(_ds18Sim ? "[SIM]" : "[OK]");

    // DS18B20 temperature size2
    display.setTextSize(2);
    display.setCursor(8, 19);
    snprintf(buf, sizeof(buf), "%.2f", _ds18Temp);
    display.print(buf);
    display.setTextSize(1);
    display.print((char)247);
    display.print("C");

    display.drawLine(0, 35, 127, 35, SSD1306_WHITE);

    // XYMD label + SIM/LIVE tag
    display.setTextSize(1);
    display.setCursor(0, 37);
    display.print("XYMD (ID:2)");
    display.setCursor(_xymdSim ? 86 : 92, 37);
    display.print(_xymdSim ? "[SIM]" : "[OK]");

    // XYMD values size1
    display.setCursor(4, 46);
    snprintf(buf, sizeof(buf), "T:%.1f%cC  H:%.1f%%",
             _xymdTemp, (char)247, _xymdHum);
    display.print(buf);

    display.drawLine(0, 55, 127, 55, SSD1306_WHITE);
    _relayBar(57);
  }

  // ── PAGE B: Weather OWM + IP ─────────────────────────────────
  //
  //  ┌──────────────────────────────┐
  //  │< Weather OWM        2/2 >    │  row 0   header
  //  ├──────────────────────────────┤  line 9
  //  │ OUT 33.5°C   Hum:78%         │  row 11
  //  │ Rain:45% [████████░░]        │  row 21 + bar
  //  │ PM2.5:18.2   AQI:2 Fair      │  row 31
  //  ├──────────────────────────────┤  line 41
  //  │ R1:■  R2:□  R3:■             │  row 43
  //  │ IP:192.168.1.x               │  row 54
  //  └──────────────────────────────┘
  void _drawPageB() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    char buf[28];

    // header
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("< Weather OWM        2/2>");
    display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

    // outdoor temp + hum
    display.setCursor(0, 11);
    snprintf(buf, sizeof(buf), "OUT %.1f%cC   Hum:%d%%",
             _owmTemp, (char)247, _owmHum);
    display.print(buf);

    // rain label + bar
    display.setCursor(0, 21);
    snprintf(buf, sizeof(buf), "Rain:%d%%", _rainPct);
    display.print(buf);
    int barW = (_rainPct * 76) / 100;
    display.drawRect(50, 22, 76, 6, SSD1306_WHITE);
    if (barW > 0) display.fillRect(50, 22, barW, 6, SSD1306_WHITE);

    // PM2.5 + AQI
    display.setCursor(0, 31);
    snprintf(buf, sizeof(buf), "PM2.5:%.1f  AQI:%d %s",
             _pm25, _aqi, _aqiStr);
    display.print(buf);

    display.drawLine(0, 41, 127, 41, SSD1306_WHITE);

    _relayBar(43);

    // IP
    display.setCursor(0, 55);
    snprintf(buf, sizeof(buf), "IP:%s", _ip);
    display.print(buf);
  }

  void _redraw() {
    if (_page == 0) _drawPageA();
    else            _drawPageB();
    display.display();
  }

public:
  DevOLED() : display(OLED_WIDTH, OLED_HEIGHT, &Wire, -1), ready(false) {}

  bool begin(uint8_t sda = 21, uint8_t scl = 22) {
    Wire.begin(sda, scl);
    ready = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
    if (!ready) { Serial.println("[DevOLED] not found!"); return false; }
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.display();
    _pageAt = millis();
    return true;
  }

  // ── เรียกใน loop() — สลับหน้าอัตโนมัติ ──────────────────────
  void tick() {
    if (!ready) return;
    if (millis() - _pageAt >= PAGE_INTERVAL) {
      _page   = (_page + 1) % 2;
      _pageAt = millis();
      _redraw();
    }
  }

  // ── อัปเดต data cache แล้ว redraw ────────────────────────────
  void showMain(float ds18Temp,  bool ds18Sim,
                float xymdTemp,  float xymdHum,  bool xymdSim,
                float owmTemp,   int   owmHum,   int rainPct,
                float pm25,      int   aqi,      const char* aqiStr,
                bool r1, bool r2, bool r3,
                const char* ip = "") {
    if (!ready) return;

    _ds18Temp = ds18Temp;  _ds18Sim = ds18Sim;
    _xymdTemp = xymdTemp;  _xymdHum = xymdHum;  _xymdSim = xymdSim;
    _owmTemp  = owmTemp;   _owmHum  = owmHum;
    _rainPct  = rainPct;   _pm25    = pm25;
    _aqi      = aqi;
    strncpy(_aqiStr, aqiStr, sizeof(_aqiStr) - 1);
    _r1 = r1;  _r2 = r2;  _r3 = r3;
    strncpy(_ip, ip, sizeof(_ip) - 1);

    _redraw();
  }

  // ── ข้อความทั่วไป (ชั่วคราว) ─────────────────────────────────
  void showMessage(const char* line1,
                   const char* line2 = "",
                   const char* line3 = "") {
    if (!ready) return;
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);  display.println(line1);
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
    display.setCursor(0, 16); display.println(line2);
    display.setCursor(0, 30); display.println(line3);
    display.display();
  }

  // ── countdown (ชั่วคราว) ─────────────────────────────────────
  void showCountdown(int seconds, int total,
                     const char* title = "Hold to Reset WiFi") {
    if (!ready) return;
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);  display.println(title);
    int barW = map(total - seconds, 0, total, 0, 124);
    display.drawRect(2, 14, 124, 10, SSD1306_WHITE);
    display.fillRect(2, 14, barW, 10, SSD1306_WHITE);
    display.setTextSize(4);
    display.setCursor((seconds >= 10) ? 44 : 52, 28);
    display.print(seconds);
    display.setTextSize(1);
    display.setCursor(30, 56);
    display.print("Release to cancel");
    display.display();
  }

  // ── IP screen (ชั่วคราว) ─────────────────────────────────────
  void showIP(const char* ip) {
    if (!ready) return;
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(28, 0);   display.println("WiFi Connected");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
    display.setCursor(0, 14);   display.println("IP Address:");
    display.setTextSize(2);
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(ip, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((128 - w) / 2, 28);
    display.println(ip);
    display.display();
  }

  // ── backward-compat ───────────────────────────────────────────
  void showRelayStatus(bool r1, bool r2, bool r3) {
    _r1 = r1; _r2 = r2; _r3 = r3;
    _redraw();
  }

  void showWeather(float temp, int hum, int rainPct, float pm25,
                   int aqi, const char* aqiStr,
                   bool r1, bool r2, bool r3,
                   const char* ip = "") {
    showMain(temp, false,
             0, 0, true,
             temp, hum, rainPct,
             pm25, aqi, aqiStr,
             r1, r2, r3, ip);
  }

  bool isReady() const { return ready; }
};

#endif // DEV_OLED_H
