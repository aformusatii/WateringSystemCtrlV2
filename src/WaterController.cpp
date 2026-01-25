#include "WaterController.h"

WaterController::WaterController(PCF8575* pcf, uint8_t channelPinBase)
    : _pcf(pcf), _channelPinBase(channelPinBase), pcfValues(0)
{
}

void WaterController::begin() {
	initializeChannels();
    loadFromFile(CONFIG_FILE_CHANNELS_PATH, CONFIG_FILE_CHANNELS);
    applyChannelsToHardware();
}

void WaterController::initializeChannels() {
	// === Channel 1 ===
	channels[0].index = 0;
    channels[0].name = "CH1";
    channels[0].mode = CHANNEL_MODE_MANUAL_OFF;
    channels[0].gpio = CHANNEL_GPIO_RELAY;
    channels[0].pin = 14;

	// === Channel 2 ===
	channels[1].index = 1;
    channels[1].name = "CH2";
    channels[1].mode = CHANNEL_MODE_MANUAL_OFF;
    channels[1].gpio = CHANNEL_GPIO_RELAY;
    channels[1].pin = 13;

	// === Channel 3 ===
	channels[2].index = 2;
    channels[2].name = "CH3";
    channels[2].mode = CHANNEL_MODE_MANUAL_OFF;
    channels[2].gpio = CHANNEL_GPIO_RELAY;
    channels[2].pin = 12;

	// === Channel 4 ===
	channels[3].index = 3;
    channels[3].name = "CH4";
    channels[3].mode = CHANNEL_MODE_MANUAL_OFF;
    channels[3].gpio = CHANNEL_GPIO_RELAY;
    channels[3].pin = 11;

	// === Channel 5 ===
	channels[4].index = 4;
    channels[4].name = "CH5";
    channels[4].mode = CHANNEL_MODE_MANUAL_OFF;
    channels[4].gpio = CHANNEL_GPIO_RELAY;
    channels[4].pin = 10;

	// === Channel 6 ===
	channels[5].index = 5;
    channels[5].name = "CH6";
    channels[5].mode = CHANNEL_MODE_MANUAL_OFF;
    channels[5].gpio = CHANNEL_GPIO_RELAY;
    channels[5].pin = 0;

    for (uint8_t ch = 0; ch < MAX_CHANNELS; ++ch) {
    	channels[ch].open = false;
    	channels[ch].timer.offDurationMs = 5000;
    	channels[ch].timer.onDurationMs = 5000;
    	channels[ch].timer.nextSwitchTime = 0;

    	for (uint8_t timeSlotIndex = 0; timeSlotIndex < MAX_TIME_SLOTS; ++timeSlotIndex) {
    		channels[ch].schedules[timeSlotIndex].index = timeSlotIndex;
    		channels[ch].schedules[timeSlotIndex].startHour = 5;
    		channels[ch].schedules[timeSlotIndex].startMinute = 30;
    		channels[ch].schedules[timeSlotIndex].durationMin = 30;
    	}
    }
}

bool WaterController::isTimeInRange(
    uint8_t currentHour,
    uint8_t currentMinute,
    uint8_t slotStartHour,
    uint8_t slotStartMinute,
    uint16_t slotDurationMinutes)
{
    // Convert times to "minutes since midnight"
    uint16_t slotStart = slotStartHour * 60 + slotStartMinute;
    uint16_t slotEnd   = slotStart + slotDurationMinutes;
    uint16_t now = currentHour * 60 + currentMinute;

    // Return true if current time is within the range
    return (now >= slotStart) && (now < slotEnd);
}

void WaterController::updateSchedule(const RTCTimeStruct& now)
{
	bool changed = false;
    for (uint8_t ch = 0; ch < MAX_CHANNELS; ++ch) {
    	if (channels[ch].mode == CHANNEL_MODE_SCHEDULE) {
			// For each schedule time slot
			bool timeMatch = false;

			for (uint8_t timeSlotIndex = 0; timeSlotIndex < MAX_TIME_SLOTS; ++timeSlotIndex) {
				Schedule schedule = channels[ch].schedules[timeSlotIndex];

				if (isTimeInRange(now.hour, now.minute, schedule.startHour, schedule.startMinute, schedule.durationMin)) {
					timeMatch = true;
				}
			}

			if (timeMatch) {
				changed = !channels[ch].open;
				channels[ch].open = true;
			} else {
				changed = channels[ch].open;
				channels[ch].open = false;
			}
    	}
    }

    if (changed) {
    	applyChannelsToHardware();
    }
}

void WaterController::updateTimer(unsigned long current_time)
{
	bool changed = false;
    for (uint8_t ch = 0; ch < MAX_CHANNELS; ++ch) {
    	if (channels[ch].mode == CHANNEL_MODE_TIMER) {
			Timer timer = channels[ch].timer;

			if (channels[ch].open) {
				if ((current_time - timer.nextSwitchTime) >= timer.onDurationMs) {
					channels[ch].timer.nextSwitchTime = current_time;
					channels[ch].open = false;
					changed = true;
				}
			} else {
				if ((current_time - timer.nextSwitchTime) >= timer.offDurationMs) {
					channels[ch].timer.nextSwitchTime = current_time;
					channels[ch].open = true;
					changed = true;
				}
			}
    	}
    }

    if (changed) {
    	applyChannelsToHardware();
    }
}

