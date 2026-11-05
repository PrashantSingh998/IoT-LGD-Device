#include "configControl.h"
#include "deviceControl.h"
#include "bleControl.h"
#include "mqttControl.h"
#include "networkControl.h"
#include "otaControl.h"
#include "variables.h"

#include <PubSubClient.h>
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

#include <esp_system.h>   // <-- for esp_random()

WiFiClient espClient;
PubSubClient client(espClient);

// bool publishflag = false;
int rack_id = -1;
int shelf_id = -1;

// Hardcoded device id
const char* DEVICE_ID = "IFLGD1220000001";

// NEW: track last hourly publish time (for 1 hr forced publish)
static unsigned long lastHourlyPublishMs = 0;


void initMqtt(const char* MQTT_BROKER, int MQTT_PORT) {
    if (checkWiFiConnection()) {
        client.setServer(MQTT_BROKER, MQTT_PORT);
        client.setCallback(callback);
        client.setKeepAlive(40);
        delay(1000);
    }
}

void reconnect(const char* MQTT_SUBTOPIC, const char* MQTT_USERNAME, const char* mqttPassword) {
    if (checkWiFiConnection()) {
        while (!client.connected()) {
            Serial.println(F("Attempting MQTT connection..."));
            String devicemac_string = WiFi.macAddress();
            String subTopic = MQTT_SUBTOPIC;
            Serial.println("subTopic : " + subTopic);
            if (client.connect(devicemac_string.c_str(), MQTT_USERNAME, mqttPassword)) {
                digitalWrite(2, HIGH);
                bool flag1 = client.subscribe(subTopic.c_str()); // for device config updates
                Serial.println("Subscribed to topic: " + subTopic + " Result: " + String(flag1));

                
            } else {
                Serial.print("failed, rc=");
                Serial.print(client.state());
                Serial.println(" try again in 5 seconds");
                digitalWrite(2, LOW);
                delay(5000);
            }
            delay(500);
        }
    }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Received [");
  Serial.print(topic);
  Serial.print("]: ");

  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }

  Serial.println();

  String topicStr = String(topic);

  DynamicJsonDocument doc(512);

  DeserializationError error = deserializeJson(doc, payload, length);

  if (error) {
    Serial.print("Failed to parse JSON: ");
    Serial.println(error.c_str());
    return;
  }

  // Only process messages from the required subscription topic.
  if (topicStr != MQTT_SUBTOPIC || !doc.containsKey("DN")) {
    return;
  }

  const char* dn = doc["DN"] | "";
  const char* deviceId = doc["Device_Id"] | "";
  const char* cmd = doc["cmd"] | "";

  if (strcmp(dn, "LGD") != 0) {
    Serial.println(F("Message for other DN"));
    return;
  }

  if (strcmp(deviceId, DEVICE_ID) != 0) {
    Serial.println(F("DEVICE ID IS NOT SAME."));
    return;
  }

  Serial.println(F("Received LGD command on config topic"));

  if (strcmp(cmd, "server_detail") == 0) {

    if (doc.containsKey("WIFI_SSID")) {
      String ssid1 = doc["WIFI_SSID"].as<String>();

      prefs.putString("ssid", ssid1);

      delay(100);

      String savedSsid = prefs.getString("ssid", "fec");
      Serial.println("SSID saved in NVS: " + savedSsid);
    }

    if (doc.containsKey("WIFI_PASSWORD")) {
      prefs.putString("pass", doc["WIFI_PASSWORD"].as<String>());
    }

    if (doc.containsKey("FARM_ID")) {
      prefs.putInt("Farmid", doc["FARM_ID"].as<int>());
    }

    if (doc.containsKey("MQTT_SUBTOPIC")) {
      prefs.putString("subtop", doc["MQTT_SUBTOPIC"].as<String>());
    }

    if (doc.containsKey("MQTT_PUBTOPIC")) {
      prefs.putString("pubtop", doc["MQTT_PUBTOPIC"].as<String>());
    }

    if (doc.containsKey("MQTT_KEEPALIVETOPIC")) {
      prefs.putString(
        "keeptopic",
        doc["MQTT_KEEPALIVETOPIC"].as<String>()
      );
    }

    if (doc.containsKey("MQTT_RESPONSETOPIC")) {
      prefs.putString(
        "resptop",
        doc["MQTT_RESPONSETOPIC"].as<String>()
      );
    }

    if (doc.containsKey("PUBLISH_INTERVAL")) {
      prefs.putFloat(
        "pubInt",
        doc["PUBLISH_INTERVAL"].as<float>()
      );
    }

    Serial.println("Tank Config Updated & Saved to NVS. Restarting device...");
    delay(500);
    ESP.restart();

  } else {
    Serial.println(F("Invalid MQTT command"));
  }
}

// ===== MQTT publish wrapper =====
bool publish(const char* topic, const String& payload) {
    if (!client.connected()) {
        Serial.println(" MQTT client not connected. Publish failed.");
        return false;
    }
    bool success = client.publish(topic, payload.c_str());
    if (success) {
        Serial.println("MQTT Publish successful!");
        Serial.print("Topic: "); Serial.println(topic);
        Serial.print("Payload: "); Serial.println(payload);
    } else {
        Serial.println("MQTT Publish failed!");
    }
    return success;
}

bool publish_keepalive_RF_MOdule(float waterLevel) {
    if (!client.connected()) {
        Serial.println("MQTT client not connected. keepalive skipped.");
        return false;
    }
    DynamicJsonDocument doc(128);
    doc["DN"] = "LGD";
    doc["Device_Id"] = DEVICE_ID;   // you already had this in your last paste
    doc["WIFI_RSSI"] = WiFi.RSSI();  // NEW: include WiFi signal strength in keepalive
    String payload; serializeJson(doc, payload);

    // publish to dedicated keepalive topic from configControl (MQTT_KEEPALIVETOPIC)
    bool success = client.publish(MQTT_KEEPALIVETOPIC, payload.c_str());
    if (success) {
        Serial.println("Keepalive publish successful.");
        Serial.print("Keepalive topic: "); Serial.println(MQTT_KEEPALIVETOPIC);
        Serial.print("Payload: "); Serial.println(payload);
    } else {
        Serial.println("Keepalive publish failed!");
    }
    return success;
}

bool publish_device_info() {
    DynamicJsonDocument doc(256);
    doc["MAC_ID"] = WiFi.macAddress();
    doc["IP"] = WiFi.localIP().toString();
    doc["CHIP_No"] = String((uint32_t)ESP.getEfuseMac());
    doc["DN"] = "LGD";
    // NEW: add Packet_Id and Device_Id for device-info packet
   ;

    String jsonString; serializeJson(doc, jsonString);
    return publish(MQTT_INFOTOPIC, jsonString);
}