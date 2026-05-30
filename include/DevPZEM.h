/**
 * @file DevPZEM.h
 * @brief PZEM-016 AC Power Monitor wrapper class for ESP32 using ModbusMaster
 * @note Uses the same Serial port as programming (Serial/UART0)
 * @note Uses MAX13487 for RS232 to RS485 conversion (Auto Direction)
 * 
 * PZEM-016 Specifications:
 * - Voltage: 80-260V AC
 * - Current: 0-100A (with external CT)
 * - Power: 0-23kW
 * - Communication: Modbus RTU (9600 8N1)
 * - Slave Address: 0x01 (default)
 */

#ifndef DEVPZEM_H
#define DEVPZEM_H

#include <Arduino.h>
#include <ModbusMaster.h>

// PZEM-016 Default Configuration
#define PZEM_DEFAULT_ADDR 0x01  // Default Modbus slave address
// #define PZEM_SIMULATION_MODE    // Comment this line to disable simulation mode

// Callback functions for ModbusMaster (MAX13487 auto direction - no control needed)
void preTransmission() {
  // MAX13487 handles direction automatically - no action needed
  // Just ensure any pending data is sent
  Serial.flush();
}

void postTransmission() {
  // MAX13487 handles direction automatically - no action needed  
  // Small delay to ensure transmission is complete before listening
  delayMicroseconds(100);
}

class DevPZEM {
private:
  ModbusMaster node;
  HardwareSerial* serial;
  uint8_t slaveAddress;
  bool initialized;
  bool dataValid;
  unsigned long lastReadTime;
  const unsigned long readInterval = 2000;  // อ่านทุก 2 วินาที
  
  // Simulation mode variables
  bool simulationMode;
  unsigned long simulationStartTime;
  float baseLoad;  // Base load for simulation (watts)
  
  // ข้อมูลที่อ่านได้
  float voltage;      // แรงดัน (V)
  float current;      // กระแส (A)
  float power;        // กำลังไฟฟ้า (W)
  float energy;       // พลังงาน (Wh -> converted to kWh)
  float frequency;    // ความถี่ (Hz)
  float powerFactor;  // Power Factor (0-1)
  uint16_t alarmStatus; // Alarm status
  
  /**
   * @brief อ่านค่า 32-bit จาก 2 registers ต่อเนื่อง
   * @param startReg Register เริ่มต้น
   * @return ค่า 32-bit
   * @note PZEM-016 uses Low Word first, High Word second
   */
  uint32_t read32BitValue(uint16_t startReg) {
    uint8_t result = node.readInputRegisters(startReg, 2);
    if (result == node.ku8MBSuccess) {
      // PZEM-016: Register[0] = Low 16 bits, Register[1] = High 16 bits
      uint32_t value = node.getResponseBuffer(0) | ((uint32_t)node.getResponseBuffer(1) << 16);
      return value;
    }
    return 0;
  }

  /**
   * @brief สร้างข้อมูลจำลองที่ดูเหมือนจริง (Realistic simulation)
   * @note ใช้เมื่อไม่มี sensor จริงเชื่อมต่อ
   */
  void generateSimulatedData() {
    unsigned long runtime = (millis() - simulationStartTime) / 1000;  // วินาที
    
    // สร้างค่าแรงดันที่ค่อนข้างคงที่ (220V ± 3V)
    voltage = 220.0 + sin(runtime * 0.1) * 3.0;
    
    // สร้างค่ากระแสที่แปรผันตามเวลา (ดูเหมือนการใช้งานจริง)
    // Base load + random variation + sine wave pattern
    float loadVariation = sin(runtime * 0.05) * 0.3;  // ±0.3A
    float randomNoise = (random(0, 100) - 50) / 1000.0;  // ±0.05A
    current = (baseLoad / 220.0) + loadVariation + randomNoise;
    if (current < 0.1) current = 0.1;  // ต่ำสุด 0.1A
    
    // สร้างค่า Power Factor ที่สมจริง (0.85-0.95)
    powerFactor = 0.90 + sin(runtime * 0.03) * 0.05;
    
    // คำนวณกำลังไฟฟ้า P = V × I × PF
    power = voltage * current * powerFactor;
    
    // สร้างค่าพลังงานสะสม (เพิ่มขึ้นเรื่อยๆ)
    energy += (power * (readInterval / 1000.0) / 3600.0);  // Wh
    
    // ความถี่ค่อนข้างคงที่ (50Hz ± 0.3Hz)
    frequency = 50.0 + sin(runtime * 0.08) * 0.3;
    
    // ไม่มี Alarm
    alarmStatus = 0;
    
    dataValid = true;
  }

public:
  /**
   * @brief Constructor - สร้าง PZEM object โดยใช้ ModbusMaster
   * @param serial ตัวชี้ไปยัง HardwareSerial object (default: &Serial)
   * @param addr Modbus slave address ของ PZEM (default: PZEM_DEFAULT_ADDR)
   */
  DevPZEM(HardwareSerial* serial = &Serial, uint8_t addr = PZEM_DEFAULT_ADDR) {
    this->serial = serial;
    this->slaveAddress = addr;
    initialized = false;
    dataValid = false;
    lastReadTime = 0;
    voltage = 0.0;
    current = 0.0;
    power = 0.0;
    energy = 0.0;
    frequency = 0.0;
    powerFactor = 0.0;
    alarmStatus = 0;
    simulationMode = false;
    simulationStartTime = 0;
    baseLoad = 500.0;  // Default 500W base load
  }

