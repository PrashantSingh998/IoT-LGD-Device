#ifndef DEVICE_CONTROL_H
#define DEVICE_CONTROL_H

#include <Arduino.h>
#include <HardwareSerial.h>
extern HardwareSerial Ultrasonic_Sensor;
extern volatile float Water_level;
extern float water_liters;
extern bool water_threshold_exceeded;
extern float last_water_level_pub;
extern bool drain_active;
extern bool irrigate_active;

void initSensors ();                // no JSON: fixed UART pins/baud
float averageDistanceCm();                   // returns NaN on failure
float calculateVolumeLiters(float distance_cm);

void  readUltrasonicSensor(void *pvParameters);               // reads one frame from sensor, updates Water_level
void buttonTask(void *pvParameters);

// runtime setter used by NVS load and MQTT update_tank
// void updateTankParams(float tank_volume_l, float loft_height_cm, float blind_spot_cm);

#endif
