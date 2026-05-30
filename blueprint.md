# ESP32 LEVEL2 — Blueprint: โครงสร้าง Class

## Hardware Pin Assignment

### Switch (DevSwitch) — Active Low, External Pull-up 10kΩ

| ชื่อ | GPIO | โหมด | หมายเหตุ |
|------|------|------|----------|
| SW1 | 34 | Active Low | External pull-up 10kΩ, Input Only |
| SW2 | 35 | Active Low | External pull-up 10kΩ, Input Only |
| SW3 | 32 | Active Low | External pull-up 10kΩ |

> **หมายเหตุ:** GPIO34 และ GPIO35 เป็น Input Only ไม่มี internal pull-up/pull-down ในตัว ต้องใช้ external pull-up เสมอ
>
> การสร้าง object ให้ใช้ `activeHigh = false` (default):
> ```cpp
> DevSwitch sw1(34, false);  // Active Low
> DevSwitch sw2(35, false);
> DevSwitch sw3(32, false);
> ```

---

## ภาพรวม (Overview)

โปรเจกต์นี้เป็น ESP32 firmware ที่มี driver class สำหรับอุปกรณ์ฮาร์ดแวร์ต่างๆ โดยแยก class ออกเป็น 5 ไฟล์ใน `include/`

```
include/
├── DevIsoInput.h       — Isolated Digital Input (debounce + edge detection)
├── DevPZEM.h           — PZEM-016 AC Power Monitor (Modbus RTU)
├── DevRelay.h          — Relay Output + Timer variant
├── DevSwitch.h         — Push Button (debounce + edge detection + callback)
└── DevXYMDSensor.h     — XY-MD03 Temperature & Humidity Sensor (Modbus RTU)
```

---

## Class Diagram (ความสัมพันธ์)

```
DevRelay
    └── DevRelayWithTimer  (inherits DevRelay)

DevIsoInput       (standalone)
DevSwitch         (standalone)
DevPZEM           (standalone, uses ModbusMaster)
DevXYMDSensor     (standalone, uses ModbusMaster)
```

---

## 1. `DevIsoInput` — Isolated Digital Input

**ไฟล์:** [include/DevIsoInput.h](include/DevIsoInput.h)

จัดการการอ่าน Isolated Digital Input พร้อม debouncing และ event detection รองรับทั้ง Active High และ Active Low

### Protected Members

| ชื่อ | ประเภท | คำอธิบาย |
|------|--------|----------|
| `pin` | `uint8_t` | หมายเลข GPIO |
| `activeHigh` | `bool` | โหมด Active High/Low |
| `currentState` | `bool` | สถานะปัจจุบัน |
| `lastState` | `bool` | สถานะก่อนหน้า |
| `lastDebounceTime` | `unsigned long` | เวลาที่เริ่ม debounce ล่าสุด |
| `debounceDelay` | `unsigned long` | ระยะเวลา debounce (ms) |
| `lastStableState` | `bool` | สถานะเสถียรล่าสุด |
| `activatedEvent` | `bool` | flag: เพิ่ง active รอบนี้ |
| `deactivatedEvent` | `bool` | flag: เพิ่ง inactive รอบนี้ |
| `activationCount` | `unsigned long` | จำนวนครั้งที่ active สะสม |
| `lastActivationTime` | `unsigned long` | เวลาที่ active ล่าสุด |
| `onActiveCallback` | `void (*)()` | callback เมื่อ active |
| `onInactiveCallback` | `void (*)()` | callback เมื่อ inactive |

### Public Methods

| Method | Return | คำอธิบาย |
|--------|--------|----------|
| `DevIsoInput(pin, activeHigh, debounceMs)` | — | Constructor |
| `begin()` | `void` | ตั้งค่า pinMode, อ่านสถานะเริ่มต้น |
| `update()` | `bool` | อัปเดตสถานะ (เรียกใน loop) — คืน `true` ถ้ามีการเปลี่ยนแปลง |
| `readRawState()` | `bool` | อ่าน GPIO โดยตรง (ไม่ผ่าน debounce) |
| `isActive()` | `bool` | สถานะ active ปัจจุบัน |
| `isInactive()` | `bool` | สถานะ inactive ปัจจุบัน |
| `wasActivated()` | `bool` | rising edge (เพิ่ง active รอบนี้) |
| `wasDeactivated()` | `bool` | falling edge (เพิ่ง inactive รอบนี้) |
| `onActive(callback)` | `void` | ลงทะเบียน callback เมื่อ active |
| `onInactive(callback)` | `void` | ลงทะเบียน callback เมื่อ inactive |
| `getPin()` | `uint8_t` | คืน GPIO pin |
| `getActivationCount()` | `unsigned long` | จำนวนครั้งที่ active |
| `getLastActivationTime()` | `unsigned long` | เวลาที่ active ล่าสุด |
| `resetActivationCount()` | `void` | รีเซ็ต counter |
| `getDebounceDelay()` | `unsigned long` | อ่านค่า debounce delay |
| `setDebounceDelay(ms)` | `void` | ตั้งค่า debounce delay |
| `getStateText()` | `String` | คืน `"ACTIVE"` หรือ `"INACTIVE"` |

