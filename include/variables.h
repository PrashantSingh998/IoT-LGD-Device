#ifndef VARIABLES_H
#define VARIABLES_H

#include <Arduino.h>
#include <HardwareSerial.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>


// Network / MQTT values populated by configControl
extern const char* WIFI_SSID;
extern const char* WIFI_PASSWORD;
extern const char* MQTT_SUBTOPIC;
extern const char* MQTT_PUBTOPIC;
extern const char* MQTT_RESPONSETOPIC;
extern const char* MQTT_BROKER;
extern int MQTT_PORT;
extern const char* MQTT_USERNAME;
extern const char* MQTT_PASSWORD;
extern const char* MQTT_INFOTOPIC;
extern float TANK_VOLUME;
extern float TANK_HEIGHT;
extern float TANK_BLIND_HEIGHT;
extern float PUBLISH_INTERVAL_MS;
extern float Lower_Limit;
extern float Higher_Limit;

extern const char* DEVICE_ID;


// Modes / flags
extern bool shouldRestart;
extern bool configMode;
extern int FARM_ID;
extern int Config_Button;

extern bool publishflag;
extern int rack_id;
extern int shelf_id;

#define ULTRA_RX   16
#define ULTRA_TX   17
#define ULTRA_BAUD 9600


#define LORA_SCK   18
#define LORA_MISO  19
#define LORA_MOSI  23
#define LORA_NSS   13
#define LORA_RST   27
#define LORA_DIO0  26

#define LORA_FREQUENCY 433000000L   // RA-02 normally uses 433 MHz
#define LORA_SYNC_WORD 0x34         // Must match sender; this is NOT encryption



#endif