  /**
   * @brief Initialize PZEM sensor
   * @return true ถ้าสำเร็จ, false ถ้าล้มเหลว
   */
  bool begin() {
    // Try to connect to real hardware first
    simulationMode = false;
    
    // Clear serial buffer
    while (serial->available()) {
      serial->read();
    }
    
    // กำหนด Modbus slave address
    node.begin(slaveAddress, *serial);
    
    // Set callbacks for MAX13487 (auto direction - just flush/delay)
    node.preTransmission(preTransmission);
    node.postTransmission(postTransmission);
    
    delay(200);  // Wait for serial and MAX13487 to stabilize
    
    // ทดสอบการเชื่อมต่อโดยพยายามอ่านแรงดัน (retry 3 times)
    for (int attempt = 0; attempt < 3; attempt++) {
      uint8_t result = node.readInputRegisters(0x0000, 1);
      
      if (result == node.ku8MBSuccess) {
        uint16_t rawVoltage = node.getResponseBuffer(0);
        if (rawVoltage > 0 && rawVoltage < 3000) {  // ช่วง 0-300V (raw: 0-3000)
          initialized = true;
          dataValid = false;  // ยังไม่ได้อ่านครบทุกค่า
          simulationMode = false;
          Serial.printf("PZEM-016: Initialized successfully (Raw V=%d)\n", rawVoltage);
          Serial.println("  Using MAX13487 Auto Direction Mode");
          Serial.println("  *** REAL SENSOR MODE ***");
          return true;
        }
      }
      
      Serial.printf("  Attempt %d failed (0x%02X), retrying...\n", attempt + 1, result);
      delay(100);
    }
    
    // ถ้าเชื่อมต่อ sensor ไม่ได้ -> เปลี่ยนเป็น Simulation Mode อัตโนมัติ
    Serial.println("PZEM-016: No response from sensor after 3 attempts");
    Serial.println("  *** AUTO FALLBACK TO SIMULATION MODE ***");
    Serial.println("  Using simulated data instead");
    
    simulationMode = true;
    initialized = true;
    dataValid = false;
    simulationStartTime = millis();
    energy = 0.0;  // เริ่มต้น energy counter
    
    return true;  // Return true เพราะจะใช้ simulation mode แทน
  }

  /**
   * @brief อ่านค่าทั้งหมดจาก PZEM
   * @return true ถ้าอ่านสำเร็จ, false ถ้าล้มเหลว
   */
  bool update() {
    if (!initialized) {
      return false;
    }

    // ตรวจสอบว่าถึงเวลาอ่านหรือยัง
    unsigned long currentTime = millis();
    if (currentTime - lastReadTime < readInterval) {
      return dataValid;  // ยังไม่ถึงเวลาอ่าน ใช้ค่าเดิม
    }
    lastReadTime = currentTime;

    // Simulation Mode: สร้างข้อมูลจำลอง
    if (simulationMode) {
      generateSimulatedData();
      return true;
    }

    // Clear any stale data in serial buffer
    while (serial->available()) {
      serial->read();
    }

    bool readSuccess = true;
    
    // อ่าน Voltage (Register 0x0000, 1 register)
    uint8_t result = node.readInputRegisters(0x0000, 1);
    if (result == node.ku8MBSuccess) {
      voltage = node.getResponseBuffer(0) * 0.1;  // Scale: 0.1
    } else {
      readSuccess = false;
      Serial.printf("PZEM: Failed to read voltage (0x%02X)\n", result);
    }
    
    delay(100);  // Delay between requests (MAX13487 needs time to switch direction)
    
    // อ่าน Current (Register 0x0001, 2 registers, 32-bit)
    uint32_t rawCurrent = read32BitValue(0x0001);
    current = rawCurrent * 0.001;  // Scale: 0.001 A
    
    delay(100);
    
    // อ่าน Power (Register 0x0003, 2 registers, 32-bit)
    uint32_t rawPower = read32BitValue(0x0003);
    power = rawPower * 0.1;  // Scale: 0.1
    
    delay(100);
    
    // อ่าน Energy (Register 0x0005, 2 registers, 32-bit)
    uint32_t rawEnergy = read32BitValue(0x0005);
    energy = rawEnergy;  // Wh (will convert to kWh when needed)
    
    delay(100);
    
    // อ่าน Frequency (Register 0x0007, 1 register)
    result = node.readInputRegisters(0x0007, 1);
    if (result == node.ku8MBSuccess) {
      frequency = node.getResponseBuffer(0) * 0.1;  // Scale: 0.1
    }
    
    delay(100);
    
    // อ่าน Power Factor (Register 0x0008, 1 register)
    result = node.readInputRegisters(0x0008, 1);
    if (result == node.ku8MBSuccess) {
      powerFactor = node.getResponseBuffer(0) * 0.01;  // Scale: 0.01
    }
    
    delay(100);
    
    // อ่าน Alarm Status (Register 0x0009, 1 register)
    result = node.readInputRegisters(0x0009, 1);
    if (result == node.ku8MBSuccess) {
      alarmStatus = node.getResponseBuffer(0);
    }

    // ตรวจสอบความถูกต้องของข้อมูล
    if (!readSuccess || voltage < 0 || voltage > 300) {
      dataValid = false;
      Serial.println("PZEM: Data validation failed");
      return false;
    }

    dataValid = true;
    return true;
  }

