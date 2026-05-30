#ifndef DEV_DS18B20_H
#define DEV_DS18B20_H

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>

class DevDS18B20 {
private:
  OneWire           wire;
  DallasTemperature sensors;
  uint8_t           pin;
  bool              simMode;
  float             lastTemp;
  unsigned long     lastReadAt;
  unsigned long     simStart;

  static const unsigned long READ_INTERVAL = 2000; // ms

  float _simTemp() {
    // จำลอง 28–34°C แกว่งตาม sine wave
    float t = (millis() - simStart) / 1000.0f;
    return 31.0f + 3.0f * sinf(t * 0.05f);
  }

public:
  DevDS18B20(uint8_t gpioPin)
    : wire(gpioPin), sensors(&wire),
      pin(gpioPin), simMode(false),
      lastTemp(0), lastReadAt(0), simStart(0) {}

  // คืน true = sensor จริง, false = simulation
  bool begin() {
    sensors.begin();
    uint8_t count = sensors.getDeviceCount();
    if (count == 0) {
      simMode   = true;
      simStart  = millis();
      lastTemp  = _simTemp();
      Serial.printf("[DS18B20] No sensor on GPIO%d → Simulation mode\n", pin);
      return false;
    }
    sensors.setResolution(12); // 12-bit = 0.0625°C
    simMode = false;
    Serial.printf("[DS18B20] Found %d sensor(s) on GPIO%d\n", count, pin);
    return true;
  }

  // เรียกใน loop() — อ่านทุก READ_INTERVAL ms
  // คืน true ถ้ามีค่าใหม่
  bool update() {
    if (millis() - lastReadAt < READ_INTERVAL) return false;
    lastReadAt = millis();

    if (simMode) {
      lastTemp = _simTemp();
      return true;
    }

    sensors.requestTemperatures();
    float t = sensors.getTempCByIndex(0);
    if (t == DEVICE_DISCONNECTED_C) {
      // sensor หลุด → fallback simulation
      simMode  = true;
      simStart = millis();
      lastTemp = _simTemp();
      Serial.println("[DS18B20] Disconnected → Simulation mode");
    } else {
      lastTemp = t;
    }
    return true;
  }

  float    getTemp()   const { return lastTemp; }
  bool     isSimMode() const { return simMode;  }
};

#endif // DEV_DS18B20_H