### หลักการทำงาน Debounce

```
Raw GPIO → เปลี่ยนแปลง → Reset timer
                        ↓
              คงที่เกิน debounceDelay?
                        ↓ Yes
              ต่างจาก lastStableState?
                        ↓ Yes
              อัปเดต currentState + ยิง event/callback
```

---

## 2. `DevPZEM` — PZEM-016 AC Power Monitor

**ไฟล์:** [include/DevPZEM.h](include/DevPZEM.h)

อ่านค่าพลังงานไฟฟ้า AC จาก PZEM-016 ผ่าน Modbus RTU บน HardwareSerial ใช้ MAX13487 (Auto Direction RS485) รองรับ **Simulation Mode** อัตโนมัติเมื่อไม่มี sensor จริง

### Private Members

| ชื่อ | ประเภท | คำอธิบาย |
|------|--------|----------|
| `node` | `ModbusMaster` | Modbus master object |
| `serial` | `HardwareSerial*` | Serial port |
| `slaveAddress` | `uint8_t` | Modbus slave address |
| `initialized` | `bool` | สถานะการ init |
| `dataValid` | `bool` | ข้อมูลถูกต้องหรือไม่ |
| `lastReadTime` | `unsigned long` | เวลาที่อ่านล่าสุด |
| `readInterval` | `const unsigned long` | ช่วงเวลาอ่าน (2000 ms) |
| `simulationMode` | `bool` | โหมดจำลองข้อมูล |
| `simulationStartTime` | `unsigned long` | เวลาเริ่ม simulation |
| `baseLoad` | `float` | กำลังไฟฟ้าพื้นฐาน (W) |
| `voltage` | `float` | แรงดัน (V) |
| `current` | `float` | กระแส (A) |
| `power` | `float` | กำลังไฟฟ้า (W) |
| `energy` | `float` | พลังงาน (Wh) |
| `frequency` | `float` | ความถี่ (Hz) |
| `powerFactor` | `float` | Power Factor (0–1) |
| `alarmStatus` | `uint16_t` | Alarm register |

### Public Methods

| Method | Return | คำอธิบาย |
|--------|--------|----------|
| `DevPZEM(serial, addr)` | — | Constructor |
| `begin()` | `bool` | Init Modbus, ทดสอบ connection 3 ครั้ง, fallback simulation |
| `update()` | `bool` | อ่านค่าทุก 2 วินาที (เรียกใน loop) |
| `resetEnergy()` | `bool` | รีเซ็ต energy counter บน PZEM |
| `isInitialized()` | `bool` | ตรวจสอบว่า init แล้ว |
| `isDataValid()` | `bool` | ตรวจสอบว่าข้อมูลถูกต้อง |
| `isSimulationMode()` | `bool` | ตรวจสอบโหมด simulation |
| `getSlaveAddress()` | `uint8_t` | Modbus slave address |
| `getVoltage()` | `float` | แรงดัน V |
| `getCurrent()` | `float` | กระแส A |
| `getPower()` | `float` | กำลังไฟฟ้า W |
| `getEnergy()` | `float` | พลังงาน **kWh** (แปลงจาก Wh) |
| `getFrequency()` | `float` | ความถี่ Hz |
| `getPowerFactor()` | `float` | Power Factor |
| `getAlarmStatus()` | `uint16_t` | Alarm register |
| `printData()` | `void` | พิมพ์ข้อมูลทั้งหมดลง Serial |
| `toJSON()` | `String` | คืน JSON string ของข้อมูลทั้งหมด |
| `setSimulationBaseLoad(watts)` | `void` | ตั้งค่า base load สำหรับ simulation |

### Modbus Register Map (PZEM-016)

| Register | ค่า | Scale | หน่วย |
|----------|-----|-------|-------|
| 0x0000 | Voltage | ×0.1 | V |
| 0x0001–0x0002 | Current (32-bit) | ×0.001 | A |
| 0x0003–0x0004 | Power (32-bit) | ×0.1 | W |
| 0x0005–0x0006 | Energy (32-bit) | ×1 | Wh |
| 0x0007 | Frequency | ×0.1 | Hz |
| 0x0008 | Power Factor | ×0.01 | — |
| 0x0009 | Alarm Status | — | — |
| 0x0042 | Reset Energy (Write) | — | — |

### Simulation Mode

