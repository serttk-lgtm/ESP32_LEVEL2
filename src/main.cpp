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

// --- XY-MD03: Serial0, Slave ID=2 ---
// Serial0 ใช้ร่วมกับ USB Serial ผ่าน switch สลับ RS232/RS485
DevXYMDSensor xymd(&Serial, 2, 3000);

// --- Web Server ---
DevWebServer webServer(&relay1, &relay2, &relay3, &weather, &ds18, &xymd);

// ── อัปเดต OLED ──────────────────────────────────────────────
static void _updateDisplay() {
  const WeatherData& w = weather.getData();
  String ip = WiFi.localIP().toString();

  oled.showMain(
    ds18.getTemp(),        ds18.isSimMode(),
    xymd.getTemperature(), xymd.getHumidity(), xymd.isSimMode(),
    w.valid ? w.temp  : 0,
    w.valid ? w.humidity : 0,
    w.valid ? w.rainChance : 0,
    w.valid ? w.pm25  : 0,
    w.valid ? w.aqi   : 0,
    w.valid ? aqiLabel(w.aqi) : "--",
    relay1.getState(), relay2.getState(), relay3.getState(),
    ip.c_str()
  );
}

// ── WiFi reset hold ───────────────────────────────────────────
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

  // DS18B20 — ไม่ใช้ Serial ไม่กระทบ Serial0
  oled.showMessage("DS18B20", "Initializing...", "GPIO14");
  ds18.begin();

  // WiFi reset check (ก่อน re-init Serial0 เพื่อ Modbus)
  bool doReset = checkWifiResetHold(sw1, oled, 5);
  wifiMgr.begin(doReset);

  String ip = wifiMgr.localIP().toString();
  Serial.printf("WiFi connected. IP: %s\n", ip.c_str());
  oled.showIP(ip.c_str());
  delay(2000);

  // XY-MD03 — begin() จะ reinit Serial0 เป็น 9600 สำหรับ Modbus
  // หลังจาก WiFiManager เสร็จแล้ว (ไม่ใช้ Serial0 แล้ว)
  oled.showMessage("XY-MD03", "Initializing...", "Serial0 ID:2");
  xymd.begin(9600);

  // Weather
  oled.showMessage("Weather", "Fetching...", OWM_CITY_NAME);
  weather.update();

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

  if (ds18.update())   changed = true;
  if (xymd.update())   changed = true;

  if (weather.isDue()) {
    weather.update();
    changed = true;
  }

  if (changed) _updateDisplay();

  oled.tick();
  webServer.loop();
}
