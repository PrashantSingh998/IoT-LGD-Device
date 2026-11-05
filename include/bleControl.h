#ifndef BLE_CONTROL_H
#define BLE_CONTROL_H

#include <Arduino.h>
#include <WebServer.h>


extern WebServer server;

// Function declarations
void handleNotFound();
void handleGetStatus();
void handlewifiConfig();
void initroutes();
void httploop();
void handlecalibrationConfig();

#endif // HTTP_ROUTES_H
