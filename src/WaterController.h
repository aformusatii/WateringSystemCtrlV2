#pragma once

#include <Arduino.h>
#include <PCF8575.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <LittleFS.h>

struct RTCTimeStruct {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
};

#define CHANNEL_GPIO_RELAY_ON_PIN     2
#define CHANNEL_GPIO_MAIN_WATER_PIN   15

#define MAX_CHANNELS                  6
#define MAX_TIME_SLOTS                4

#define CONFIG_FILE_SLOTS             1
#define CONFIG_FILE_CHANNELS          2

#define CONFIG_FILE_CHANNELS_PATH     "/channels.json"

#define CHANNEL_MODE_SCHEDULE         1
#define CHANNEL_MODE_TIMER            2
#define CHANNEL_MODE_MANUAL_ON        3
#define CHANNEL_MODE_MANUAL_OFF       4

#define CHANNEL_GPIO_RELAY            1
#define CHANNEL_GPIO_MOSFET_LOW       2

class WaterController {
public:
    struct Schedule {
    	uint8_t index;
        uint8_t startHour;
        uint8_t startMinute;
        uint16_t durationMin;
    };

    struct Timer {
    	unsigned long onDurationMs;
    	unsigned long offDurationMs;
    	unsigned long nextSwitchTime;
    };

    struct Channel {
    	uint8_t index;
        String name;
        uint8_t mode;
        uint8_t gpio;
        uint8_t pin;
        Schedule schedules[MAX_TIME_SLOTS];
        Timer timer;
        bool open;
    };

    // -- Constructor/Init
    WaterController(PCF8575* pcf, uint8_t channelPinBase = 0);
    void begin();

    // -- Channel logic
    void channelsReadFromJson(const JsonArray& arr);
    void channelsWriteToJson(JsonArray& arr) const;

    void saveChannelsToFile();
    void applyChannelsToHardware();

    // -- Scheduler/update
    void updateSchedule(const RTCTimeStruct& now);

    // -- Timer/update
    void updateTimer(unsigned long);

private:
    PCF8575* _pcf;
    uint16_t pcfValues;
    uint8_t _channelPinBase;

    Channel channels[MAX_CHANNELS];

    void initializeChannels();
    bool isTimeInRange(uint8_t curH, uint8_t curM, uint8_t startH, uint8_t startM, uint16_t duration);

    // -- File system magic
    bool saveToFile(const char*, const uint8_t);
    bool loadFromFile(const char*, const uint8_t);
};
