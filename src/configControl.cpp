#include "configControl.h"
#include "deviceControl.h"
#include "bleControl.h"
#include "mqttControl.h"
#include "networkControl.h"
#include "otaControl.h"
#include "variables.h"

#include <nvs_flash.h>
#include <LittleFS.h>
#include <Preferences.h>

Preferences prefs;


// ---- WiFi/MQTT globals (same as your original) ----
const char* WIFI_SSID = "";
const char* WIFI_PASSWORD = "";
const char* MQTT_SUBTOPIC = "";
const char* MQTT_PUBTOPIC = "";
const char* MQTT_KEEPALIVETOPIC = "";
const char* MQTT_BROKER = "";
int   MQTT_PORT = 0;
const char* MQTT_USERNAME = "";
const char* MQTT_PASSWORD = "";
const char* MQTT_RESPONSETOPIC = "";
const char* MQTT_INFOTOPIC = "";      //  NEW: info topic for device info payload

float TANK_VOLUME;
float TANK_HEIGHT;
float TANK_BLIND_HEIGHT;
float PUBLISH_INTERVAL_MS;
int FARM_ID;
float Lower_Limit;
float Higher_Limit;

bool shouldRestart = false;
bool configMode = false;
bool drain_active = false;
bool irrigate_active = false;
bool publishflag = false;


// ---- NVS init/print ----
bool initNVS() {
  if (!prefs.begin("config", false)) {   // false = RW
    Serial.println("Failed to initialize NVS!");
    return false;
  }
  Serial.println(" NVS Initialized Successfully");
  return true;
}

void printNVSStats() {
  nvs_stats_t nvsStats;
  nvs_handle handle;

  if (nvs_flash_init() != ESP_OK) {
    Serial.println("Failed to init NVS");
    return;
  }

  if (nvs_open("config", NVS_READWRITE, &handle) == ESP_OK) {
    nvs_get_stats(NULL, &nvsStats);

    Serial.println("===== NVS MEMORY USAGE =====");
    Serial.printf("Total Entries : %d\n", nvsStats.total_entries);
    Serial.printf("Used  Entries : %d\n", nvsStats.used_entries);
    Serial.printf("Free  Entries : %d\n", nvsStats.free_entries);

    nvs_close(handle);
  } else {
    Serial.println("Failed to open NVS namespace");
  }
}





