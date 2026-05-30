#include <Arduino.h>

#include "DevIsoInput.h"
#include "DevSwitch.h"
#include "DevRelay.h"
#include "DevPZEM.h"
#include "DevXYMDSensor.h"
#include "DevOLED.h"
#include "DevWifiManager.h"
#include "DevWeather.h"
#include "DevWebServer.h"

// --- Switch: Active Low, External Pull-up 10kΩ (ดู blueprint.md) ---
DevSwitch sw1(34, false);
DevSwitch sw2(35, false);
DevSwitch sw3(32, false);

// --- Relay: Active Low (ดู blueprint.md) ---
DevRelay relay1(17, true);
DevRelay relay2(16, true);
DevRelay relay3(4,  true);

// --- OLED: I2C SDA=21, SCL=22 ---
DevOLED oled;

// --- WiFi Manager ---
DevWifiManager wifiMgr(&oled, "ESP32-Setup");

// --- Weather ---
DevWeather weather;

// --- Web Server ---
DevWebServer webServer(&relay1, &relay2, &relay3, &weather);

// อัปเดต OLED ด้วยข้อมูล Weather + สถานะ Relay ปัจจุบัน
static void _updateDisplay() {
  const WeatherData& w = weather.getData();
  if (w.valid) {
    oled.showWeather(w.temp, w.humidity, w.rainChance, w.pm25, w.aqi,
                     aqiLabel(w.aqi),
                     relay1.getState(), relay2.getState(), relay3.getState());
  } else {
    oled.showRelayStatus(relay1.getState(), relay2.getState(), relay3.getState());
  }
}

// ตรวจสอบว่า sw1 ค้างครบ HOLD_SEC วินาที — คืนค่า true ถ้าให้ reset
static bool checkWifiResetHold(DevSwitch& sw, DevOLED& disp, int holdSec = 5) {
  sw.begin();

  // อ่านสถานะโดยตรง (ไม่ผ่าน debounce loop) เพื่อตรวจว่ากดอยู่ตั้งแต่แรก
  if (!sw.readRawState()) {
    return false;  // ไม่ได้กด — ข้ามไป
  }

  // กดอยู่ — เริ่มนับถอยหลัง
  unsigned long start = millis();
  for (int remain = holdSec; remain > 0; remain--) {
    disp.showCountdown(remain, holdSec);

    // รอ 1 วินาที โดยตรวจ raw state ทุก 50ms
    unsigned long tick = millis();
    while (millis() - tick < 1000) {
      if (!sw.readRawState()) {
        // ปล่อยก่อนครบ — ยกเลิก
        disp.showMessage("WiFi Reset", "Cancelled", "");
        delay(1000);
        return false;
      }
      delay(50);
    }
  }

  // ครบ 5 วินาที
  disp.showMessage("WiFi Reset", "Resetting...", "");
  delay(800);
  return true;
}

void setup() {
  Serial.begin(115200);

  oled.begin(21, 22);
  oled.showMessage("Booting...", "", "");

  sw2.begin();
  sw3.begin();
  relay1.begin();
  relay2.begin();
  relay3.begin();

  // ตรวจ sw1 ค้าง 5 วินาที เพื่อ reset WiFi
  bool doReset = checkWifiResetHold(sw1, oled, 5);

  // เชื่อมต่อ WiFi (บล็อกจนสำเร็จ)
  wifiMgr.begin(doReset);

  // แสดง IP หลังเชื่อมต่อ
  String ip = wifiMgr.localIP().toString();
  Serial.printf("WiFi connected. IP: %s\n", ip.c_str());
  oled.showIP(ip.c_str());
  delay(2000);

  // ดึงข้อมูล Weather ครั้งแรก
  oled.showMessage("Weather", "Fetching...", OWM_CITY_NAME);
  weather.update();

  // แสดงหน้า Weather + Relay status
  _updateDisplay();

  // เริ่ม Web Server — ผูก callback อัปเดต OLED เมื่อ relay เปลี่ยนผ่าน web
  webServer.setOnRelayChange(_updateDisplay);
  webServer.begin();

  Serial.println("Ready — SW1/SW2/SW3 toggles Relay1/2/3");
}

void loop() {
  sw1.update();
  sw2.update();
  sw3.update();

  bool changed = false;

  if (sw1.wasPressed()) {
    relay1.toggle();
    Serial.printf("Relay1 = %s\n", relay1.getState() ? "ON" : "OFF");
    changed = true;
  }

  if (sw2.wasPressed()) {
    relay2.toggle();
    Serial.printf("Relay2 = %s\n", relay2.getState() ? "ON" : "OFF");
    changed = true;
  }

  if (sw3.wasPressed()) {
    relay3.toggle();
    Serial.printf("Relay3 = %s\n", relay3.getState() ? "ON" : "OFF");
    changed = true;
  }

  // อัปเดต Weather ตามรอบเวลา
  if (weather.isDue()) {
    weather.update();
    changed = true;
  }

  if (changed) {
    _updateDisplay();
  }

  // Web Server loop (WebSocket broadcast + cleanup)
  webServer.loop();
}
