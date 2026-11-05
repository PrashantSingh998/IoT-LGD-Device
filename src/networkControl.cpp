#include "configControl.h"
#include "deviceControl.h"
#include "bleControl.h"
#include "mqttControl.h"
#include "networkControl.h"
#include "otaControl.h"
#include "variables.h"



bool connect_to_wifi(const char* ssid, const char* password, bool recconnect = false) {
  Serial.println();
  Serial.println(" Connecting to WiFi...");
  Serial.print("SSID: ");
  const char* ssid1 = strdup(prefs.getString("ssid","Airtel_INNOFARMS AI AGRITECH_4G").c_str());
  const char* password1 = strdup(prefs.getString("pass",     "Innofarm1008").c_str());
  Serial.println(ssid1);
  unsigned long timeout;
  
 

  WiFi.mode(WIFI_STA);           // Station mode
  // WiFi.begin(ssid1, password1);
  WiFi.begin("Airtel_INNOFARMS AI AGRITECH_4G", "Innofarm1008");

  unsigned long startAttemptTime = millis();
  if (recconnect){
    timeout = 60000;  // 60 seconds timeout
  }else {
    timeout = 15000;  // 30 seconds timeout
  }
 

  // Wait until connected or timeout
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout) {
    Serial.print(".");
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    return true;
  } else {
    Serial.println("\n❌ Failed to connect to WiFi!");
    if (recconnect){
      Serial.println("\n❌ Restarting esp32 as it Failed to connect to WiFi!");
      delay(500);
      esp_restart();
      
    }
    return false;
  }
}


bool checkWiFiConnection() {
  return (WiFi.status() == WL_CONNECTED);
}