// ---- Load all runtime config from NVS (NO JSON) ----
void loadConfigFromNVS() {
  // WiFi / MQTT (with sane defaults)
  String WIFI_SSID1           = prefs.getString("ssid","Airtel_INNOFARMS AI AGRITECH_4G");
  Serial.println("SSID from NVS: " + WIFI_SSID1);
  WIFI_SSID = WIFI_SSID1.c_str();
  WIFI_PASSWORD       = strdup(prefs.getString("pass","Innofarm1008").c_str());
  FARM_ID = prefs.getInt("Farmid", 1193);
  
  MQTT_SUBTOPIC       = strdup(prefs.getString("subtop",   "Sub").c_str());
  String fullSubTopic = "farm/" + String(FARM_ID) + "/" + String(MQTT_SUBTOPIC);
  MQTT_SUBTOPIC       = strdup(fullSubTopic.c_str());  // build full topic

  MQTT_PUBTOPIC       = strdup(prefs.getString("pubtop",   "sensor").c_str());
  String fullPubTopic = "farm/" + String(FARM_ID) + "/" + String(MQTT_PUBTOPIC);
  MQTT_PUBTOPIC       = strdup(fullPubTopic.c_str());  // build full topic

  MQTT_KEEPALIVETOPIC = strdup(prefs.getString("keeptopic","sensor_keepalive").c_str());
  String fullKeepaliveTopic = "farm/" + String(FARM_ID) + "/" + String(MQTT_KEEPALIVETOPIC);
  MQTT_KEEPALIVETOPIC = strdup(fullKeepaliveTopic.c_str());  // build full topic

  MQTT_RESPONSETOPIC  = strdup(prefs.getString("resptop",  "Response").c_str());
  String fullRespTopic = "farm/" + String(FARM_ID) + "/" + String(MQTT_RESPONSETOPIC);
  MQTT_RESPONSETOPIC  = strdup(fullRespTopic.c_str());  // build full topic

  MQTT_INFOTOPIC      = strdup(prefs.getString("infotop",  "info").c_str()); 
  String fullInfoTopic = "farm/" + String(FARM_ID) + "/" + String(MQTT_INFOTOPIC);
  MQTT_INFOTOPIC      = strdup(fullInfoTopic.c_str());  // build full topic




  MQTT_BROKER         = strdup(prefs.getString("broker",   "mqtt.innofarms.ai").c_str());
  MQTT_PORT           = prefs.getInt("port", 1883);
  MQTT_USERNAME       = strdup(prefs.getString("user",     "webusers").c_str());
  MQTT_PASSWORD       = strdup(prefs.getString("mqtpass",  "innofarmweb2024").c_str()); 
  Higher_Limit       = prefs.getFloat("HL", 1000.0f);
  Lower_Limit        = prefs.getFloat("LL", 100.0f); 
  TANK_VOLUME       = prefs.getFloat("tankV", 500.0f);
  TANK_HEIGHT       = prefs.getFloat("tankH", 48.8f);
  TANK_BLIND_HEIGHT = prefs.getFloat("blind", 3.0f);

  drain_active = prefs.getBool("drain", false);
  irrigate_active = prefs.getBool("irrigate", false);
  publishflag = prefs.getBool("pubflag", true);

  float PUBLISH_INTERVAL = prefs.getFloat("pubInt", 30.0f);
  PUBLISH_INTERVAL_MS = PUBLISH_INTERVAL*60*1000; // convert minutes to milliseconds



 Serial.println("\nConfig Loaded from NVS:");
  Serial.println("------------------------------------------------");

  Serial.printf("WIFI_SSID             : %s\n", WIFI_SSID);
  Serial.printf("WIFI_PASSWORD         : %s\n", WIFI_PASSWORD);
  Serial.printf("FARM_ID               : %d\n", FARM_ID); 

  Serial.printf("MQTT_BROKER           : %s\n", MQTT_BROKER);
  Serial.printf("MQTT_PORT             : %d\n", MQTT_PORT);
  Serial.printf("MQTT_USERNAME         : %s\n", MQTT_USERNAME);
  Serial.printf("MQTT_PASSWORD         : %s\n", MQTT_PASSWORD);

  Serial.printf("MQTT_SUBTOPIC         : %s\n", MQTT_SUBTOPIC);
  Serial.printf("MQTT_PUBTOPIC         : %s\n", MQTT_PUBTOPIC);
  Serial.printf("MQTT_KEEPALIVETOPIC   : %s\n", MQTT_KEEPALIVETOPIC);
  Serial.printf("MQTT_RESPONSETOPIC    : %s\n", MQTT_RESPONSETOPIC);
  Serial.printf("MQTT_INFOTOPIC        : %s\n", MQTT_INFOTOPIC);

  Serial.printf("TANK_VOLUME (Liters)  : %.2f\n", TANK_VOLUME);
  Serial.printf("TANK_HEIGHT (cm)      : %.2f\n", TANK_HEIGHT);
  Serial.printf("TANK_BLIND_HEIGHT (cm): %.2f\n", TANK_BLIND_HEIGHT);
  Serial.println("drain_active: " + String(drain_active));
  Serial.println("irrigate_active: " + String(irrigate_active));
  Serial.println("publishflag: " + String(publishflag));

  Serial.printf("PUBLISH_INTERVAL_MS  : %.2f\n", PUBLISH_INTERVAL_MS);

  Serial.println("------------------------------------------------");
}

void resetWifiCred(){

  prefs.putString("ssid", "innofarms");
  prefs.putString("pass", "innofarms1008");
  Serial.println("✅ Tank Config Updated & Saved to NVS.   Restarting device...");
  delay(500);
  ESP.restart();


}

// ---- Update NVS with partial WiFi/MQTT changes (unchanged) ----
bool updateConfig(
    String ssid, 
    String pass, 
    String subtop, 
    String pubtop, 
    String resptop,
    String broker, 
    int port, 
    String user, 
    String mqtpass
    ) {

  if (ssid.length() > 0)       WIFI_SSID = strdup(ssid.c_str());
  if (pass.length() > 0)       WIFI_PASSWORD = strdup(pass.c_str());
  if (subtop.length() > 0)     MQTT_SUBTOPIC = strdup(subtop.c_str());
  if (pubtop.length() > 0)     MQTT_PUBTOPIC = strdup(pubtop.c_str());
  if (resptop.length() > 0)    MQTT_RESPONSETOPIC = strdup(resptop.c_str());
  if (broker.length() > 0)     MQTT_BROKER = strdup(broker.c_str());
  if (port > 0)                MQTT_PORT = port;
  if (user.length() > 0)       MQTT_USERNAME = strdup(user.c_str());
  if (mqtpass.length() > 0)    MQTT_PASSWORD = strdup(mqtpass.c_str());

  prefs.begin("config", false);
  prefs.putString("ssid", WIFI_SSID);
  prefs.putString("pass", WIFI_PASSWORD);
  prefs.putString("subtop", MQTT_SUBTOPIC);
  prefs.putString("pubtop", MQTT_PUBTOPIC);
  prefs.putString("resptop", MQTT_RESPONSETOPIC);
  prefs.putString("broker", MQTT_BROKER);
  prefs.putInt("port", MQTT_PORT);
  prefs.putString("user", MQTT_USERNAME);
  prefs.putString("mqtpass", MQTT_PASSWORD);
  // ⚠ info topic is only handled in loadConfigFromNVS defaults for now
  prefs.end();

  Serial.println(" Partial Config Updated & Saved to NVS");
  return true;
}


