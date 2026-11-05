// deviceControl.cpp
#include "configControl.h"
#include "deviceControl.h"
#include "variables.h"

#include <Arduino.h>
#include <HardwareSerial.h>
#include <vector>
#include <algorithm>

HardwareSerial Ultrasonic_Sensor(2); 
volatile float Water_level;
float water_liters = 0.0;
bool water_threshold_exceeded = false;
float last_water_level_pub = 0.0;


// bool drain_active = false;
// bool irrigate_active = false;

int Config_Button = 27;


void buttonTask(void *pvParameters) {

  pinMode(Config_Button, INPUT_PULLUP);

  bool buttonPressed = false;
  bool actionDone = false;
  unsigned long pressStartTime = 0;

  for (;;) {

    bool currentState = (digitalRead(Config_Button) == LOW);

    if (currentState) {

      if (!buttonPressed) {
        buttonPressed = true;
        pressStartTime = millis();
        actionDone = false;
      }

      if (!actionDone && (millis() - pressStartTime >= 5000)) {
        Serial.println("Hello World");
        resetWifiCred();
        actionDone = true;
      }

    } else {
      buttonPressed = false;
      pressStartTime = 0;
    }

    vTaskDelay(pdMS_TO_TICKS(20)); // debounce + yield CPU
  }
}
