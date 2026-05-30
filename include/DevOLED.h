#ifndef DEV_OLED_H
#define DEV_OLED_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_WIDTH   128
#define OLED_HEIGHT   64
#define OLED_ADDR    0x3C
#define PAGE_INTERVAL 5000  // ms ต่อหน้า

class DevOLED {
private:
  Adafruit_SSD1306 display;
  bool ready;

  // ── ข้อมูลล่าสุดที่ cache ไว้ ─────────────────────────────────
  float  _ds18Temp  = 0;  bool _ds18Sim = true;
  float  _owmTemp   = 0;  int  _owmHum = 0;
  int    _rainPct   = 0;
  float  _pm25      = 0;  int  _aqi = 0;  char _aqiStr[10] = "";
  bool   _r1 = false, _r2 = false, _r3 = false;
  char   _ip[20]    = "";

  // ── Page switcher ─────────────────────────────────────────────
  uint8_t       _page       = 0;   // 0 = หน้า A, 1 = หน้า B
  unsigned long _pageAt     = 0;   // millis ที่เปลี่ยนหน้าล่าสุด
  bool          _pageForced = false; // true = ไม่สลับอัตโนมัติชั่วคราว

  // ── วาดหน้า A: DS18B20 (ใหญ่) + Relay ───────────────────────
  //
  //  ┌─────────────────────────────┐
  //  │  PAGE 1/2                   │  row 0  size1 label
  //  │                             │
  //  │     32.65°C                 │  row 10 size3 DS18B20
  //  │     [LIVE] / [SIM]          │  row 36 size1 tag
  //  ├─────────────────────────────┤  line y=46
  //  │  RELAY                      │  row 49 size1
  //  │  R1:■  R2:□  R3:■           │  row 55 size1 relay icons
  //  └─────────────────────────────┘
  void _drawPageA() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    char buf[24];

    // header
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("< DS18B20 Sensor  1/2>");

    // temperature — ตัวใหญ่ size3 (24px)
    display.setTextSize(3);
    snprintf(buf, sizeof(buf), "%.2f", _ds18Temp);
    // จัดกึ่งกลาง
    int16_t bx, by; uint16_t bw, bh;
    display.getTextBounds(buf, 0, 0, &bx, &by, &bw, &bh);
    display.setCursor((128 - bw) / 2 - 10, 12);
    display.print(buf);
    // degree C เล็กกว่า
    display.setTextSize(2);
    display.print((char)247);
    display.print("C");

    // SIM / LIVE tag
    display.setTextSize(1);
    if (_ds18Sim) {
      display.setCursor(44, 38);
      display.print("[ SIMULATION ]");
    } else {
      display.setCursor(52, 38);
      display.print("[ LIVE ]");
    }

    // divider
    display.drawLine(0, 47, 127, 47, SSD1306_WHITE);

