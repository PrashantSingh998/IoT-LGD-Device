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
StaticJsonDocument<384> mqttDoc;
// void receiveJsonPayload(void *parameter) {
//   while (true) {
//     int packetSize = LoRa.parsePacket();

//     if (packetSize > 0) {
//       size_t received = 0;

//       while (LoRa.available() &&
//              received < sizeof(loraJsonBuffer) - 1) {
//         loraJsonBuffer[received++] = (char)LoRa.read();
//       }

//       loraJsonBuffer[received] = '\0';

//       loraDoc.clear();

//       DeserializationError error = deserializeJson(
//         loraDoc,
//         (const char *)loraJsonBuffer,
//         received
//       );

//       if (error) {
//         Serial.print("Invalid JSON: ");
//         Serial.println(error.c_str());
//       } else {
//         Serial.print("Received JSON: ");
//         Serial.println(loraJsonBuffer);
//         const char *DN = loraDoc["DN"] | "unknown";
//         const char *deviceId = loraDoc["Device_Id"] | "unknown";
//         if (DN == "THC"|| DN == "FWD") {
//             Serial.println("1");
//             String farm_id = getFarmIdFromDeviceId(deviceId);
//             String device_farm_id = String(FARM_ID);
//             if (farm_id == device_farm_id) {
//                 Serial.println("2");

//                 String type = loraDoc["type"] | "unknown";

//                 if (DN == "THC") {
//                     Serial.println("3");
//                     if (loraDoc["Etemp"] == 0 || loraDoc["Humidity"] == 0 || loraDoc["CO2"] == 0) {
//                         Serial.println("Invalid sensor data, skipping publish.");
//                         continue; // Skip publishing if any value is zero
//                     }
//                 }

//                 if (DN == "FWD") {
//                     Serial.println("4");
//                     if (loraDoc["waterLevel"] == 0) {
//                         Serial.println("Invalid water level data, skipping publish.");
//                         continue; // Skip publishing if water level is zero
//                     }
//                 }

//                 String msgTopic;
//                 DynamicJsonDocument doc(256);
//                     if (DN == "THC") {
//                         Serial.println("5");
//                         doc["Etemp"]    = loraDoc["Etemp"] | 0.0;
//                         doc["Humidity"] = loraDoc["Humidity"] | 0.0;
//                         doc["CO2"] = loraDoc["CO2"] | 0.0;
//                         doc["DN"] = "THC";
//                     }if (DN == "FWD") {
//                         Serial.println("6");
//                         doc["waterLevel"] = loraDoc["waterLevel"] | 0.0;
//                         doc["DN"] = "FWD";
//                     }
//                     doc["Device_Id"] = loraDoc["Device_Id"] | "unknown";
//                     doc["rack_id"] = loraDoc["rack_id"] | 0;
//                     if (type == "sensor") {

//                         Serial.println("7");
//                         msgTopic = "farm" + String(FARM_ID) + "sensor";
//                         doc["Packet_Id"] = loraDoc["Packet_Id"] | "unknown";
//                     }else {
//                         Serial.println("8");
//                         msgTopic = "farm" + String(FARM_ID) + "sensor_keepalive";
//                     }
                    
//                     String jsonString;
//                     serializeJson(doc, jsonString);
//                     Serial.println("9");
//                     bool published = publish(msgTopic.c_str(), jsonString);
//                     if (!published) {
//                         Serial.println("MQTT publish failed.");
//                     }
                    

//             }
          
//         }
//       }
//     }

//     vTaskDelay(pdMS_TO_TICKS(10));
//   }
// }


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
        Serial.print("Invalid LoRa JSON: ");
        Serial.println(error.c_str());

      } else {
        Serial.print("Received JSON: ");
        Serial.println(loraJsonBuffer);

        const char *dn = loraDoc["DN"] | "";
        const char *deviceId = loraDoc["Device_Id"] | "";

        bool isTHC = strcmp(dn, "THC") == 0;
        bool isFWD = strcmp(dn, "FWD") == 0;

        if (!isTHC && !isFWD) {
          Serial.println("Unsupported DN. Skipping packet.");

        } else {
          String farmId = getFarmIdFromDeviceId(deviceId);

          if (farmId != String(FARM_ID)) {
            Serial.println("Farm ID does not match. Skipping packet.");

          } else {
            String type = loraDoc["type"] | "sensor";
            bool validPayload = true;

            mqttDoc.clear();

            mqttDoc["DN"] = dn;
            mqttDoc["Device_Id"] = deviceId;
            mqttDoc["rack_id"] = loraDoc["rack_id"] | 0;

            // ---------- THC ----------
            if (isTHC) {
              float etemp = loraDoc["Etemp"] | 0.0;
              float humidity = loraDoc["Humidity"] | 0.0;
              float co2 = loraDoc["CO2"] | 0.0;

              if (etemp == 0 || humidity == 0 || co2 == 0) {
                Serial.println("Invalid THC data.");
                validPayload = false;
              } else {
                mqttDoc["Etemp"] = etemp;
                mqttDoc["Humidity"] = humidity;
                mqttDoc["CO2"] = co2;
              }
            }

            // ---------- FWD ----------
            if (isFWD) {
              // Payload key is WaterLevel, capital W and L.
              float waterLevel = loraDoc["WaterLevel"] | 0.0;

              if (waterLevel == 0) {
                Serial.println("Invalid FWD water level.");
                validPayload = false;
              } else {
                mqttDoc["WaterLevel"] = waterLevel;

                if (loraDoc.containsKey("waterLevel_height")) {
                  mqttDoc["waterLevel_height"] =
                    loraDoc["waterLevel_height"];
                }
              }
            }

            if (validPayload) {
              String msgTopic;

              if (type == "sensor") {
                msgTopic = "farm/" + String(FARM_ID) + "/sensor";

                if (loraDoc.containsKey("Packet_Id")) {
                  mqttDoc["Packet_Id"] = loraDoc["Packet_Id"];
                }

              } else if (type == "keepalive") {
                msgTopic =
                  "farm/" + String(FARM_ID) + "/sensor_keepalive";

                if (loraDoc.containsKey("WIFI_RSSI")) {
                  mqttDoc["WIFI_RSSI"] = loraDoc["WIFI_RSSI"];
                }

              } else {
                Serial.println("Unknown packet type.");
                validPayload = false;
              }

              if (validPayload) {
                String jsonString;
                serializeJson(mqttDoc, jsonString);

                Serial.print("Publishing: ");
                Serial.println(jsonString);

                bool published = publish(
                  msgTopic.c_str(),
                  jsonString
                );

                if (!published) {
                  Serial.println("MQTT publish failed.");
                }
              }
            }
          }
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}