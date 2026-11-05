#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>

#include "configControl.h"
#include "deviceControl.h"
#include "bleControl.h"
#include "mqttControl.h"
#include "networkControl.h"
#include "otaControl.h"
#include "variables.h"
#include "RFControl.h"



String getFarmIdFromDeviceId(const char* deviceId) {
  if (deviceId == nullptr || strlen(deviceId) < 8) {
    return "0"; // Invalid device ID
  }

  char farmIdText[4];

  farmIdText[0] = deviceId[5];
  farmIdText[1] = deviceId[6];
  farmIdText[2] = deviceId[7];
  farmIdText[3] = '\0';

  return farmIdText;
}

bool beginRa02() {
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);

  LoRa.setPins(LORA_NSS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("RA-02 initialization failed");
    return false;
  }

  // These settings must exactly match on sender and receiver.
  LoRa.setSyncWord(LORA_SYNC_WORD);
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125000);
  LoRa.setCodingRate4(5);
  LoRa.enableCrc();

  Serial.println("RA-02 receiver ready");
  return true;
}



// Keep these outside the task function.
// They use normal ESP32 RAM, not LoRaReceiveTask stack.
char loraJsonBuffer[256];
StaticJsonDocument<384> loraDoc;

void receiveJsonPayload(void *parameter) {
  while (true) {
    int packetSize = LoRa.parsePacket();

    if (packetSize > 0) {
      size_t received = 0;

      while (LoRa.available() &&
             received < sizeof(loraJsonBuffer) - 1) {
        loraJsonBuffer[received++] = (char)LoRa.read();
      }

      loraJsonBuffer[received] = '\0';

      loraDoc.clear();

      DeserializationError error = deserializeJson(
        loraDoc,
        (const char *)loraJsonBuffer,
        received
      );

      if (error) {
        Serial.print("Invalid JSON: ");
        Serial.println(error.c_str());
      } else {
        const char *DN = loraDoc["DN"] | "unknown";
        const char *deviceId = loraDoc["Device_Id"] | "unknown";
        if (DN == "THC"|| DN == "FWD") {
            String farm_id = getFarmIdFromDeviceId(deviceId);
            String device_farm_id = String(FARM_ID);
            if (farm_id == device_farm_id) {

                String type = loraDoc["type"] | "unknown";

                if (DN == "THC") {
                    if (loraDoc["Etemp"] == 0 || loraDoc["Humidity"] == 0 || loraDoc["CO2"] == 0) {
                        Serial.println("Invalid sensor data, skipping publish.");
                        continue; // Skip publishing if any value is zero
                    }
                }

                if (DN == "FWD") {
                    if (loraDoc["waterLevel"] == 0) {
                        Serial.println("Invalid water level data, skipping publish.");
                        continue; // Skip publishing if water level is zero
                    }
                }

                String msgTopic;
                DynamicJsonDocument doc(256);
                    if (DN == "THC") {
                        doc["Etemp"]    = loraDoc["Etemp"] | 0.0;
                        doc["Humidity"] = loraDoc["Humidity"] | 0.0;
                        doc["CO2"] = loraDoc["CO2"] | 0.0;
                        doc["DN"] = "THC";
                    }if (DN == "FWD") {
                        doc["waterLevel"] = loraDoc["waterLevel"] | 0.0;
                        doc["DN"] = "FWD";
                    }
                    doc["Device_Id"] = loraDoc["Device_Id"] | "unknown";
                    doc["rack_id"] = loraDoc["rack_id"] | 0;
                    if (type == "sensor") {
                        msgTopic = "farm" + String(FARM_ID) + "sensor";
                        doc["Packet_Id"] = loraDoc["Packet_Id"] | "unknown";
                    }else {
                        msgTopic = "farm" + String(FARM_ID) + "sensor_keepalive";
                    }
                    
                    String jsonString;
                    serializeJson(doc, jsonString);
                    bool published = publish(msgTopic.c_str(), jsonString);
                    if (!published) {
                        Serial.println("MQTT publish failed.");
                    }
                    

            }
          
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}