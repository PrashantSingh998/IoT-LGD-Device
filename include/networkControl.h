#ifndef NETWORK_CONTROL_H
#define NETWORK_CONTROL_H

#include <Arduino.h>

void connect_to_wifi(const char* ssid, const char* password);
bool checkWiFiConnection();

#endif
