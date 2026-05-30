#ifndef DEV_XYMD_SENSOR_H
#define DEV_XYMD_SENSOR_H

#include <Arduino.h>
#include <ModbusMaster.h>

class DevXYMDSensor {
private:
  ModbusMaster      modbus;
  uint8_t           slaveID;
  HardwareSerial*   serial;

  float             temperature;
  float             humidity;
  bool              lastReadSuccess;
  unsigned long     lastReadTime;
  unsigned long     readInterval;   // ms ระหว่างการอ่าน

  bool              simMode;
  unsigned long     simStart;

  static const uint16_t REG_TEMPERATURE = 0x0001;
  static const uint16_t REG_HUMIDITY    = 0x0002;

  // จำลองค่าที่สมจริง — แกว่งตาม sine wave
  void _generateSim() {
    float t = (millis() - simStart) / 1000.0f;
    temperature = 28.0f + 4.0f * sinf(t * 0.04f);           // 24–32°C
    humidity    = 65.0f + 15.0f * sinf(t * 0.03f + 1.0f);   // 50–80%
  }

public:
  DevXYMDSensor(HardwareSerial* hwSerial, uint8_t slaveId = 1,
                unsigned long intervalMs = 3000)
    : serial(hwSerial), slaveID(slaveId),
      temperature(0), humidity(0),
      lastReadSuccess(false), lastReadTime(0),
      readInterval(intervalMs),
      simMode(false), simStart(0) {}

  // คืน true = sensor จริง, false = simulation mode
  bool begin(unsigned long baudRate = 9600) {
    // Serial0 ถูก begin() ใน Serial.begin(115200) แล้ว
    // begin() ใหม่ด้วย baud 9600 สำหรับ Modbus
    serial->begin(baudRate, SERIAL_8N1);
    modbus.begin(slaveID, *serial);

    // ทดสอบอ่านครั้งแรก 3 attempts
    for (int i = 0; i < 3; i++) {
      delay(100);
      uint8_t r = modbus.readInputRegisters(REG_TEMPERATURE, 2);
      if (r == modbus.ku8MBSuccess) {
        simMode = false;
        Serial.printf("[XYMD] SlaveID=%d OK (real sensor)\n", slaveID);
        return true;
      }
      Serial.printf("[XYMD] Attempt %d failed (0x%02X)\n", i + 1, r);
    }

    // fallback simulation
    simMode  = true;
    simStart = millis();
    _generateSim();
    Serial.printf("[XYMD] SlaveID=%d → Simulation mode\n", slaveID);
    return false;
  }

  // เรียกใน loop() — อ่านตาม interval, คืน true ถ้ามีค่าใหม่
  bool update() {
    if (millis() - lastReadTime < readInterval) return false;
    lastReadTime = millis();

    if (simMode) {
      _generateSim();
      lastReadSuccess = true;
      return true;
    }

    uint8_t r = modbus.readInputRegisters(REG_TEMPERATURE, 2);
    if (r == modbus.ku8MBSuccess) {
      temperature     = modbus.getResponseBuffer(0) / 10.0f;
      humidity        = modbus.getResponseBuffer(1) / 10.0f;
      lastReadSuccess = true;
    } else {
      lastReadSuccess = false;
      Serial.printf("[XYMD] Read failed 0x%02X — switching to sim\n", r);
      simMode  = true;
      simStart = millis();
      _generateSim();
    }
    return true;
  }

  float    getTemperature()    const { return temperature; }
  float    getHumidity()       const { return humidity; }
  bool     isLastReadSuccess() const { return lastReadSuccess; }
  bool     isSimMode()         const { return simMode; }
  uint8_t  getSlaveID()        const { return slaveID; }
  unsigned long getLastReadTime() const { return lastReadTime; }
};

#endif // DEV_XYMD_SENSOR_H
