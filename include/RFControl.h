#ifndef RF_CONTROL_H
#define RF_CONTROL_H

#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <ArduinoJson.h>

bool beginRa02();
void receiveJsonPayload(void *parameter);

#endif




