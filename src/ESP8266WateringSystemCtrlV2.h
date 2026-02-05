// Only modify this file to include
// - function definitions (prototypes)
// - include files
// - extern variable definitions
// In the appropriate section

#ifndef _ESP8266WateringSystemCtrlV1_H_
#define _ESP8266WateringSystemCtrlV1_H_
#include "Arduino.h"
//add your includes for the project ESP8266WateringSystemCtrlV1 here
#include "WiFiHelper.h"
#include "Logger.h"

#include <secrets.h>
#include <ESP8266HTTPUpdateServer.h>

// #define NOT_SEQUENTIAL_PINOUT
#include "PCF8575.h"
#include "DS3231.h"
#include "webpage.h"
#include "WaterController.h"

//end of add your includes here


//add your function definitions for the project ESP8266WateringSystemCtrlV1 here
void setupAfterWiFiConnected();
void setupGPIO();

void handleWaterMeterPortChange();

void indexPage();
void handleNotFound();
void writeJson(int httpStatus, const JsonDocument &doc);
void setupHTTPActions();

void setPortState();
void getPortState();
void getChannels();
void setChannels();
void getTimeSlots();
void setTimeSlots();
void getTime();
void setTime();
void getPinState();
void setPinState();

void handleFilesRead();
bool handleFileRead(String);
void handleFileUpload();
void handleFileDelete();
void handleSpiffsInfo();
void getHardwareInfo();

//Do not add code below this line
#endif /* _ESP8266WateringSystemCtrlV1_H_ */
