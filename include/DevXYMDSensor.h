#ifndef DEV_XYMD_SENSOR_H
#define DEV_XYMD_SENSOR_H

#include <Arduino.h>
#include <ModbusMaster.h>

class DevXYMDSensor {
private:
  ModbusMaster      modbus;
  uint8_t           slaveID;
  HardwareSerial*   serial;
  unsigned long     baudRate;

  float             temperature;
  float             humidity;
  bool              lastReadSuccess;
  unsigned long     lastReadTime;
  unsigned long     readInterval;

  bool              simMode;
  unsigned long     simStart;

  // จำนวน fail ติดกัน ก่อน fallback sim
  static const uint8_t MAX_FAIL = 5;
  uint8_t failCount;

  static const uint16_t REG_TEMPERATURE = 0x0001;
  static const uint16_t REG_HUMIDITY    = 0x0002;

  void _generateSim() {
    float t = (millis() - simStart) / 1000.0f;
    temperature = 28.0f + 4.0f * sinf(t * 0.04f);
    humidity    = 65.0f + 15.0f * sinf(t * 0.03f + 1.0f);
  }

  // อ่านค่าจริง — คืน true ถ้าสำเร็จ
  bool _readSensor() {
    // flush buffer ค้างก่อนส่ง request
    while (serial->available()) serial->read();

    uint8_t r = modbus.readInputRegisters(REG_TEMPERATURE, 2);
    if (r == modbus.ku8MBSuccess) {
      float t = modbus.getResponseBuffer(0) / 10.0f;
      float h = modbus.getResponseBuffer(1) / 10.0f;
      // sanity check ค่าที่สมเหตุสมผล
      if (t > -40 && t < 80 && h >= 0 && h <= 100) {
        temperature = t;
        humidity    = h;
        return true;
      }
    }
    return false;
  }

public:
  DevXYMDSensor(HardwareSerial* hwSerial, uint8_t slaveId = 1,
                unsigned long intervalMs = 3000)
    : serial(hwSerial), slaveID(slaveId), baudRate(9600),
      temperature(0), humidity(0),
      lastReadSuccess(false), lastReadTime(0),
      readInterval(intervalMs),
      simMode(false), simStart(0), failCount(0) {}

  // คืน true = sensor จริง, false = simulation mode
  bool begin(unsigned long baud = 9600) {
    baudRate = baud;

    // reinit Serial0 เป็น Modbus baud rate
    // *** ไม่ใช้ Serial.print หลังจากนี้ เพราะ Serial0 เปลี่ยน baud แล้ว ***
    serial->begin(baudRate, SERIAL_8N1);
    serial->flush();
    modbus.begin(slaveID, *serial);

    // รอให้ bus และ slave stabilize
    delay(500);

    // ทดสอบ 5 attempts พร้อม delay ระหว่างแต่ละครั้ง
    for (int i = 0; i < 5; i++) {
      while (serial->available()) serial->read();  // flush
      if (_readSensor()) {
        simMode   = false;
        failCount = 0;
        return true;
      }
      delay(200);
    }

    // ไม่พบ sensor — เข้า sim mode
    simMode  = true;
    simStart = millis();
    _generateSim();
    return false;
  }

  // เรียกใน loop() — อ่านตาม interval
  // คืน true ถ้ามีค่าใหม่
  bool update() {
    if (millis() - lastReadTime < readInterval) return false;
    lastReadTime = millis();

    if (simMode) {
      _generateSim();
      lastReadSuccess = true;
      return true;
    }

    if (_readSensor()) {
      lastReadSuccess = true;
      failCount       = 0;
      return true;
    }

    // อ่านล้มเหลว
    lastReadSuccess = false;
    failCount++;

    if (failCount >= MAX_FAIL) {
      // fallback sim หลัง fail ติดกัน MAX_FAIL ครั้ง
      simMode  = true;
      simStart = millis();
      _generateSim();
    }
    return true;
  }

  // ลอง reconnect กลับ sensor จริง (เรียกจากภายนอกเมื่อต้องการ retry)
  bool reconnect() {
    simMode   = false;
    failCount = 0;
    return begin(baudRate);
  }

  // สแกนหา Slave ID 1-10 — คืน ID ที่ตอบสนอง, 0 = ไม่พบ
  // เรียกหลัง serial->begin() แล้วเท่านั้น
  uint8_t scanSlaveID(uint8_t maxID = 10) {
    for (uint8_t id = 1; id <= maxID; id++) {
      modbus.begin(id, *serial);
      delay(50);
      while (serial->available()) serial->read();
      uint8_t r = modbus.readInputRegisters(REG_TEMPERATURE, 2);
      if (r == modbus.ku8MBSuccess) {
        // พบ — restore slave ID เดิม
        slaveID = id;
        modbus.begin(slaveID, *serial);
        return id;
      }
      delay(100);
    }
    modbus.begin(slaveID, *serial);  // restore
    return 0;
  }

  float         getTemperature()    const { return temperature; }
  float         getHumidity()       const { return humidity; }
  bool          isLastReadSuccess() const { return lastReadSuccess; }
  bool          isSimMode()         const { return simMode; }
  uint8_t       getSlaveID()        const { return slaveID; }
  uint8_t       getFailCount()      const { return failCount; }
  unsigned long getLastReadTime()   const { return lastReadTime; }
};

#endif // DEV_XYMD_SENSOR_H
