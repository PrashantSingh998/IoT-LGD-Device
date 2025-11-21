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
const char* MQTT_BROKER = "";
int   MQTT_PORT = 0;
const char* MQTT_USERNAME = "";
const char* MQTT_PASSWORD = "";
const char* MQTT_RESPONSETOPIC = "";

bool shouldRestart = false;
bool configMode = false;

// ---- NEW: define the globals the linker complained about ----
unsigned long PUBLISH_INTERVAL_SEC = 60;   // default if not set in NVS
bool drain_active = false;                 // default: not draining

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
  WIFI_SSID          = strdup(prefs.getString("ssid", "Airtel_INNOFARMS AI AGRITECH_4G").c_str());
  WIFI_PASSWORD      = strdup(prefs.getString("pass", "Innofarm1008").c_str());
  MQTT_SUBTOPIC      = strdup(prefs.getString("subtop", "farm/118/Sub").c_str());
  MQTT_PUBTOPIC      = strdup(prefs.getString("pubtop", "farm/118/sensor").c_str());
  MQTT_RESPONSETOPIC = strdup(prefs.getString("resptop", "farm/118/Response").c_str());
  MQTT_BROKER        = strdup(prefs.getString("broker", "mqtt.innofarms.ai").c_str());
  MQTT_PORT          = prefs.getInt("port", 1883);
  MQTT_USERNAME      = strdup(prefs.getString("user", "webusers").c_str());
  MQTT_PASSWORD      = strdup(prefs.getString("mqtpass", "innofarmweb2024").c_str());

  // Publish interval (seconds)
  PUBLISH_INTERVAL_SEC = prefs.getUInt("pubInt", 60);

  // Tank parameters (Vmax, loft height, blind) — NVS only
  float tankV = prefs.getFloat("tankV", 500.0f);
  float loftH = prefs.getFloat("loftH", 48.8f);
  float blind = prefs.getFloat("blind", 3.0f);

  // If keys were missing, save defaults once (so next boot the NOT_FOUND goes away)
  if (!prefs.isKey("pubInt")) prefs.putUInt("pubInt", PUBLISH_INTERVAL_SEC);
  if (!prefs.isKey("tankV"))  prefs.putFloat("tankV", tankV);
  if (!prefs.isKey("loftH"))  prefs.putFloat("loftH", loftH);
  if (!prefs.isKey("blind"))  prefs.putFloat("blind", blind);

  // Push tank params into the water-level math (DeviceControl)
  updateTankParams(tankV, loftH, blind);

  Serial.println("✅ Config Loaded from NVS:");
  Serial.println("--------------------------------------");
  Serial.printf("WIFI_SSID            : %s\n", WIFI_SSID);
  Serial.printf("WIFI_PASSWORD        : %s\n", WIFI_PASSWORD);
  Serial.printf("MQTT_SUBTOPIC        : %s\n", MQTT_SUBTOPIC);
  Serial.printf("MQTT_PUBTOPIC        : %s\n", MQTT_PUBTOPIC);
  Serial.printf("MQTT_RESPONSETOPIC   : %s\n", MQTT_RESPONSETOPIC);
  Serial.printf("MQTT_BROKER          : %s\n", MQTT_BROKER);
  Serial.printf("MQTT_PORT            : %d\n", MQTT_PORT);
  Serial.printf("MQTT_USERNAME        : %s\n", MQTT_USERNAME);
  Serial.printf("MQTT_PASSWORD        : %s\n", MQTT_PASSWORD);
  Serial.printf("PUBLISH_INTERVAL_SEC : %lu\n", PUBLISH_INTERVAL_SEC);
  Serial.println("--------------------------------------");
  Serial.printf("[TANK] NVS load: Vmax=%.2f L | loftH=%.2f cm | blind=%.2f cm\n", tankV, loftH, blind);
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
  prefs.end();

  Serial.println("✅ Partial Config Updated & Saved to NVS");
  return true;
}

// ---- FS (unchanged, kept because other parts include them) ----
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