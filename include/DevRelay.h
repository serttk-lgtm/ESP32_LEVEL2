#ifndef DEV_RELAY_H
#define DEV_RELAY_H

#include <Arduino.h>

/**
 * @class DevRelay
 * @brief คลาสสำหรับควบคุม Relay ที่เป็น Active Low
 * @details Relay จะทำงานแบบ Active Low (HIGH = OFF, LOW = ON)
 */
class DevRelay {
protected:
  uint8_t pin;          // GPIO pin number
  bool state;           // สถานะปัจจุบัน (true = ON, false = OFF)
  bool activeLow;       // โหมดการทำงาน (true = Active Low, false = Active High)

public:
  /**
   * @brief Constructor
   * @param gpioPin หมายเลขขา GPIO ที่ต้องการควบคุม
   * @param activeLow กำหนดโหมดการทำงาน (default: true สำหรับ Active Low)
   */
  DevRelay(uint8_t gpioPin, bool activeLow = true) {
    pin = gpioPin;
    state = false;
    this->activeLow = activeLow;
  }

  /**
   * @brief เริ่มต้นการทำงาน (ตั้งค่า pinMode และสถานะเริ่มต้น)
   */
  virtual void begin() {
    pinMode(pin, OUTPUT);
    off(); // เริ่มต้นด้วยสถานะปิด
  }

  /**
   * @brief เปิด Relay
   */
  virtual void on() {
    state = true;
    digitalWrite(pin, activeLow ? LOW : HIGH);
  }

  /**
   * @brief ปิด Relay
   */
  virtual void off() {
    state = false;
    digitalWrite(pin, activeLow ? HIGH : LOW);
  }

  /**
   * @brief สลับสถานะ Relay (เปิด <-> ปิด)
   */
  virtual void toggle() {
    if (state) {
      off();
    } else {
      on();
    }
  }

  /**
   * @brief ตั้งค่าสถานะ Relay
   * @param newState สถานะที่ต้องการ (true = ON, false = OFF)
   */
  virtual void setState(bool newState) {
    if (newState) {
      on();
    } else {
      off();
    }
  }

  /**
   * @brief อ่านสถานะปัจจุบันของ Relay
   * @return true = ON, false = OFF
   */
  virtual bool getState() const {
    return state;
  }

  /**
   * @brief อ่านหมายเลขขา GPIO
   * @return หมายเลขขา GPIO
   */
  uint8_t getPin() const {
    return pin;
  }

  /**
   * @brief ตรวจสอบว่าเป็น Active Low หรือไม่
   * @return true = Active Low, false = Active High
   */
  bool isActiveLow() const {
    return activeLow;
  }
};

/**
 * @class DevRelayWithTimer
 * @brief คลาสขยายจาก DevRelay พร้อมฟังก์ชัน Timer
 */
class DevRelayWithTimer : public DevRelay {
private:
  unsigned long timerDuration;  // ระยะเวลา Timer (milliseconds)
  unsigned long timerStart;     // เวลาเริ่มต้น Timer
  bool timerActive;             // สถานะ Timer

public:
  /**
   * @brief Constructor
   * @param gpioPin หมายเลขขา GPIO
   * @param activeLow โหมดการทำงาน (default: true)
   */
  DevRelayWithTimer(uint8_t gpioPin, bool activeLow = true) 
    : DevRelay(gpioPin, activeLow) {
    timerDuration = 0;
    timerStart = 0;
    timerActive = false;
  }

  /**
   * @brief เปิด Relay พร้อมตั้ง Timer
   * @param duration ระยะเวลาในหน่วย milliseconds
   */
  void onWithTimer(unsigned long duration) {
    on();
    timerDuration = duration;
    timerStart = millis();
    timerActive = true;
  }

  /**
   * @brief ตรวจสอบและปิด Relay เมื่อหมดเวลา (ต้องเรียกใน loop)
   * @return true ถ้า Timer หมดเวลาและปิด Relay แล้ว
   */
  bool checkTimer() {
    if (timerActive && state) {
      if (millis() - timerStart >= timerDuration) {
        off();
        timerActive = false;
        return true;
      }
    }
    return false;
  }

  /**
   * @brief ยกเลิก Timer
   */
  void cancelTimer() {
    timerActive = false;
  }

  /**
   * @brief ตรวจสอบว่า Timer กำลังทำงานอยู่หรือไม่
   */
  bool isTimerActive() const {
    return timerActive;
  }

  /**
   * @brief อ่านเวลาที่เหลือของ Timer (milliseconds)
   */
  unsigned long getRemainingTime() const {
    if (!timerActive) return 0;
    unsigned long elapsed = millis() - timerStart;
    if (elapsed >= timerDuration) return 0;
    return timerDuration - elapsed;
  }
};

#endif // DEV_RELAY_H
