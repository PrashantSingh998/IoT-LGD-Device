#ifndef NETWORK_CONTROL_H
#define NETWORK_CONTROL_H

#include <Arduino.h>

bool connect_to_wifi(const char* ssid, const char* password, bool recconnect);
bool checkWiFiConnection();

#endif
