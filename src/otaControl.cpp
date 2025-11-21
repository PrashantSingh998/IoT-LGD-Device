#include "otaControl.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <Update.h>

void checkAndUpdateFirmware(const String &versionUrl,const char * currentVersion )  {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected.");
    return;
  }

  HTTPClient httpClient;
  httpClient.setTimeout(20000);
  httpClient.begin(versionUrl);
  int httpCode = httpClient.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = httpClient.getString();
    StaticJsonDocument<256> json;
    auto err = deserializeJson(json, payload);
    if (!err) {
      String latest = json["version"].as<String>();
      String binUrl = json["bin_url"].as<String>();
      httpClient.end();

      if (latest.length() && latest != currentVersion) {
        Serial.println("New version available. Starting OTA update...");
        if (httpClient.begin(binUrl)) {
          int code = httpClient.GET();
          Serial.printf("HTTP GET result: %d\n", code);
          if (code == HTTP_CODE_OK) {
            size_t len = httpClient.getSize();
            if (Update.begin(len, U_FLASH)) {
              WiFiClient *stream = httpClient.getStreamPtr();
              size_t written = Update.writeStream(*stream);
              if (written == len && Update.end() && Update.isFinished()) {
                Serial.println("Update complete. Rebooting...");
                ESP.restart();
              } else {
                Serial.printf("OTA error: %d\n", Update.getError());
              }
            } else {
              Serial.println("Unable to begin OTA Update.");
            }
          } else {
            Serial.printf("HTTP error code: %d\n", code);
          }
        } else {
          Serial.println("Unable to begin OTA HTTP request.");
        }
      } else {
        Serial.println("No new version available.");
      }
    } else {
      Serial.println("Failed to parse version JSON.");
      httpClient.end();
    }
  } else {
    Serial.println("Failed to get version info.");
    httpClient.end();
  }
}

void handleOTA() {
  // no-op placeholder
}
