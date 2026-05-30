#ifndef DEV_ISO_INPUT_H
#define DEV_ISO_INPUT_H

#include <Arduino.h>

/**
 * @class DevIsoInput
 * @brief คลาสสำหรับจัดการ Isolated Input พร้อม debouncing และ state tracking
 * @details รองรับ Active Low/High และตรวจจับการเปลี่ยนสถานะ
 */
class DevIsoInput {
protected:
  uint8_t pin;              // GPIO pin number
  bool activeHigh;          // โหมดการทำงาน (true = Active High, false = Active Low)
  bool currentState;        // สถานะปัจจุบัน (true = active, false = inactive)
  bool lastState;           // สถานะก่อนหน้า
  unsigned long lastDebounceTime;  // เวลาที่เกิดการเปลี่ยนสถานะล่าสุด
  unsigned long debounceDelay;     // ระยะเวลา debounce (milliseconds)
  bool lastStableState;     // สถานะที่เสถียรล่าสุด
  bool activatedEvent;      // true เฉพาะรอบ update() ที่เพิ่ง active
  bool deactivatedEvent;    // true เฉพาะรอบ update() ที่เพิ่ง inactive
  
  // Event counters
  unsigned long activationCount;   // จำนวนครั้งที่ active
  unsigned long lastActivationTime; // เวลาที่ active ล่าสุด
  
  // Callback functions
  void (*onActiveCallback)();
  void (*onInactiveCallback)();

public:
  /**
   * @brief Constructor
   * @param gpioPin หมายเลขขา GPIO ที่ต้องการอ่าน
   * @param activeHigh กำหนดโหมดการทำงาน (default: false สำหรับ Active Low)
   * @param debounceMs ระยะเวลา debounce ในหน่วย ms (default: 50ms)
   */
  DevIsoInput(uint8_t gpioPin, bool activeHigh = false, unsigned long debounceMs = 50) {
    pin = gpioPin;
    this->activeHigh = activeHigh;
    currentState = false;
    lastState = false;
    lastStableState = false;
    activatedEvent = false;
    deactivatedEvent = false;
    lastDebounceTime = 0;
    debounceDelay = debounceMs;
    activationCount = 0;
    lastActivationTime = 0;
    onActiveCallback = nullptr;
    onInactiveCallback = nullptr;
  }

  /**
   * @brief เริ่มต้นการทำงาน (ตั้งค่า pinMode)
   */
  virtual void begin() {
    if (activeHigh) {
      pinMode(pin, INPUT);  // Active High ใช้ PULLDOWN ภายนอก
    } else {
      pinMode(pin, INPUT_PULLUP);  // Active Low ใช้ PULLUP
    }
    // อ่านสถานะเริ่มต้น
    currentState = readRawState();
    lastState = currentState;
    lastStableState = currentState;
    activatedEvent = false;
    deactivatedEvent = false;
  }

  /**
   * @brief อ่านสถานะ raw จาก GPIO (ไม่ผ่าน debounce)
   * @return สถานะ raw (true/false ตาม Active Low/High)
   */
  bool readRawState() {
    bool reading = digitalRead(pin);
    return activeHigh ? reading : !reading;  // Invert ถ้าเป็น Active Low
  }

  /**
   * @brief อัปเดตสถานะ input (ต้องเรียกใน loop())
   * @return true ถ้ามีการเปลี่ยนสถานะ
   */
  virtual bool update() {
    bool reading = readRawState();
    bool stateChanged = false;
    activatedEvent = false;
    deactivatedEvent = false;

    // ตรวจสอบว่ามีการเปลี่ยนสถานะหรือไม่
    if (reading != lastState) {
      lastDebounceTime = millis();  // Reset debounce timer
    }

    // ถ้าสถานะคงที่เกิน debounceDelay
    if ((millis() - lastDebounceTime) > debounceDelay) {
      // ถ้าสถานะใหม่ต่างจากสถานะเสถียรล่าสุด
      if (reading != lastStableState) {
        bool previousStableState = lastStableState;
        lastStableState = reading;
        currentState = reading;
        stateChanged = true;
        activatedEvent = currentState && !previousStableState;
        deactivatedEvent = !currentState && previousStableState;

        // เรียก callbacks และอัปเดต counters
        if (currentState) {
          // Active
          activationCount++;
          lastActivationTime = millis();
          if (onActiveCallback) onActiveCallback();
        } else {
          // Inactive
          if (onInactiveCallback) onInactiveCallback();
        }
      }
    }

    lastState = reading;
    return stateChanged;
  }

  /**
   * @brief ตรวจสอบว่า input เป็น active หรือไม่
   * @return true = active, false = inactive
   */
  bool isActive() const {
    return currentState;
  }

  /**
   * @brief ตรวจสอบว่า input เป็น inactive หรือไม่
   * @return true = inactive, false = active
   */
  bool isInactive() const {
    return !currentState;
  }

  /**
   * @brief ตรวจสอบว่าเพิ่ง active (rising edge)
   * @return true ถ้าเพิ่ง active
   */
  bool wasActivated() const {
    return activatedEvent;
  }

  /**
   * @brief ตรวจสอบว่าเพิ่ง inactive (falling edge)
   * @return true ถ้าเพิ่ง inactive
   */
  bool wasDeactivated() const {
    return deactivatedEvent;
  }

  /**
   * @brief กำหนด callback เมื่อ active
   */
  void onActive(void (*callback)()) {
    onActiveCallback = callback;
  }

  /**
   * @brief กำหนด callback เมื่อ inactive
   */
  void onInactive(void (*callback)()) {
    onInactiveCallback = callback;
  }

  /**
   * @brief อ่านหมายเลขขา GPIO
   */
  uint8_t getPin() const {
    return pin;
  }

  /**
   * @brief อ่านจำนวนครั้งที่ active
   */
  unsigned long getActivationCount() const {
    return activationCount;
  }

  /**
   * @brief อ่านเวลาที่ active ล่าสุด (milliseconds)
   */
  unsigned long getLastActivationTime() const {
    return lastActivationTime;
  }

  /**
   * @brief รีเซ็ต activation counter
   */
  void resetActivationCount() {
    activationCount = 0;
  }

  /**
   * @brief อ่านค่า debounce delay
   */
  unsigned long getDebounceDelay() const {
    return debounceDelay;
  }

  /**
   * @brief ตั้งค่า debounce delay
   */
  void setDebounceDelay(unsigned long ms) {
    debounceDelay = ms;
  }

  /**
   * @brief รับสถานะเป็นข้อความ
   */
  String getStateText() const {
    return currentState ? "ACTIVE" : "INACTIVE";
  }
};

#endif // DEV_ISO_INPUT_H
