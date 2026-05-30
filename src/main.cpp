#include <Arduino.h>
#include <WiFi.h>

#include "DevIsoInput.h"
#include "DevSwitch.h"
#include "DevRelay.h"
#include "DevPZEM.h"
#include "DevXYMDSensor.h"
#include "DevOLED.h"
#include "DevWifiManager.h"
#include "DevWeather.h"
#include "DevDS18B20.h"
#include "DevWebServer.h"

// --- Switch: Active Low, External Pull-up 10kΩ ---
DevSwitch sw1(34, false);
DevSwitch sw2(35, false);
DevSwitch sw3(32, false);

// --- Relay: Active Low ---
DevRelay relay1(17, true);
DevRelay relay2(16, true);
DevRelay relay3(4,  true);

// --- OLED: I2C SDA=21, SCL=22 ---
DevOLED oled;

// --- WiFi Manager ---
DevWifiManager wifiMgr(&oled, "ESP32-Setup");

// --- Weather ---
DevWeather weather;

// --- DS18B20: GPIO14 ---
DevDS18B20 ds18(14);

// --- Web Server ---
DevWebServer webServer(&relay1, &relay2, &relay3, &weather, &ds18);

// อัปเดต OLED หน้าจอหลัก
static void _updateDisplay() {
  const WeatherData& w = weather.getData();
  String ip = WiFi.localIP().toString();
  if (w.valid) {
    oled.showMain(ds18.getTemp(), ds18.isSimMode(),
                  w.temp, w.humidity, w.rainChance,
                  w.pm25, w.aqi, aqiLabel(w.aqi),
                  relay1.getState(), relay2.getState(), relay3.getState(),
                  ip.c_str());
  } else {
    // Weather ยังไม่มีข้อมูล — แสดงแค่ DS18B20 + Relay
    oled.showMain(ds18.getTemp(), ds18.isSimMode(),
                  0, 0, 0, 0, 0, "",
                  relay1.getState(), relay2.getState(), relay3.getState(),
                  ip.c_str());
  }
}

// ตรวจสอบว่า sw1 ค้างครบ HOLD_SEC วินาที — คืนค่า true ถ้าให้ reset
static bool checkWifiResetHold(DevSwitch& sw, DevOLED& disp, int holdSec = 5) {
  sw.begin();
  if (!sw.readRawState()) return false;

  for (int remain = holdSec; remain > 0; remain--) {
    disp.showCountdown(remain, holdSec);
    unsigned long tick = millis();
    while (millis() - tick < 1000) {
      if (!sw.readRawState()) {
        disp.showMessage("WiFi Reset", "Cancelled", "");
        delay(1000);
        return false;
      }
      delay(50);
    }
  }
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

  // DS18B20 init (auto fallback to sim)
  oled.showMessage("DS18B20", "Initializing...", "GPIO14");
  ds18.begin();

  // ตรวจ sw1 ค้าง 5 วินาที เพื่อ reset WiFi
  bool doReset = checkWifiResetHold(sw1, oled, 5);

  // เชื่อมต่อ WiFi
  wifiMgr.begin(doReset);

  String ip = wifiMgr.localIP().toString();
  Serial.printf("WiFi connected. IP: %s\n", ip.c_str());
  oled.showIP(ip.c_str());
  delay(2000);

  // ดึงข้อมูล Weather ครั้งแรก
  oled.showMessage("Weather", "Fetching...", OWM_CITY_NAME);
  weather.update();

  // อัปเดต OLED และเริ่ม Web Server
  _updateDisplay();
  webServer.setOnRelayChange(_updateDisplay);
  webServer.begin();

  Serial.println("Ready");
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

  // DS18B20 อัปเดตทุก 2 วินาที
  if (ds18.update()) changed = true;

  // Weather อัปเดตตามรอบ
  if (weather.isDue()) {
    weather.update();
    changed = true;
  }

  if (changed) _updateDisplay();

  // สลับหน้า OLED อัตโนมัติทุก PAGE_INTERVAL ms
  oled.tick();

  webServer.loop();
}
