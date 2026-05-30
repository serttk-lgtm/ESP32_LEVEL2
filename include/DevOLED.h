#ifndef DEV_OLED_H
#define DEV_OLED_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_WIDTH  128
#define OLED_HEIGHT  64
#define OLED_ADDR   0x3C

class DevOLED {
private:
  Adafruit_SSD1306 display;
  bool ready;

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
    return true;
  }

  // แสดงสถานะ Relay 1-3 บนหน้าจอ
  void showRelayStatus(bool r1, bool r2, bool r3) {
    if (!ready) return;

    display.clearDisplay();

    // หัวข้อ
    display.setTextSize(1);
    display.setCursor(20, 0);
    display.println("=== RELAY STATUS ===");

    // Relay 1
    display.setTextSize(2);
    display.setCursor(0, 18);
    display.print("R1:");
    display.setTextColor(r1 ? SSD1306_WHITE : SSD1306_BLACK);
    display.fillRect(48, 16, 50, 18, r1 ? SSD1306_WHITE : SSD1306_BLACK);
    display.setTextColor(r1 ? SSD1306_BLACK : SSD1306_WHITE);
    display.setCursor(50, 18);
    display.print(r1 ? " ON " : " OFF");
    display.setTextColor(SSD1306_WHITE);

    // Relay 2
    display.setCursor(0, 36);
    display.print("R2:");
    display.fillRect(48, 34, 50, 18, r2 ? SSD1306_WHITE : SSD1306_BLACK);
    display.setTextColor(r2 ? SSD1306_BLACK : SSD1306_WHITE);
    display.setCursor(50, 36);
    display.print(r2 ? " ON " : " OFF");
    display.setTextColor(SSD1306_WHITE);

    // Relay 3
    display.setCursor(0, 54);
    display.print("R3:");
    display.fillRect(48, 52, 50, 18, r3 ? SSD1306_WHITE : SSD1306_BLACK);
    display.setTextColor(r3 ? SSD1306_BLACK : SSD1306_WHITE);
    display.setCursor(50, 54);
    display.print(r3 ? " ON " : " OFF");
    display.setTextColor(SSD1306_WHITE);

    display.display();
  }

  // แสดง countdown นับถอยหลัง (วินาที) พร้อมข้อความกำกับ
  void showCountdown(int seconds, int total, const char* title = "Hold to Reset WiFi") {
    if (!ready) return;

    display.clearDisplay();

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(title);

    // progress bar
    int barW = map(total - seconds, 0, total, 0, 124);
    display.drawRect(2, 14, 124, 10, SSD1306_WHITE);
    display.fillRect(2, 14, barW, 10, SSD1306_WHITE);

    // ตัวเลขนับถอยหลัง
    display.setTextSize(4);
    int x = (seconds >= 10) ? 44 : 52;
    display.setCursor(x, 28);
    display.print(seconds);

    display.setTextSize(1);
    display.setCursor(30, 56);
    display.print("Release to cancel");

    display.display();
  }

  // แสดง IP address หลังเชื่อมต่อ WiFi สำเร็จ
  void showIP(const char* ip) {
    if (!ready) return;

    display.clearDisplay();

    display.setTextSize(1);
    display.setCursor(28, 0);
    display.println("WiFi Connected");

    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(0, 14);
    display.println("IP Address:");

    display.setTextSize(2);
    // จัดกึ่งกลาง IP
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(ip, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((128 - w) / 2, 28);
    display.println(ip);

    display.display();
  }

  // แสดงข้อความทั่วไป 3 บรรทัด
  void showMessage(const char* line1, const char* line2 = "", const char* line3 = "") {
    if (!ready) return;

    display.clearDisplay();

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(line1);
    display.drawLine(0, 10, 127, 10, SSD1306_WHITE);

    display.setTextSize(1);
    display.setCursor(0, 16);
    display.println(line2);

    display.setCursor(0, 30);
    display.println(line3);

    display.display();
  }

  bool isReady() const { return ready; }
};

#endif // DEV_OLED_H
