#ifndef BLE_CONTROL_H
#define BLE_CONTROL_H

#include <Arduino.h>
#include <WebServer.h>

// Function declarations
void turnONBLE();
void setupHttpRoutes(WebServer &server);
void handleRoot();
void handleNotFound();

#endif // HTTP_ROUTES_H
