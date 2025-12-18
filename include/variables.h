#ifndef VARIABLES_H
#define VARIABLES_H

#include <Arduino.h>

// Network / MQTT values populated by configControl
extern const char* WIFI_SSID;
extern const char* WIFI_PASSWORD;
extern const char* MQTT_SUBTOPIC;
extern const char* MQTT_PUBTOPIC;
extern const char* MQTT_RESPONSETOPIC;
extern const char* MQTT_BROKER;
extern int         MQTT_PORT;
extern const char* MQTT_USERNAME;
extern const char* MQTT_PASSWORD;
extern const char* MQTT_INFOTOPIC;


// Modes / flags
extern bool shouldRestart;
extern bool configMode;

// App timing

extern unsigned long PUBLISH_INTERVAL_SEC;



#endif
