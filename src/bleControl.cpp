#include "configControl.h"
#include "deviceControl.h"
#include "bleControl.h"
#include "mqttControl.h"
#include "networkControl.h"
#include "otaControl.h"
#include <WebServer.h>
#include <ArduinoJson.h>
#include "variables.h"
#include <ESPmDNS.h>

WebServer server(80);

void initroutes() {
    if (!MDNS.begin(DEVICE_ID)) {   // Set the hostname to "esp32.local"
        Serial.println("Error setting up MDNS responder!");
        while(1) {
        delay(1000);
        }
    }
    Serial.println("mDNS responder started");
    server.on("/status", HTTP_GET, handleGetStatus);
    server.on("/config/wifi", HTTP_POST, handlewifiConfig);
    server.on("/config/calibration", HTTP_POST, handlecalibrationConfig);
    server.onNotFound(handleNotFound);

    server.begin();
    Serial.println("HTTP Server Started");
}


void handleGetStatus() {
    float pubMin = prefs.getFloat("pubInt", 30.0);
    DynamicJsonDocument doc(512);
    doc["wifi_ssid"] = prefs.getString("ssid", "fec");
    doc["wifi_pass"] = prefs.getString("pass", "Innofarm1008");
    doc["farm_id"] = prefs.getInt("Farmid", 1193);
    doc["Device_ID"] = DEVICE_ID;
    doc["mqtt_sub"]    = prefs.getString("subtop", "Sub");
    doc["mqtt_pub"]    = prefs.getString("pubtop", "sensor");
    doc["mqtt_keep"]   = prefs.getString("keeptopic", "sensor_keepalive");
    doc["mqtt_resp"]   = prefs.getString("resptop", "Response");
    doc["mqtt_info"]   = prefs.getString("infotop", "info");
    doc["tank_volume"] = prefs.getFloat("tankV");
    doc["tank_height"] = prefs.getFloat("tankH");
    doc["tank_blind"]  = prefs.getFloat("blind");
    doc["Higher_Limit"] = prefs.getFloat("HL");
    doc["Lower_Limit"] = prefs.getFloat("LL");
    doc["publish_interval_min"] = pubMin;

    String response;
    serializeJson(doc, response);

    server.send(200, "application/json", response);
    Serial.println("Sent status response via HTTP"+response);
}

void handlewifiConfig() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No body received\"}");
        return;
    }
    String body = server.arg("plain");

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    if (doc.containsKey("WIFI_SSID")) {
        String ssid1 = doc["WIFI_SSID"];
        prefs.putString("ssid", ssid1);
    }

    if (doc.containsKey("WIFI_PASSWORD")) {
        String password1 = doc["WIFI_PASSWORD"];
        prefs.putString("pass", password1);
    }

    Serial.println("POST Data Received:");
    Serial.println(body);

    server.send(200, "application/json", "{\"message\":\"Configuration updated\"}");
    delay(500);
    ESP.restart();
}




void handlecalibrationConfig() {
    if (!server.hasArg("plain")) {
        server.send(400, "application/json", "{\"error\":\"No body received\"}");
        return;
    }
    String body = server.arg("plain");

    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, body);

    if (error) {
        server.send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    if (doc.containsKey("FARM_ID")) {
        int farm_ids = doc["FARM_ID"];
        prefs.putInt("Farmid", farm_ids);
    }

    if (doc.containsKey("tank_Volume")) {
        float tank_volume = doc["tank_Volume"];
        prefs.putFloat("tankV", tank_volume);
    }

    if (doc.containsKey("tank_height")) {
        float tank_height = doc["tank_height"];
        prefs.putFloat("tankH", tank_height);
    }

    if (doc.containsKey("blind_spot")) {
        float blind_spot = doc["blind_spot"];
        prefs.putFloat("blind", blind_spot);
    }
    if (doc.containsKey("HL")) {
        Higher_Limit = doc["HL"];
        prefs.putFloat("HL", Higher_Limit);
    }
    if (doc.containsKey("LL")) {
        Lower_Limit = doc["LL"];
        prefs.putFloat("LL", Lower_Limit);
    }

    Serial.println("POST Data Received:");
    Serial.println(body);

    server.send(200, "application/json", "{\"message\":\"Configuration updated\"}");
    delay(500);
    ESP.restart();
}


void handleNotFound() {
  server.send(404, "text/plain", "404: Route Not Found");
}


void httploop() {
    server.handleClient();
}   