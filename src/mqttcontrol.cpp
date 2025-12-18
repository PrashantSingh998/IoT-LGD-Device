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
#include <esp_system.h>   // <-- for esp_random()

WiFiClient espClient;
PubSubClient client(espClient);

// define drain_active exactly once here
volatile bool drain_active = false;

static const char* WL_PREF_NAMESPACE = "wl_ns";
// threshold in liters for change-publish
static constexpr float WL_DELTA = 20.0f;

// Hardcoded device id
const char* DEVICE_ID = "IFWLD1180000001";

// NEW: track last hourly publish time (for 1 hr forced publish)
static unsigned long lastHourlyPublishMs = 0;

// UUID v4 generator for Packet_Id
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

bool publish_water_level_if_changed(float waterLevel) {
    if (!client.connected()) {
        Serial.println("MQTT client not connected. publish_water_level_if_changed skipped.");
        return false;
    }
    if (drain_active) {
        Serial.println("Drain active — skipping change-publish.");
        return false;
    }

    // ----- existing 20 L logic -----
    prefs.begin(WL_PREF_NAMESPACE, false);
    float last = prefs.getFloat("lastWL", NAN);
    bool shouldPublish = false;
    if (isnan(last)) {
        shouldPublish = true; // first-time publish
    } else {
        float diff = fabsf(waterLevel - last);
        if (diff >= WL_DELTA) shouldPublish = true;
    }

    // ----- NEW: 1-hour forced publish -----
    unsigned long now = millis();
    const unsigned long ONE_HOUR_MS = 3600000UL; // 1 hour
    bool oneHourElapsed = (lastHourlyPublishMs == 0) ||
                          (now - lastHourlyPublishMs >= ONE_HOUR_MS);

    if (!shouldPublish && !oneHourElapsed) {
        // same behaviour as before when neither condition is met
        Serial.println("No significant change in water level — skipping publish.");
        prefs.end();
        return false;
    }

    if (!shouldPublish && oneHourElapsed) {
        Serial.println("1 hour elapsed — forcing periodic publish on PUB topic.");
    }

    // Build payload (same JSON, whether 20L-change or hourly)
    DynamicJsonDocument doc(256);
    doc["DN"] = "WL";
    doc["WaterLevel"] = waterLevel;
    doc["Packet_Id"] = generateUUID();
    doc["Device_Id"] = DEVICE_ID;

    String payload; serializeJson(doc, payload);

    bool success = client.publish(MQTT_PUBTOPIC, payload.c_str());
    if (success) {
        Serial.println("Change/Hourly publish successful.");
        Serial.print("Payload: "); Serial.println(payload);
        prefs.putFloat("lastWL", waterLevel);   // update last published value
        lastHourlyPublishMs = now;              // update hourly timer
    } else {
        Serial.println("Change/Hourly publish failed!");
    }
    prefs.end();
    return success;
}

// Keepalive: publish current volume (call every 60s from your loop/timer)
// Returns true on successful publish, false otherwise.
bool publish_keepalive_water_level(float waterLevel) {
    if (!client.connected()) {
        Serial.println("MQTT client not connected. keepalive skipped.");
        return false;
    }
    if (drain_active) {
        Serial.println("Drain active — keepalive skipped.");
        return false;
    }

    DynamicJsonDocument doc(128);
    doc["DN"] = "WL";
    doc["WaterLevel"] = waterLevel;
    doc["Device_Id"] = DEVICE_ID;   // you already had this in your last paste
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
    doc["DN"] = "WL";
    // NEW: add Packet_Id and Device_Id for device-info packet
   ;

    String jsonString; serializeJson(doc, jsonString);
    return publish(MQTT_INFOTOPIC, jsonString);
}
