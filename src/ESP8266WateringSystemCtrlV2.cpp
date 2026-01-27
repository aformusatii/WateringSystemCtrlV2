// Do not remove the include below
#include "ESP8266WateringSystemCtrlV2.h"
#include "vuejs.h"
#include <LittleFS.h>

WiFiHelper wiFiHelper;
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
ServerHelper serverHelper("WateringSystemCtrlV1", &server);
Logger logger;

#define GPIO_WATER_METER      D5
#define GPIO_WATER_SENSOR_A   D6
#define GPIO_WATER_SENSOR_B   D7

#define GPIO_PCF8575_SCL      D1
#define GPIO_PCF8575_SDA      D2

#define PIN_MIN               0
#define PIN_MAX               15

// Set i2c address
PCF8575 PCF(0x20);
DS3231 DS3231;
WaterController WaterCtrl(&PCF, 0);


#define SCHEDULE_CHECK_INTERVAL_MS         20000 // 20 seconds
unsigned long schedulerPreviousMillis = 0;

// File upload handler
File fsUploadFile; // a File object to temporarily store the upload


//The setup function is called once at startup of the sketch
void setup()
{
	Serial.begin(115200);
	Wire.begin();

	delay(100);

	// Initialize LittleFS
	if (!LittleFS.begin()) {
		Serial.println("Failed to mount LittleFS");
		return;
	}

	PCF.begin();
	WaterCtrl.begin();

	// Configures static IP address
	// https://docs.google.com/spreadsheets/d/15TZZKE4YDxZscbjAQP_kMxA631p_ueJwKnBA6VqtsIE/edit#gid=0
	if (!wiFiHelper.configureStaticIp(192, 168, 100, 18)) {
		logger.error("STA Failed to configure");
	}

	wiFiHelper.begin(LOCAL_SSID, LOCAL_KEY, setupAfterWiFiConnected);

	Serial.println("Started WateringSystemCtrlV1!");
}

/* ------------------------------------------------------------------------- */
/*  HTTP Hanlders  */
void indexPage() {
	server.sendHeader("Content-Encoding", "gzip");
	server.send_P(200, "text/html", reinterpret_cast<const char*>(webpage_gz), webpage_gz_len);
}

void vueJs() {
	server.sendHeader("Cache-Control", "public, max-age=15552000, immutable"); // ~6 months
	server.sendHeader("Content-Encoding", "gzip");
	server.send_P(200, "application/javascript", reinterpret_cast<const char*>(vuejs_gz), vuejs_gz_len);
}

void handleNotFound() {
	String path = server.uri();
	if (!handleFileRead(path)) {
		serverHelper.handleNotFound();
	}
}

void setPortState() {
	JsonDocument request;
    deserializeJson(request, server.arg("plain"));

    for (uint8_t i = PIN_MIN; i <= PIN_MAX; i++) {
    	char pinKey[6];
    	sprintf(pinKey, "p%d", i);

        if (request[pinKey].is<bool>()) {
        	bool channelValue = request[pinKey].as<bool>();

        	PCF.write(i, channelValue ? HIGH : LOW);
        }
    }

    writeJson(200, request);
}

void getPortState() {
	JsonDocument rootDoc;

	int portState = PCF.read16();

    for (uint8_t i = PIN_MIN; i <= PIN_MAX; i++) {
    	char pinKey[6];
    	sprintf(pinKey, "p%d", i);
    	rootDoc[pinKey] = (bitRead(portState, i) == 1);
    }

	writeJson(200, rootDoc);
}

void getTime() {
	JsonDocument rootDoc;

	bool h12, PM_time, century;

	rootDoc["year"] = DS3231.getYear();
	rootDoc["month"] = DS3231.getMonth(century);
	rootDoc["day"] = DS3231.getDate();
	rootDoc["hour"] = DS3231.getHour(h12, PM_time);
	rootDoc["minute"] = DS3231.getMinute();
	rootDoc["second"] = DS3231.getSecond();
	rootDoc["dayOfWeek"] = DS3231.getDoW();
	rootDoc["temperature"] = DS3231.getTemperature();

	writeJson(200, rootDoc);
}

