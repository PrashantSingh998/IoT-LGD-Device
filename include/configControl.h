#ifndef CONFIG_CONTROL_H
#define CONFIG_CONTROL_H

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Preferences.h>

extern Preferences prefs;

bool initNVS();
void loadConfigFromNVS();
bool updateConfig(
  String ssid , 
  String pass , 
  String subtop, 
  String pubtop , 
  String resptop ,
  String broker , 
  int port, 
  String user, 
  String mqtpass
) ;

bool initFileSystem();
bool fileExists(const char* path);
DynamicJsonDocument readJsonFile(const char* path);
bool updateJsonFile(const char* path, JsonDocument& doc);
void createDefaultJson(const char* path);
void printConfig(const JsonDocument &config);
DynamicJsonDocument checkAndLoadConfig(const char *filename);
void printNVSStats() ;

#endif