เมื่อ `begin()` หา sensor จริงไม่พบ จะ fallback อัตโนมัติ:
- Voltage: 220V ± 3V (sine wave)
- Current: แปรตาม baseLoad + noise
- Power Factor: 0.85–0.95
- Energy: สะสมเพิ่มขึ้นตาม runtime

---

## 3. `DevRelay` — Relay Output

**ไฟล์:** [include/DevRelay.h](include/DevRelay.h)

ควบคุม Digital Output สำหรับ Relay รองรับ Active Low และ Active High

### Protected Members

| ชื่อ | ประเภท | คำอธิบาย |
|------|--------|----------|
| `pin` | `uint8_t` | GPIO pin |
| `state` | `bool` | สถานะปัจจุบัน (true = ON) |
| `activeLow` | `bool` | โหมด Active Low/High |

### Public Methods

| Method | Return | คำอธิบาย |
|--------|--------|----------|
| `DevRelay(pin, activeLow)` | — | Constructor (default: Active Low) |
| `begin()` | `void` | ตั้งค่า OUTPUT, เริ่มต้น OFF |
| `on()` | `void` | เปิด Relay |
| `off()` | `void` | ปิด Relay |
| `toggle()` | `void` | สลับสถานะ |
| `setState(bool)` | `void` | ตั้งสถานะโดยตรง |
| `getState()` | `bool` | อ่านสถานะปัจจุบัน |
| `getPin()` | `uint8_t` | อ่าน GPIO pin |
| `isActiveLow()` | `bool` | ตรวจสอบโหมด Active Low |

---

## 4. `DevRelayWithTimer` — Relay with Auto-Off Timer

**ไฟล์:** [include/DevRelay.h](include/DevRelay.h) (อยู่ในไฟล์เดียวกับ DevRelay)

**Inherits:** `DevRelay`

ขยาย DevRelay ให้มีความสามารถ Auto-Off หลังหมดเวลา

### Private Members (เพิ่มเติม)

| ชื่อ | ประเภท | คำอธิบาย |
|------|--------|----------|
| `timerDuration` | `unsigned long` | ระยะเวลา timer (ms) |
| `timerStart` | `unsigned long` | เวลาเริ่ม timer |
| `timerActive` | `bool` | สถานะ timer |

### Public Methods (เพิ่มเติม)

| Method | Return | คำอธิบาย |
|--------|--------|----------|
| `DevRelayWithTimer(pin, activeLow)` | — | Constructor |
| `onWithTimer(duration)` | `void` | เปิด Relay พร้อมตั้ง timer |
| `checkTimer()` | `bool` | ตรวจสอบ timer (เรียกใน loop) — คืน `true` ถ้าหมดเวลา |
| `cancelTimer()` | `void` | ยกเลิก timer |
| `isTimerActive()` | `bool` | ตรวจสอบว่า timer ทำงานอยู่ |
| `getRemainingTime()` | `unsigned long` | เวลาที่เหลือ (ms) |

---

## 5. `DevSwitch` — Push Button

**ไฟล์:** [include/DevSwitch.h](include/DevSwitch.h)

จัดการปุ่มกดพร้อม debouncing และ edge detection คล้าย `DevIsoInput` แต่ออกแบบมาสำหรับ push button โดยเฉพาะ มี callback เพิ่มคือ `onClick`

### Protected Members

| ชื่อ | ประเภท | คำอธิบาย |
|------|--------|----------|
| `pin` | `uint8_t` | GPIO pin |
| `activeHigh` | `bool` | โหมด Active High/Low |
| `currentState` | `bool` | สถานะปัจจุบัน (true = กด) |
| `lastState` | `bool` | สถานะก่อนหน้า |
| `lastDebounceTime` | `unsigned long` | เวลา debounce ล่าสุด |
| `debounceDelay` | `unsigned long` | ระยะเวลา debounce (ms) |
| `lastStableState` | `bool` | สถานะเสถียรล่าสุด |
| `pressedEvent` | `bool` | flag: เพิ่งกดรอบนี้ |
| `releasedEvent` | `bool` | flag: เพิ่งปล่อยรอบนี้ |
| `onPressCallback` | `void (*)()` | callback เมื่อกด |
| `onReleaseCallback` | `void (*)()` | callback เมื่อปล่อย |
| `onClickCallback` | `void (*)()` | callback เมื่อ click (กด+ปล่อย) |

### Public Methods