void setTime() {
	JsonDocument request;
    deserializeJson(request, server.arg("plain"));

    byte year = (byte) request["year"];
    byte month = (byte) request["month"];
    byte day = (byte) request["day"];
    byte hour = (byte) request["hour"];
    byte minute = (byte) request["minute"];
    byte second = (byte) request["second"];

    year = year & 0xFF;
    month = month & 0xFF;
    day = day & 0xFF;
    hour = hour & 0xFF;
    minute = minute & 0xFF;
    second = second & 0xFF;

    DS3231.setClockMode(false); // set to 24h
    DS3231.setYear(year);
    DS3231.setMonth(month);
    DS3231.setDate(day);
    DS3231.setHour(hour);
    DS3231.setMinute(minute);
    DS3231.setSecond(second);

    request["ok"] = true;

    writeJson(200, request);
}

void getChannels() {
	JsonDocument responseJson;
	JsonArray responseJsonArray = responseJson.to<JsonArray>();
	WaterCtrl.channelsWriteToJson(responseJsonArray);

	writeJson(200, responseJson);
}

void setChannels() {
	JsonDocument requestJson; 
    deserializeJson(requestJson, server.arg("plain"));
    WaterCtrl.channelsReadFromJson(requestJson.as<JsonArray>());
    WaterCtrl.saveChannelsToFile();
    WaterCtrl.applyChannelsToHardware();

    writeJson(200, requestJson);
}

// ============================================== LittleFS Handlers ==============================================
void handleFileUpload() {
	HTTPUpload &upload = server.upload();

	if (upload.status == UPLOAD_FILE_START) {
		String filename = upload.filename;
		if (!filename.startsWith("/"))
			filename = "/" + filename;

		Serial.print("Upload Start: ");
		Serial.println(filename);

		fsUploadFile = LittleFS.open(filename, "w");
	} else if (upload.status == UPLOAD_FILE_WRITE) {
		// Write the bytes to the file
		if (fsUploadFile) {
			fsUploadFile.write(upload.buf, upload.currentSize);
		}

	} else if (upload.status == UPLOAD_FILE_END) {
		// Finish up
		if (fsUploadFile) {
			fsUploadFile.close();
			Serial.print("Upload End: ");
			Serial.print(upload.totalSize);
			Serial.println(" bytes");

			server.send(200, "text/plain", "Upload OK");
		} else {
			Serial.println("Upload Failed");
			server.send(500, "text/plain", "Upload Failed");
		}

	} else if (upload.status == UPLOAD_FILE_ABORTED) {
		// Abort
		if (fsUploadFile) {
			fsUploadFile.close();
			LittleFS.remove(upload.filename);
		}

		Serial.println("Upload Aborted");
	}
}

void handleFileDelete() {
	// Check if 'filename' parameter exists
	if (!server.hasArg("filename")) {
		server.send(400, "text/plain", "Bad Request: 'filename' param missing");
		return;
	}

	String filename = server.arg("filename");
	// LittleFS expects filenames to start with '/'
	if (!filename.startsWith("/")) {
		filename = "/" + filename;
	}

	// Check if file exists
	if (!LittleFS.exists(filename)) {
		server.send(404, "text/plain", "File Not Found");
		return;
	}

	// Try deleting
	if (LittleFS.remove(filename)) {
		server.send(200, "text/plain", "File deleted successfully");
	} else {
		server.send(500, "text/plain", "File deletion failed");
	}
}

void handleFilesRead() {
	// Estimate capacity for up to 20 files; adjust if needed!
	JsonDocument doc;

	JsonArray files = doc["files"].to<JsonArray>();

	Dir dir = LittleFS.openDir("/");

	while (dir.next()) {
		JsonObject file = files.add<JsonObject>();
		file["name"] = dir.fileName();
		file["size"] = dir.fileSize();
	}

	writeJson(200, doc);
}

