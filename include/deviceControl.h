#ifndef DEVICE_CONTROL_H
#define DEVICE_CONTROL_H

#include <Arduino.h>

void initSensors ();
bool initUltrasonicSensor();                 // no JSON: fixed UART pins/baud
float averageDistanceCm();                   // returns NaN on failure
float calculateVolumeLiters(float distance_cm);

// runtime setter used by NVS load and MQTT update_tank
void updateTankParams(float tank_volume_l, float loft_height_cm, float blind_spot_cm);

#endif
