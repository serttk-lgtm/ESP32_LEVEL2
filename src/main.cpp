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
#include "DevMQTT.h"
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
DevXYMDSensor xymd(&Serial, 2, 3000);

// --- MQTT ---
DevMQTT mqtt(&relay1, &relay2, &relay3, &weather, &ds18, &xymd);

// --- Web Server ---
DevWebServer webServer(&relay1, &relay2, &relay3, &weather, &ds18, &xymd);

// ── อัปเดต OLED ──────────────────────────────────────────────
static void _updateDisplay() {
  const WeatherData& w = weather.getData();
  String ip = WiFi.localIP().toString();

  oled.showMain(
    ds18.getTemp(),        ds18.isSimMode(),
    xymd.getTemperature(), xymd.getHumidity(), xymd.isSimMode(),
    w.valid ? w.temp       : 0,
    w.valid ? w.humidity   : 0,
    w.valid ? w.rainChance : 0,
    w.valid ? w.pm25       : 0,
    w.valid ? w.aqi        : 0,
    w.valid ? aqiLabel(w.aqi) : "--",
    relay1.getState(), relay2.getState(), relay3.getState(),
    ip.c_str()
  );
}

// ── relay toggle จาก physical switch ─────────────────────────
static void _toggleRelay(int n) {
  DevRelay* r[] = {&relay1, &relay2, &relay3};
  r[n-1]->toggle();
  Serial.printf("Relay%d = %s\n", n, r[n-1]->getState() ? "ON" : "OFF");
  mqtt.publishRelayState(n);
  _updateDisplay();
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

  oled.showMessage("DS18B20", "Initializing...", "GPIO14");
  ds18.begin();

  bool doReset = checkWifiResetHold(sw1, oled, 5);
  wifiMgr.begin(doReset);

  String ip = wifiMgr.localIP().toString();
  oled.showIP(ip.c_str());
  delay(2000);

  oled.showMessage("XY-MD03", "Initializing...", "Serial0 ID:2");
  xymd.begin(9600);

  oled.showMessage("Weather", "Fetching...", OWM_CITY_NAME);
  weather.update();

  // MQTT — callback อัปเดต OLED + publish relay เมื่อถูกสั่งผ่าน MQTT
  auto mqttRelayChangedCb = []() {
    _updateDisplay();
    // publish state ของทุก relay (MQTT callback handle ไว้แล้ว relay เดียว)
  };
  mqtt.setOnRelayChange(mqttRelayChangedCb);
  oled.showMessage("MQTT", "Connecting...", MQTT_HOST);
  mqtt.begin();

  // Web Server
  webServer.setOnRelayChange([]() {
    _updateDisplay();
    // sync relay state กลับ MQTT เมื่อ toggle จาก Web
    mqtt.publishRelayState(1);
    mqtt.publishRelayState(2);
    mqtt.publishRelayState(3);
  });
  webServer.begin();

  _updateDisplay();
  Serial.println("Ready");
}

void loop() {
  sw1.update();
  sw2.update();
  sw3.update();

  if (sw1.wasPressed()) _toggleRelay(1);
  if (sw2.wasPressed()) _toggleRelay(2);
  if (sw3.wasPressed()) _toggleRelay(3);

  if (ds18.update()) _updateDisplay();
  if (xymd.update()) _updateDisplay();

  if (weather.isDue()) {
    weather.update();
    _updateDisplay();
  }

  // sync MQTT connected state → Web dashboard
  webServer.setMqttConnected(mqtt.isConnected());

  mqtt.loop();
  oled.tick();
  webServer.loop();
}