    // relay label + icons
    display.setCursor(0, 50);
    display.print("RELAY:");
    const int rx[3] = {42, 72, 102};
    for (int i = 0; i < 3; i++) {
      display.setCursor(rx[i], 50);
      display.print(i == 0 ? "R1" : i == 1 ? "R2" : "R3");
      bool on = (i == 0) ? _r1 : (i == 1) ? _r2 : _r3;
      if (on) {
        display.fillRect(rx[i], 58, 18, 6, SSD1306_WHITE);
      } else {
        display.drawRect(rx[i], 58, 18, 6, SSD1306_WHITE);
      }
    }
  }

  // ── วาดหน้า B: Weather OWM + IP ─────────────────────────────
  //
  //  ┌─────────────────────────────┐
  //  │  PAGE 2/2                   │  row 0  size1 label
  //  │  OUT 33.5°C   Hum: 78%     │  row 10 size1
  //  │  Rain: 45%                  │  row 20 size1 + bar
  //  │  PM2.5: 18.2  AQI:2 Fair   │  row 34 size1
  //  ├─────────────────────────────┤  line y=44
  //  │  R1:■  R2:□  R3:■          │  row 48 size1
  //  │  IP: 192.168.1.x            │  row 57 size1
  //  └─────────────────────────────┘
  void _drawPageB() {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    char buf[28];

    // header
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("< Weather OWM    2/2>");

    // outdoor temp + humidity
    display.setCursor(0, 11);
    snprintf(buf, sizeof(buf), "OUT %.1f%cC   Hum:%d%%",
             _owmTemp, (char)247, _owmHum);
    display.print(buf);

    // rain label
    display.setCursor(0, 21);
    snprintf(buf, sizeof(buf), "Rain:%d%%", _rainPct);
    display.print(buf);
    // rain bar
    int barW = (_rainPct * 80) / 100;
    display.drawRect(48, 22, 80, 6, SSD1306_WHITE);
    display.fillRect(48, 22, barW, 6, SSD1306_WHITE);

    // PM2.5 + AQI
    display.setCursor(0, 31);
    snprintf(buf, sizeof(buf), "PM2.5:%.1f  AQI:%d %s",
             _pm25, _aqi, _aqiStr);
    display.print(buf);

    // divider
    display.drawLine(0, 41, 127, 41, SSD1306_WHITE);

    // relay icons compact
    const int rx[3] = {0, 44, 88};
    for (int i = 0; i < 3; i++) {
      display.setCursor(rx[i], 44);
      display.print(i == 0 ? "R1" : i == 1 ? "R2" : "R3");
      display.print(":");
      bool on = (i == 0) ? _r1 : (i == 1) ? _r2 : _r3;
      if (on) {
        display.fillRect(rx[i] + 18, 44, 8, 8, SSD1306_WHITE);
      } else {
        display.drawRect(rx[i] + 18, 44, 8, 8, SSD1306_WHITE);
      }
    }

    // IP address
    display.setCursor(0, 56);
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
    if (!ready) {
      Serial.println("[DevOLED] SSD1306 not found!");
      return false;
    }
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.display();
    _pageAt = millis();
    return true;
  }

  // ── เรียกใน loop() เพื่อสลับหน้าอัตโนมัติ ────────────────────
  void tick() {
    if (!ready) return;
    if (millis() - _pageAt >= PAGE_INTERVAL) {
      _page   = (_page + 1) % 2;
      _pageAt = millis();
      _redraw();
    }
  }

  // ── อัปเดตข้อมูลและวาดใหม่ทันที ─────────────────────────────
  void showMain(float ds18Temp, bool ds18Sim,
                float owmTemp,  int owmHum, int rainPct,
                float pm25,     int aqi,    const char* aqiStr,
                bool r1, bool r2, bool r3,
                const char* ip = "") {
    if (!ready) return;

    // cache ข้อมูล
    _ds18Temp = ds18Temp;  _ds18Sim = ds18Sim;
    _owmTemp  = owmTemp;   _owmHum  = owmHum;
    _rainPct  = rainPct;
    _pm25     = pm25;      _aqi     = aqi;
    strncpy(_aqiStr, aqiStr, sizeof(_aqiStr) - 1);
    _r1 = r1;  _r2 = r2;  _r3 = r3;
    strncpy(_ip, ip, sizeof(_ip) - 1);

    _redraw();
  }

  // ── แสดง countdown (ชั่วคราว ไม่เปลี่ยน page timer) ─────────
  void showCountdown(int seconds, int total,
                     const char* title = "Hold to Reset WiFi") {
    if (!ready) return;
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(title);
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

  // ── แสดง IP (ชั่วคราว) ───────────────────────────────────────
  void showIP(const char* ip) {
    if (!ready) return;
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(28, 0);
    display.println("WiFi Connected");
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
    display.setCursor(0, 14);
    display.println("IP Address:");
    display.setTextSize(2);
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(ip, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((128 - w) / 2, 28);
    display.println(ip);
    display.display();
  }

  // ── ข้อความทั่วไป (ชั่วคราว) ─────────────────────────────────
  void showMessage(const char* line1,
                   const char* line2 = "",
                   const char* line3 = "") {
    if (!ready) return;
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(line1);
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
    display.setCursor(0, 16);
    display.println(line2);
    display.setCursor(0, 30);
    display.println(line3);
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
    showMain(temp, false, temp, hum, rainPct,
             pm25, aqi, aqiStr, r1, r2, r3, ip);
  }

  bool isReady() const { return ready; }
};

#endif // DEV_OLED_H
