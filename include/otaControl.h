#ifndef OTA_CONTROL_H
#define OTA_CONTROL_H

#include <Arduino.h>

void checkAndUpdateFirmware(const String &versionUrl,const char * currentVersion );
void handleOTA(); // if you use it elsewhere

#endif