  /**
   * @brief รีเซ็ตค่า Energy counter
   * @return true ถ้าสำเร็จ, false ถ้าล้มเหลว
   */
  bool resetEnergy() {
    if (!initialized) {
      return false;
    }
    
    // PZEM-016 Reset Command: Write to register 0x0042
    // Using function code 0x06 (Write Single Register)
    uint8_t result = node.writeSingleRegister(0x0042, 0x0000);
    
    if (result == node.ku8MBSuccess) {
      energy = 0.0;
      Serial.println("PZEM: Energy counter reset successfully");
      return true;
    }
    
    Serial.printf("PZEM: Failed to reset energy (0x%02X)\n", result);
    return false;
  }

  // Getter methods
  bool isInitialized() const { return initialized; }
  bool isDataValid() const { return dataValid; }
  bool isSimulationMode() const { return simulationMode; }
  uint8_t getSlaveAddress() const { return slaveAddress; }
  float getVoltage() const { return dataValid ? voltage : 0.0; }
  float getCurrent() const { return dataValid ? current : 0.0; }
  float getPower() const { return dataValid ? power : 0.0; }
  float getEnergy() const { return dataValid ? (energy / 1000.0) : 0.0; }  // Convert Wh to kWh
  float getFrequency() const { return dataValid ? frequency : 0.0; }
  float getPowerFactor() const { return dataValid ? powerFactor : 0.0; }
  uint16_t getAlarmStatus() const { return dataValid ? alarmStatus : 0; }

  /**
   * @brief แสดงข้อมูลทั้งหมดใน Serial Monitor
   */
  void printData() {
    if (!dataValid) {
      Serial.println("PZEM: No valid data");
      return;
    }

    Serial.println("========== PZEM-016 Data ==========");
    if (simulationMode) {
      Serial.println("[SIMULATION MODE]");
    }
    Serial.printf("Voltage:      %.2f V\n", voltage);
    Serial.printf("Current:      %.3f A\n", current);
    Serial.printf("Power:        %.2f W\n", power);
    Serial.printf("Energy:       %.3f kWh\n", getEnergy());
    Serial.printf("Frequency:    %.1f Hz\n", frequency);
    Serial.printf("Power Factor: %.2f\n", powerFactor);
    Serial.printf("Alarm Status: 0x%04X\n", alarmStatus);
    Serial.println("===================================");
  }

  /**
   * @brief สร้าง JSON string สำหรับส่งข้อมูล
   * @return JSON string
   */
  String toJSON() {
    String json = "{";
    json += "\"slaveId\":" + String(slaveAddress) + ",";
    json += "\"simulation\":" + String(simulationMode ? "true" : "false") + ",";
    json += "\"voltage\":" + String(voltage, 2) + ",";
    json += "\"current\":" + String(current, 3) + ",";
    json += "\"power\":" + String(power, 2) + ",";
    json += "\"energy\":" + String(getEnergy(), 3) + ",";
    json += "\"frequency\":" + String(frequency, 1) + ",";
    json += "\"powerFactor\":" + String(powerFactor, 2) + ",";
    json += "\"alarmStatus\":" + String(alarmStatus) + ",";
    json += "\"valid\":" + String(dataValid ? "true" : "false");
    json += "}";
    return json;
  }

  /**
   * @brief ตั้งค่า Base Load สำหรับ Simulation Mode
   * @param watts กำลังไฟฟ้าพื้นฐาน (Watts)
   */
  void setSimulationBaseLoad(float watts) {
    baseLoad = watts;
  }
};

#endif // DEVPZEM_H
