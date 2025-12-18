#ifndef MQTT_CONTROL_H
#define MQTT_CONTROL_H

#include <Arduino.h>
#include <PubSubClient.h> // for PubSubClient declaration

// shared externs (defined in mqttcontrol.cpp and configControl.cpp)
extern PubSubClient client;
extern volatile bool drain_active;

// keepalive topic (defined in configControl.cpp via strdup from NVS)
extern const char* MQTT_KEEPALIVETOPIC;

void initMqtt(const char* MQTT_BROKER, int MQTT_PORT);
void reconnect(const char* MQTT_SUBTOPIC, const char* MQTT_USERNAME, const char* mqttPassword);
bool publish(const char* topic, const String& payload);
void callback(char* topic, byte* payload, unsigned int length);
bool publish_device_info();
bool publish_water_level_if_changed(float waterLevel);
bool publish_keepalive_water_level(float waterLevel);

#endif // MQTT_CONTROL_H
