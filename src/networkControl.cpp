#include "configControl.h"
#include "deviceControl.h"
#include "bleControl.h"
#include "mqttControl.h"
#include "networkControl.h"
#include "otaControl.h"




void connect_to_wifi(const char* ssid, const char* password) {
  Serial.println();
  Serial.println(" Connecting to WiFi...");
  Serial.print("SSID: ");
  Serial.println(ssid);
  


  WiFi.mode(WIFI_STA);           // Station mode
  // WiFi.begin(ssid, password);
  WiFi.begin("Airtel_INNOFARMS AI AGRITECH_4G", "Innofarm1008");

  unsigned long startAttemptTime = millis();
  const unsigned long timeout = 15000;  // 15 seconds timeout

  // Wait until connected or timeout
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < timeout) {
    Serial.print(".");
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ Failed to connect to WiFi!");
  }
}


bool checkWiFiConnection() {
  return (WiFi.status() == WL_CONNECTED);
}