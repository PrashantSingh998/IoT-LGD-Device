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
#include <Preferences.h>

WiFiClient espClient;
PubSubClient client(espClient);
extern volatile bool drain_active;

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
                client.subscribe(subTopic.c_str());
                Serial.println(F("connected to MQTT server"));
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
    for (unsigned int i = 0; i < length; i++) Serial.print((char)payload[i]);
    Serial.println();

    DynamicJsonDocument doc(512);
    DeserializationError error = deserializeJson(doc, payload, length);
    if (error) {
        Serial.print("Failed to parse JSON: ");
        Serial.println(error.c_str());
        return;
    }

    // ===== 1. Handle irrigation drain pause/resume =====
    // Expected payload:
    // {
    //   "cmd": "irrigation",
    //   "status": {
    //      "task": "drain",
    //      "status": "start" / "completed"
    //   }
    // }
    if (doc.containsKey("status")) {
        JsonObject st = doc["status"];
        const char* task = st["task"] | "";
        const char* state = st["status"] | "";
        Serial.printf("[IRRIGATION] task=%s, status=%s\n", task, state);

        if (strcmp(task, "drain") == 0) {
            if (strcmp(state, "start") == 0) {
                drain_active = true;
                Serial.println("✅ Drain started — pausing water level publish.");
            } else if (strcmp(state, "completed") == 0 ||
                       strcmp(state, "done") == 0 ||
                       strcmp(state, "stopped") == 0) {
                drain_active = false;
                Serial.println("✅ Drain completed — resuming water level publish.");
            }
        }
    }

    // ===== 2. Dynamic tank update =====
    const char* cmd = doc["cmd"] | "";
    if (strcmp(cmd, "update_tank") == 0 && doc.containsKey("tank")) {
        JsonObject tank = doc["tank"];

        if (tank.containsKey("tank_Volume") &&
            tank.containsKey("loft_height") &&
            tank.containsKey("blind_spot_cm")) {

            float tv = tank["tank_Volume"].as<float>();
            float lh = tank["loft_height"].as<float>();
            float bl = tank["blind_spot_cm"].as<float>();

            updateTankParams(tv, lh, bl);
            prefs.begin("config", false);
            prefs.putFloat("tankV", tv);
            prefs.putFloat("loftH", lh);
            prefs.putFloat("blind", bl);
            prefs.end();

            DynamicJsonDocument ack(128);
            ack["ack"] = "update_tank";
            ack["status"] = "ok";
            String ackPayload; serializeJson(ack, ackPayload);
            publish(MQTT_RESPONSETOPIC, ackPayload);
        } else {
            DynamicJsonDocument nack(160);
            nack["ack"] = "update_tank";
            nack["status"] = "error";
            nack["reason"] = "missing_fields";
            String nackPayload; serializeJson(nack, nackPayload);
            publish(MQTT_RESPONSETOPIC, nackPayload);
        }
        return;
    }

    // ===== 3. THC SendConfigData Response =====
    const char* DN = doc["DN"] | "";
    if (strcmp(DN, "WL") == 0) {
        const char* command = doc["command"] | "";
        if (strcmp(command, "SendCongigData") == 0) {
            DynamicJsonDocument configuration(256);
            configuration["wifi_ssid"] = WIFI_SSID;
            configuration["wifi_password"] = WIFI_PASSWORD;
            configuration["mqtt_broker"] = MQTT_BROKER;
            configuration["mqtt_port"] = MQTT_PORT;
            configuration["mqtt_username"] = MQTT_USERNAME;
            configuration["mqtt_password"] = MQTT_PASSWORD;
            configuration["mqtt_sub_topic"] = MQTT_SUBTOPIC;
            configuration["mqtt_pub_topic"] = MQTT_PUBTOPIC;
            configuration["mqtt_response_topic"] = MQTT_RESPONSETOPIC;
            configuration["publish_interval_sec"] = PUBLISH_INTERVAL_SEC;

            String payloadOut;
            serializeJson(configuration, payloadOut);
            publish(MQTT_RESPONSETOPIC, payloadOut);
            Serial.println("Response sent: " + payloadOut);
        } else {
            Serial.println(F("invalid Mqtt command"));
        }
    }
}

// ===== MQTT publish wrapper =====
bool publish(const char* topic, const String& payload) {
    if (!client.connected()) {
        Serial.println(" MQTT client not connected. Publish failed.");
        return false;
    }
    if (drain_active) {
        Serial.println(" Drain active — skipping publish.");
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

bool publish_device_info() {
    DynamicJsonDocument doc(256);
    doc["MAC_ID"] = WiFi.macAddress();
    doc["IP"] = WiFi.localIP().toString();
    doc["CHIP_No"] = String((uint32_t)ESP.getEfuseMac());
    doc["DN"] = "WL";
    String jsonString; serializeJson(doc, jsonString);
    return publish(MQTT_PUBTOPIC, jsonString);
}
