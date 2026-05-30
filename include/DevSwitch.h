#ifndef DEV_SWITCH_H
#define DEV_SWITCH_H

#include <Arduino.h>

/**
 * @class DevSwitch
 * @brief คลาสสำหรับจัดการปุ่มกดพร้อม debouncing และ edge detection
 * @details รองรับ Active Low/High, callback functions, และตรวจจับการกด/ปล่อย
 */
class DevSwitch {
protected:
  uint8_t pin;              // GPIO pin number
  bool activeHigh;          // โหมดการทำงาน (true = Active High, false = Active Low)
  bool currentState;        // สถานะปัจจุบัน (true = กด, false = ปล่อย)
  bool lastState;           // สถานะก่อนหน้า
  unsigned long lastDebounceTime;  // เวลาที่เกิดการเปลี่ยนสถานะล่าสุด
  unsigned long debounceDelay;     // ระยะเวลา debounce (milliseconds)
  bool lastStableState;     // สถานะที่เสถียรล่าสุด
  bool pressedEvent;        // true เฉพาะรอบ update() ที่เพิ่งกด
  bool releasedEvent;       // true เฉพาะรอบ update() ที่เพิ่งปล่อย
  
  // Callback functions
  void (*onPressCallback)();
  void (*onReleaseCallback)();
  void (*onClickCallback)();

public:
  /**
   * @brief Constructor
   * @param gpioPin หมายเลขขา GPIO ที่ต้องการอ่าน
   * @param activeHigh กำหนดโหมดการทำงาน (default: false สำหรับ Active Low)
   * @param debounceMs ระยะเวลา debounce ในหน่วย ms (default: 50ms)
   */
  DevSwitch(uint8_t gpioPin, bool activeHigh = false, unsigned long debounceMs = 50) {
    pin = gpioPin;
    this->activeHigh = activeHigh;
    currentState = false;
    lastState = false;
    lastStableState = false;
    pressedEvent = false;
    releasedEvent = false;
    lastDebounceTime = 0;
    debounceDelay = debounceMs;
    onPressCallback = nullptr;
    onReleaseCallback = nullptr;
    onClickCallback = nullptr;
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
    pressedEvent = false;
    releasedEvent = false;
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
   * @brief อัปเดตสถานะปุ่ม (ต้องเรียกใน loop())
   * @return true ถ้ามีการเปลี่ยนสถานะ
   */
  virtual bool update() {
    bool reading = readRawState();
    bool stateChanged = false;
    pressedEvent = false;
    releasedEvent = false;

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
        pressedEvent = currentState && !previousStableState;
        releasedEvent = !currentState && previousStableState;

        // เรียก callbacks
        if (currentState) {
          // กดปุ่ม (Pressed)
          if (onPressCallback) onPressCallback();
        } else {
          // ปล่อยปุ่ม (Released)
          if (onReleaseCallback) onReleaseCallback();
          if (onClickCallback) onClickCallback();  // Click = Press + Release
        }
      }
    }

    lastState = reading;
    return stateChanged;
  }

  /**
   * @brief ตรวจสอบว่าปุ่มถูกกดอยู่หรือไม่
   * @return true = กดอยู่, false = ปล่อยอยู่
   */
  bool isPressed() const {
    return currentState;
  }

  /**
   * @brief ตรวจสอบว่าปุ่มถูกปล่อยอยู่หรือไม่
   * @return true = ปล่อยอยู่, false = กดอยู่
   */
  bool isReleased() const {
    return !currentState;
  }

  /**
   * @brief ตรวจสอบว่าเพิ่งกดปุ่ม (rising edge)
   * @return true ถ้าเพิ่งกด
   */
  bool wasPressed() const {
    return pressedEvent;
  }

  /**
   * @brief ตรวจสอบว่าเพิ่งปล่อยปุ่ม (falling edge)
   * @return true ถ้าเพิ่งปล่อย
   */
  bool wasReleased() const {
    return releasedEvent;
  }

  /**
   * @brief กำหนด callback เมื่อกดปุ่ม
   */
  void onPress(void (*callback)()) {
    onPressCallback = callback;
  }

  /**
   * @brief กำหนด callback เมื่อปล่อยปุ่ม
   */
  void onRelease(void (*callback)()) {
    onReleaseCallback = callback;
  }

  /**
   * @brief กำหนด callback เมื่อคลิก (กด + ปล่อย)
   */
  void onClick(void (*callback)()) {
    onClickCallback = callback;
  }

  /**
   * @brief อ่านหมายเลขขา GPIO
   */
  uint8_t getPin() const {
    return pin;
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
};

#endif // DEV_SWITCH_H