String getContentType(String filename) { // convert the file extension to the MIME type
  if (filename.endsWith(".html")) return "text/html";
  else if (filename.endsWith(".css")) return "text/css";
  else if (filename.endsWith(".json")) return "application/json";
  else if (filename.endsWith(".js")) return "application/javascript";
  else if (filename.endsWith(".ico")) return "image/x-icon";
  else if (filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path) {
	String prefix = "/file";
	if (!path.startsWith(prefix)) {
		return false;
	}

	// Extract filename from URI, e.g., /file/hello.txt -> /hello.txt
	String filename = path.substring(prefix.length());
	if (filename.length() == 0) {
		return false;
	}

	String fullPath = filename;

	if (!LittleFS.exists(fullPath)) {
		return false;
	}

	File file = LittleFS.open(fullPath, "r");
	if (!file) {
		server.send(500, "text/plain", "Failed to open file");
		return true;
	}

	String contentType = getContentType(filename);
	server.streamFile(file, contentType);

	file.close();
	return true;
}

void handleSpiffsInfo() {
  FSInfo fs_info;
  if (LittleFS.info(fs_info)) {
    size_t total = fs_info.totalBytes;
    size_t used = fs_info.usedBytes;
    size_t free = (total > used) ? (total - used) : 0;

    JsonDocument doc;
    doc["total"] = total;
    doc["used"]  = used;
    doc["free"]  = free;

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
  } else {
    server.send(500, "application/json", "{\"error\":\"LittleFS.info() failed\"}");
  }
}
// ============================================== End LittleFS Handlers ==============================================

void writeJson(int httpStatus, const JsonDocument &doc) {
	String output;
	serializeJson(doc, output);
	server.send(httpStatus, "application/json", output);
}

void setupHTTPActions() {
	server.on("/", HTTP_GET, indexPage);
	server.on("/vue.js", HTTP_GET, vueJs);

	server.on("/channels.json", HTTP_GET, getChannels);
	server.on("/channels.json", HTTP_POST, setChannels);

	server.on("/DS3231.json", HTTP_GET, getTime);
	server.on("/DS3231.json", HTTP_POST, setTime);

	server.on("/files", HTTP_GET, handleFilesRead);
	server.on("/delete", HTTP_GET, handleFileDelete);
	server.on("/upload", HTTP_POST, []() {
		server.send(200, "text/html","<h2>Upload Done. <a href='/'>Go Back</a></h2>");
	}, handleFileUpload);
	server.on("/spiffsinfo", HTTP_GET, handleSpiffsInfo);

	// Not found handler
	server.onNotFound(handleNotFound);

	// Setup HTTP Updater
	httpUpdater.setup(&server);

	server.begin();
}
/* ------------------------------------------------------------------------- */

void setupAfterWiFiConnected() {
	JsonDocument rootDoc;
	//rootDoc["IP"] = WiFi.localIP();
	logger.info(rootDoc, "Connected to WiFi");

	Serial.println(WiFi.localIP());

	setupHTTPActions();
}

// The loop function is called in an endless loop
void loop()
{
	unsigned long current_time = millis();

	wiFiHelper.loop(current_time);

	if (wiFiHelper.wiFiOk) {
		server.handleClient();
	}

    // Check if interval has passed
    if ((current_time - schedulerPreviousMillis) >= SCHEDULE_CHECK_INTERVAL_MS) {
    	schedulerPreviousMillis = current_time;

    	bool h12, PM_time;
        RTCTimeStruct now;
        now.hour = DS3231.getHour(h12, PM_time);
        now.minute = DS3231.getMinute();
        now.second = DS3231.getSecond();

        WaterCtrl.updateSchedule(now);
    }

    WaterCtrl.updateTimer(current_time);
}

