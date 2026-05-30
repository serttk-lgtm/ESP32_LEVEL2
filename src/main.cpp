#include <Arduino.h>

#include "DevIsoInput.h"
#include "DevSwitch.h"
#include "DevRelay.h"
#include "DevPZEM.h"
#include "DevXYMDSensor.h"

// --- Switch: Active Low, External Pull-up 10kΩ (ดู blueprint.md) ---
DevSwitch sw1(34, false);
DevSwitch sw2(35, false);
DevSwitch sw3(32, false);

// --- Relay: Active Low (ดู blueprint.md) ---
DevRelay relay1(17, true);
DevRelay relay2(16, true);
DevRelay relay3(4,  true);

void setup() {
  Serial.begin(115200);

  sw1.begin();
  sw2.begin();
  sw3.begin();

  relay1.begin();
  relay2.begin();
  relay3.begin();

  Serial.println("Ready — SW1/SW2/SW3 toggles Relay1/2/3");
}

void loop() {
  sw1.update();
  sw2.update();
  sw3.update();

  if (sw1.wasPressed()) {
    relay1.toggle();
    Serial.printf("Relay1 = %s\n", relay1.getState() ? "ON" : "OFF");
  }

  if (sw2.wasPressed()) {
    relay2.toggle();
    Serial.printf("Relay2 = %s\n", relay2.getState() ? "ON" : "OFF");
  }

  if (sw3.wasPressed()) {
    relay3.toggle();
    Serial.printf("Relay3 = %s\n", relay3.getState() ? "ON" : "OFF");
  }
}
