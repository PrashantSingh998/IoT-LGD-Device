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
#include "RFControl.h"

unsigned long previousMillis = 0;
unsigned long lastPublishedMillis = 0;
unsigned long lastKeepaliveMillis = 0;
unsigned long lastTick_1s_unique = 0; 
unsigned long lastTick_1s_unique_irrigate = 0;

String versionUrl ="https://device-ota-bucket.s3.ap-south-1.amazonaws.com/Climate_Control/IFFNC1200000001/meta.json";
const char* currentVersion = "1.0.0";
int retryCount = 0;


void othertask(void *pvParameters) {

  for (;;) {   // 🔴 REQUIRED — THIS WAS MISSING

    // Serial.println("1");

    if (checkWiFiConnection() && client.connected() && !configMode) {

      client.loop();

      if (millis() - lastPublishedMillis >= 60000) {
        publish_keepalive_RF_MOdule(water_liters);
        // Serial.println("8");
        lastPublishedMillis = millis();
      }
      httploop();  // Handle HTTP server in config mode

    } else {

      if (connect_to_wifi(WIFI_SSID, WIFI_PASSWORD, true)) {
        // Serial.println("9");
        if (!configMode) { 
          reconnect(MQTT_SUBTOPIC, MQTT_USERNAME, MQTT_PASSWORD);
        }
        httploop();  // Handle HTTP server in config mode
      }
    }
     
    // Serial.println("10");

    vTaskDelay(pdMS_TO_TICKS(1));  // ✅ correct delay
  }
}


void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("<New code >>");

  initNVS();
  printNVSStats();
  loadConfigFromNVS();   // fills WiFi/MQTT + PUBLISH_INTERVAL_MS + tank params
  initFileSystem();  
  while (true) {
    if (beginRa02()) {
      Serial.println("RA-02 initialized successfully.");
      break;  // Exit while loop and continue normal code
    }
    retryCount++;
    Serial.printf("RA-02 initialization failed. Attempt %d/10\n",retryCount);
    if (retryCount >= 10) {
      Serial.println("RA-02 failed 10 times. Restarting ESP32...");
      delay(100);
      ESP.restart();
    }
    delay(1000); // Wait 1 second before next attempt
  }


  if (!configMode){
    Serial.println("Normal Mode: Connecting to WiFi and MQTT ssid is" + String(WIFI_SSID));
    Serial.println("Device ID is: " + String(DEVICE_ID)); 
    connect_to_wifi(WIFI_SSID ,WIFI_PASSWORD, false);
    checkAndUpdateFirmware(versionUrl, currentVersion);
    initroutes();

    initMqtt(MQTT_BROKER, MQTT_PORT);
    reconnect(MQTT_SUBTOPIC, MQTT_USERNAME, MQTT_PASSWORD);
    publish_device_info();
  } else {
    connect_to_wifi(WIFI_SSID ,WIFI_PASSWORD, false);
    initroutes();
    Serial.println("Config Mode: Started HTTP Server for configuration");
    
  }
  xTaskCreatePinnedToCore(receiveJsonPayload,"LoRaReceiveTask",2048,NULL,1,NULL,1);
  xTaskCreatePinnedToCore(othertask,"PrintTask",4096,NULL,1,NULL,0);
  // xTaskCreatePinnedToCore(buttonTask, "ButtonTask", 4096,NULL,1,NULL, 0);             
  


}

void loop() {

}