| Method | Return | คำอธิบาย |
|--------|--------|----------|
| `DevSwitch(pin, activeHigh, debounceMs)` | — | Constructor |
| `begin()` | `void` | ตั้งค่า pinMode, อ่านสถานะเริ่มต้น |
| `update()` | `bool` | อัปเดตสถานะ (เรียกใน loop) |
| `readRawState()` | `bool` | อ่าน GPIO โดยตรง |
| `isPressed()` | `bool` | กดอยู่หรือไม่ |
| `isReleased()` | `bool` | ปล่อยอยู่หรือไม่ |
| `wasPressed()` | `bool` | rising edge (เพิ่งกดรอบนี้) |
| `wasReleased()` | `bool` | falling edge (เพิ่งปล่อยรอบนี้) |
| `onPress(callback)` | `void` | ลงทะเบียน callback เมื่อกด |
| `onRelease(callback)` | `void` | ลงทะเบียน callback เมื่อปล่อย |
| `onClick(callback)` | `void` | ลงทะเบียน callback เมื่อ click |
| `getPin()` | `uint8_t` | อ่าน GPIO pin |
| `getDebounceDelay()` | `unsigned long` | อ่านค่า debounce delay |
| `setDebounceDelay(ms)` | `void` | ตั้งค่า debounce delay |

### ความต่างจาก DevIsoInput

| Feature | DevSwitch | DevIsoInput |
|---------|-----------|-------------|
| เหมาะสำหรับ | Push button | Isolated input (opto-coupler) |
| onClick callback | ✅ | ❌ |
| Activation counter | ❌ | ✅ |
| lastActivationTime | ❌ | ✅ |

---

## 6. `DevXYMDSensor` — Temperature & Humidity Sensor

**ไฟล์:** [include/DevXYMDSensor.h](include/DevXYMDSensor.h)

อ่านค่าอุณหภูมิและความชื้นจาก XY-MD03 ผ่าน Modbus RTU (Function Code 04) บน HardwareSerial รองรับ Auto Direction RS485

### Private Members

| ชื่อ | ประเภท | คำอธิบาย |
|------|--------|----------|
| `modbus` | `ModbusMaster` | Modbus master object |
| `slaveID` | `uint8_t` | Modbus Slave ID |
| `serial` | `HardwareSerial*` | Serial port |
| `temperature` | `float` | อุณหภูมิล่าสุด (°C) |
| `humidity` | `float` | ความชื้นล่าสุด (%) |
| `lastReadSuccess` | `bool` | ผลการอ่านล่าสุด |
| `lastReadTime` | `unsigned long` | เวลาที่อ่านล่าสุด |
| `REG_TEMPERATURE` | `const uint16_t` | 0x0001 |
| `REG_HUMIDITY` | `const uint16_t` | 0x0002 |

### Public Methods

| Method | Return | คำอธิบาย |
|--------|--------|----------|
| `DevXYMDSensor(hwSerial, slaveId)` | — | Constructor |
| `begin(baudRate)` | `void` | Init Serial + Modbus (default: 9600) |
| `update()` | `bool` | อ่าน 2 register พร้อมกัน (temp + humidity) |
| `getTemperature()` | `float` | อุณหภูมิ °C |
| `getHumidity()` | `float` | ความชื้น % |
| `isLastReadSuccess()` | `bool` | ผลการอ่านครั้งล่าสุด |
| `getLastReadTime()` | `unsigned long` | เวลาที่อ่านล่าสุด (ms) |
| `printInfo()` | `void` | พิมพ์ข้อมูลทั้งหมดลง Serial |
| `setSlaveID(newId)` | `void` | เปลี่ยน Slave ID |
| `getSlaveID()` | `uint8_t` | อ่าน Slave ID ปัจจุบัน |

### Modbus Register Map (XY-MD03)

| Register | ข้อมูล | Scale | หน่วย |
|----------|--------|-------|-------|
| 0x0001 | Temperature | ÷10 | °C |
| 0x0002 | Humidity | ÷10 | % |

---

## Pin Assignment

| อุปกรณ์ | GPIO | โหมด |
|---------|------|------|
| Relay1 | 17 | Active Low |
| Relay2 | 16 | Active Low |
| Relay3 | 4 | Active Low |

---

## สรุปโครงสร้างการใช้งาน (Usage Pattern)

class ทั้งหมดใช้ pattern เดียวกัน:

```cpp
// 1. สร้าง object
DevXxx device(pin, ...);

// 2. เรียก begin() ใน setup()
device.begin();

// 3. เรียก update() ใน loop() ทุก iteration
device.update();

// 4. อ่านค่า / ตรวจ event
if (device.wasActivated()) { ... }
float val = device.getValue();
```

| Class | ประเภท | โปรโตคอล | ต้องเรียก update() |
|-------|--------|-----------|-------------------|
| `DevIsoInput` | Input | GPIO | ✅ |
| `DevSwitch` | Input | GPIO | ✅ |
| `DevRelay` | Output | GPIO | ❌ |
| `DevRelayWithTimer` | Output | GPIO | ✅ (`checkTimer()`) |
| `DevPZEM` | Sensor | Modbus RTU | ✅ |
| `DevXYMDSensor` | Sensor | Modbus RTU | ✅ |