bool initFileSystem() {
  if (!LittleFS.begin(true)) {
    Serial.println(" LittleFS Mount Failed");
    return false;
  }
  Serial.println(" LittleFS Mounted Successfully");
  return true;
}
bool fileExists(const char* path) {
  return LittleFS.exists(path);
}

void createDefaultJson(const char* path) {
  DynamicJsonDocument doc(256);
  doc["device"] = "ESP32";
  doc["version"] = 1.0;
  doc["status"] = "initialized";

  File file = LittleFS.open(path, FILE_WRITE);
  if (!file) {
    Serial.println(" Failed to create default JSON file");
    return;
  }

  serializeJsonPretty(doc, file);
  file.close();
  Serial.println(" Default JSON file created");
}

DynamicJsonDocument readJsonFile(const char* path) {
  if (!fileExists(path)) {
    Serial.println(" JSON file not found. Creating default...");
    createDefaultJson(path);
  }

  File file = LittleFS.open(path, FILE_READ);
  if (!file) {
    Serial.println(" Failed to open JSON file for reading");
    return DynamicJsonDocument(0);
  }

  size_t size = file.size();
  (void)size;
  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, file);

  if (error) {
    Serial.print(" Failed to parse JSON: ");
    Serial.println(error.f_str());
  } else {
    Serial.println(" JSON file read successfully");
  }

  file.close();
  return doc;
}

bool updateJsonFile(const char* path, JsonDocument& doc) {
  File file = LittleFS.open(path, FILE_WRITE);
  if (!file) {
    Serial.println(" Failed to open JSON file for writing");
    return false;
  }

  if (serializeJsonPretty(doc, file) == 0) {
    Serial.println(" Failed to write JSON data");
    file.close();
    return false;
  }

  file.close();
  Serial.println(" JSON file updated successfully");
  return true;
}

void printConfig(const JsonDocument &config) {
  Serial.println("===== Config File Content =====");
  serializeJsonPretty(config, Serial);
  Serial.println("\n===============================");
}

DynamicJsonDocument checkAndLoadConfig(const char *filename) {
  DynamicJsonDocument config(512);
  if (!LittleFS.exists(filename)) {
    Serial.println(" Config file not found. Creating default one");
    File newFile = LittleFS.open(filename, "w");
    if (newFile) {
      config["device"] = "InnofarmsAI";
      config["ssid"] = "InnofarmsAI";
      config["password"] = "InnofarmsAI123";
      config["mqtt_subtopic"] = "default/subtopic";
      config["mqtt_pubtopic"] = "default/pubtopic";
      config["mqtt_responsetopic"] = "default/responsetopic";
      config["mqtt_broker"] = "broker.hivemq.com";
      config["mqtt_port"] = 1883;
      config["mqtt_username"] = "user";
      config["mqtt_password"] = "password";
      serializeJsonPretty(config, newFile);
      newFile.close();
      Serial.println("Default config file created.");
    }
    return config;
  }

  File file = LittleFS.open(filename, "r");
  if (!file) {
    Serial.println("Failed to open config file");
    return config;
  }

  DeserializationError error = deserializeJson(config, file);
  file.close();

  if (error) {
    Serial.print("Invalid JSON format: ");
    Serial.println(error.c_str());
    File newFile = LittleFS.open(filename, "w");
    if (newFile) {
      config.clear();
      config["device"] = "ESP32_Device";
      config["ssid"] = "defaultSSID";
      config["password"] = "defaultPassword";
      serializeJsonPretty(config, newFile);
      newFile.close();
      Serial.println("Recreated default config file.");
    }
  } else {
    Serial.println(" JSON file is valid!");
  }

  return config;
}



String generateUUID() {
    uint8_t uuid[16];

    // Fill uuid with random bytes from ESP32 hardware RNG
    for (int i = 0; i < 16; i++) {
        uuid[i] = (uint8_t)esp_random();
    }

    // Set version (UUID v4)
    uuid[6] = (uuid[6] & 0x0F) | 0x40;

    // Set variant (RFC4122)
    uuid[8] = (uuid[8] & 0x3F) | 0x80;

    char uuidStr[37];
    sprintf(uuidStr,
            "%02x%02x%02x%02x-"
            "%02x%02x-"
            "%02x%02x-"
            "%02x%02x-"
            "%02x%02x%02x%02x%02x%02x",
            uuid[0], uuid[1], uuid[2], uuid[3],
            uuid[4], uuid[5],
            uuid[6], uuid[7],
            uuid[8], uuid[9],
            uuid[10], uuid[11], uuid[12], uuid[13], uuid[14], uuid[15]);

    return String(uuidStr);
}