#include <Arduino.h>
#include <esp_system.h>
#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <Preferences.h>

#include "configControl.h"
#include "deviceControl.h"
#include "bleControl.h"
#include "mqttControl.h"
#include "networkControl.h"
#include "otaControl.h"
#include "variables.h"

unsigned long previousMillis = 0;
unsigned long lastPublishedMillis = 0;

String versionUrl ="https://innofarmsiotfirmware.blob.core.windows.net/farm118/metadata.json";
const char* currentVersion = "1.0.5";

extern PubSubClient client; 
static void safeRestart(const char* reason = nullptr) {
  if (reason) {
    Serial.print("safeRestart(): ");
    Serial.println(reason);
  } else {
    Serial.println("safeRestart()");
  }
  delay(200);
  esp_restart();
}

static bool attemptRecoveryOrRestart(std::function<bool()> recover, int tries = 2, unsigned long delayMs = 2000) {
  for (int i = 0; i < tries; ++i) {
    Serial.printf("attemptRecoveryOrRestart: try %d/%d\n", i + 1, tries);
    if (recover()) {
      Serial.println("Recovery OK");
      return true;
    }
    delay(delayMs);
  }
  // if recovery fail after tries, reboot device
  safeRestart("auto-recovery-failed");
  return false; // unreachable (esp_restart)
}// from mqttcontrol.cpp

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("<New code >>");

  initNVS();
  printNVSStats();

  loadConfigFromNVS();   // fills WiFi/MQTT + PUBLISH_INTERVAL_SEC + tank params
  initFileSystem();      // if you still need FS elsewhere
  initSensors();
  initUltrasonicSensor(); // fixed UART pins from deviceControl.cpp

  if (!configMode){
    connect_to_wifi(WIFI_SSID ,WIFI_PASSWORD);
    checkAndUpdateFirmware(versionUrl, currentVersion);

    initMqtt(MQTT_BROKER, MQTT_PORT);
    reconnect(MQTT_SUBTOPIC, MQTT_USERNAME, MQTT_PASSWORD);
    publish_device_info();
  } else {
    turnONBLE();
  }
}

void loop() {
  // service MQTT incoming (for drain callback)
  if (WiFi.status() == WL_CONNECTED) {
    client.loop();
  }

  // periodic reconnect (15s)
  if (!configMode && millis() - previousMillis >= 15000 ){
    previousMillis = millis();

    // minimal change: track consecutive WiFi/MQTT failures and reboot after threshold
    static int wifiFailCount = 0;
    static int mqttFailCount = 0;
    const int FAIL_THRESHOLD = 5; // reboot after 5 consecutive failures

    if (!checkWiFiConnection()) {
      wifiFailCount++;
      Serial.printf("WiFi not connected. wifiFailCount=%d\n", wifiFailCount);
      connect_to_wifi(WIFI_SSID ,WIFI_PASSWORD);
      // recheck
      if (!checkWiFiConnection()) {
        if (wifiFailCount >= FAIL_THRESHOLD) {
          safeRestart("WiFi failed threshold reached");
        }
      }
    } else {
      // reset counter on success
      if (wifiFailCount != 0) {
        Serial.println("WiFi reconnected, resetting wifiFailCount");
      }
      wifiFailCount = 0;
    }

    // Ensure MQTT is connected; reconnect and count failures
    reconnect(MQTT_SUBTOPIC, MQTT_USERNAME, MQTT_PASSWORD);
    if (!client.connected()) {
      mqttFailCount++;
      Serial.printf("MQTT not connected after reconnect(), mqttFailCount=%d\n", mqttFailCount);
      if (mqttFailCount >= FAIL_THRESHOLD) {
        safeRestart("MQTT failed threshold reached");
      }
    } else {
      if (mqttFailCount != 0) {
        Serial.println("MQTT reconnected, resetting mqttFailCount");
      }
      mqttFailCount = 0;
    }
  }

  // read every ~5s
  static unsigned long last_read_ms = 0;
  if (millis() - last_read_ms < 5000) return;
  last_read_ms = millis();

  float distance_cm = averageDistanceCm();
  if (isnan(distance_cm) || distance_cm > 1000.0f) {
    Serial.println("[WL] invalid/out-of-range, retry next cycle");
    return;
  }

  float volume_liters = calculateVolumeLiters(distance_cm);
  Serial.printf("Water present: %.2f L\n", volume_liters);

  // publish on interval
  if (!configMode && (millis() - lastPublishedMillis) >= (PUBLISH_INTERVAL_SEC * 1000UL)) {
    // payload: {"DN":"WL","WaterLevel":<liters>}
    String payload;
    payload.reserve(64);
    payload += "{\"DN\":\"WL\",\"WaterLevel\":";
    payload += String(volume_liters, 2);
    payload += "}";

    // reuse mqttFailCount (declared earlier in reconnect block as static, so still accessible)
    extern PubSubClient client; // already declared above; here to clarify usage
    static int mqttFailCount_publish = 0;
    const int FAIL_THRESHOLD_PUB = 5;

    bool ok = publish(MQTT_PUBTOPIC, payload);
    if (!ok) {
      mqttFailCount_publish++;
      Serial.printf("Publish failed, mqttFailCount_publish=%d\n", mqttFailCount_publish);

      reconnect(MQTT_SUBTOPIC, MQTT_USERNAME, MQTT_PASSWORD);

      if (!client.connected()) {
        if (mqttFailCount_publish >= FAIL_THRESHOLD_PUB) {
          safeRestart("MQTT publish failed threshold reached");
        }
      }
    } else {
      if (mqttFailCount_publish != 0) {
        Serial.println("Publish OK, resetting mqttFailCount_publish");
      }
      mqttFailCount_publish = 0;
    }
    lastPublishedMillis = millis();
  }
}