void WaterController::applyChannelsToHardware()
{
	bool haveChannelsOpen = false;
    for (uint8_t ch = 0; ch < MAX_CHANNELS; ++ch) {
    	switch (channels[ch].gpio) {
    		case CHANNEL_GPIO_RELAY:
    	        if (channels[ch].open) {
    	        	pcfValues &= ~(1 << (channels[ch].pin));
    	        	haveChannelsOpen = true;
    	        } else {
    	        	pcfValues |= (1 << (channels[ch].pin));
    	        }

    			break;

    		case CHANNEL_GPIO_MOSFET_LOW:
    	        if (channels[ch].open) {
    	        	pcfValues |= (1 << (channels[ch].pin));
    	        	haveChannelsOpen = true;
    	        } else {
    	        	pcfValues &= ~(1 << (channels[ch].pin));
    	        }

    			break;
    	}
    }

    // The relay board is actually enabled by dedicated pin
    pcfValues |= (1 << CHANNEL_GPIO_RELAY_ON_PIN);

    if (haveChannelsOpen) {
    	pcfValues &= ~(1 << CHANNEL_GPIO_MAIN_WATER_PIN);
    } else {
    	pcfValues |= (1 << CHANNEL_GPIO_MAIN_WATER_PIN);
    }

    _pcf->write16(pcfValues);
}

// ==== CHANNELS LOAD/SAVE ====

// For HTTP POST: Load channel states from JSON
void WaterController::channelsReadFromJson(const JsonArray& arr)
{
    uint8_t ch = 0;

    for (JsonObject obj : arr) {
        if (ch >= MAX_CHANNELS) break;

        if (obj.containsKey("name")) {
        	channels[ch].name = (String) obj["name"];
        }

        if (obj.containsKey("mode")) {
        	channels[ch].mode = obj["mode"];

        	switch (channels[ch].mode) {
				case CHANNEL_MODE_MANUAL_ON:
					channels[ch].open = true;
					break;

				case CHANNEL_MODE_MANUAL_OFF:
					channels[ch].open = false;
					break;
        	}
        }

        if (obj.containsKey("schedules")) {
        	uint8_t timeSlotIndex = 0;
        	JsonArray schedules = (JsonArray) obj["schedules"];

        	for (JsonObject schedule : schedules) {
        		if (schedule.containsKey("startHour")
        				&& schedule.containsKey("startMinute")
						&& schedule.containsKey("durationMin")) {
            		channels[ch].schedules[timeSlotIndex].startHour = schedule["startHour"];
            		channels[ch].schedules[timeSlotIndex].startMinute = schedule["startMinute"];
            		channels[ch].schedules[timeSlotIndex].durationMin = schedule["durationMin"];
        		}
        		timeSlotIndex++;
        	}
        }

        if (obj.containsKey("timer")) {
        	JsonObject timer = obj["timer"];
        	if (timer.containsKey("onDurationMs") && timer.containsKey("offDurationMs")) {
            	channels[ch].timer.onDurationMs = timer["onDurationMs"];
            	channels[ch].timer.offDurationMs = timer["offDurationMs"];
        	}
        }

        ch++;
    }
}

// For HTTP GET: Save channel states as JSON
void WaterController::channelsWriteToJson(JsonArray& arr) const
{
    for (uint8_t ch = 0; ch < MAX_CHANNELS; ++ch) {
        JsonObject c = arr.createNestedObject();
        c["name"] = channels[ch].name;
        c["mode"] = channels[ch].mode;

        JsonObject timer = c.createNestedObject("timer");
        timer["onDurationMs"] = channels[ch].timer.onDurationMs;
        timer["offDurationMs"] = channels[ch].timer.offDurationMs;

        JsonArray schedules = c.createNestedArray("schedules");
		for (uint8_t timeSlotIndex = 0; timeSlotIndex < MAX_TIME_SLOTS; ++timeSlotIndex) {
			Schedule schedule = channels[ch].schedules[timeSlotIndex];

			JsonObject scheduleJson = schedules.createNestedObject();
			scheduleJson["startHour"] = schedule.startHour;
			scheduleJson["startMinute"] = schedule.startMinute;
			scheduleJson["durationMin"] = schedule.durationMin;
		}
    }
}

// ==== PERSISTENCE ON SPIFFS ====

// Load from JSON file
bool WaterController::loadFromFile(const char* path, const uint8_t configType) {
    File file = SPIFFS.open(path, "r");

    if (!file || file.size() == 0) {
        if (file) file.close();
        return false;
    }

    StaticJsonDocument<2048> doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    if (err) return false;

    JsonArray arr = doc.as<JsonArray>();

    switch (configType) {
    	case CONFIG_FILE_CHANNELS:
    		channelsReadFromJson(arr);
    		break;
    }

    return true;
}

// Save to JSON file
bool WaterController::saveToFile(const char* path, const uint8_t configType) {
    StaticJsonDocument<2048> doc;
    JsonArray arr = doc.to<JsonArray>();

    switch (configType) {
    	case CONFIG_FILE_CHANNELS:
    		channelsWriteToJson(arr);
    		break;
    }


    File f = SPIFFS.open(path, "w");
    if (!f) return false;

    bool ok = (serializeJson(doc, f) > 0);
    f.close();
    return ok;
}

void WaterController::saveChannelsToFile() {
	saveToFile(CONFIG_FILE_CHANNELS_PATH, CONFIG_FILE_CHANNELS);
}
