// Wifi Duet3D monitor for the M5Stack core.

// Expects a SD card with config file named /duet_buddy.json
// and fields like these:
//
// {
//   "wifi_ssid" : "xxx",
//   "wifi_password" : "yyy",
//   "duet_ip" : "10.20.30.40"
// }
//
// The program polls the duet at the url
// http://xx.xx.xx.xx/rr_status?type=3
// and displayes selected values such as
// state and progress.
//
// Arduino IDE configuration:
// --------------------------
// Board:             M5Stack-Core-ESP32
// Upload Speed:      921600
// Flash Frequencey:  80Mhz
// Flash Mode:        QIO
// Partition Schema:  No OTA (Large APP)
// Core Debug Level:  None


// TODO: add beeping in pause mode.
// TODO: filter out sporadic connection/response issues.

#include <Arduino.h>
#include "duet_parser.h"
#include "config_parser.h"
#include <M5Stack.h>
#include <stdint.h>
#include <WiFi.h>
#include <FS.h>
#include <SD.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>

// 16 bits RGB565 colors. Picked using
// http://www.barth-dev.de/online/rgb565-color-picker/

static const uint16_t kBlack  = 0x0000;
static const uint16_t kBlue   = 0x001F;
static const uint16_t kRed    = 0xF800;
static const uint16_t kYellow = 0xFFE0;
static const uint16_t kGreen  = 0x07E0;
static const uint16_t kPurple = 0x801F;
static const uint16_t kGray   = 0xC618;

static WiFiMulti wifiMulti;
static HTTPClient http;

static JsonParser json_parser;
static DuetParser duet_parser;
// Stores configuration parsed from SD file.
static ConfigParser config_parser;
static SimpleString<80> duet_url;

// Per duet status char screen configurations.
// Colors are 16 bit RGB565 format.
struct StatusConfig {
  // The status char as returned by dutet. The special
  // char '*' indicates default catch all terminator.
  const char c;
  // User friendly status name.
  const char* text;
  const bool display_progress;
  const bool display_temps;
  const bool display_z;
  const uint16_t bg_color;
  const uint16_t text_color;
};

static const StatusConfig status_configs[] = {
  {'A', "PAUSED", true, true, false, kPurple, kBlack},
  {'B', "BUSY", false, true, false, kRed, kBlack},
  {'C', "CONFIGURING", false,  false, false, kYellow, kBlack},
  {'D', "PAUSING", true, true, false, kYellow, kBlack},
  {'F', "FLASHING", false, false, false, kYellow, kBlack},
  {'I', "IDLE", false, true, false,  kGreen, kBlack},  // fix
  {'P', "PRINTING", true, true, true, kRed, kBlack},
  {'R', "RESUMING", true, true, false, kYellow, kBlack},
  {'S', "PAUSED", true, true, false, kYellow, kBlack},
  // Default terminator. Must be last.
  {'*', "[unknown]", false, false, false, kGray, kBlack}
};

// Finds the configuration for a given duet status char.
static const StatusConfig& decodeStatusChar(char c) {
  const StatusConfig* p = status_configs;
  for (;;) {
    // Actual match or a catch all default terminator.
    if (p->c == c || p->c == '*') {
      return *p;
    }
    p++;
  }
}

// Common helper for text screens.
static void initTextScreen() {
  M5.Lcd.fillScreen(kBlue);
  M5.Lcd.setTextColor(kYellow, kBlue);
  M5.Lcd.setTextSize(2);
  M5.Lcd.setCursor(0, 12);
}

static bool fatal_error = false;
static bool wifi_connected = false;

// Common rendering for all fatal error messages.
static void drawFatalErrorScreen(const char* msg) {
  fatal_error = true;
  initTextScreen();
  M5.Lcd.print(" FATAL ERROR:\n\n ");
  M5.Lcd.print(msg);
}

static void drawNoWifiScreen() {
  wifi_connected = false;
  initTextScreen();
  M5.Lcd.printf(" Connecting to WIFI.\n\n\n SSID: [%s]",
                config_parser.ParsedData().wifi_ssid.c_str());
}

static void drawWifiConnectedScreen() {
  wifi_connected = true;
  initTextScreen();
  M5.Lcd.printf(" Wifi connected.\n\n\n Connecting to Duet.\n\n\n IP: [%s]",
                config_parser.ParsedData().duet_ip.c_str());
}

static void drawNoHttpConnectionScreen(const char* error_message) {
  initTextScreen();
  M5.Lcd.print(" Duet connection failed.\n\n\n ");
  M5.Lcd.println(error_message);
  M5.Lcd.printf("\n\n IP: [%s]", config_parser.ParsedData().duet_ip.c_str());
}

static void drawBadDuetResponseScreen() {
  initTextScreen();
  M5.Lcd.print(" Bad response from duet.");
}

static void drawInfoScreen(const DuetStatus& duet_status) {
  // Map the duet status char to screen configuration.
  const StatusConfig& config = decodeStatusChar(duet_status.state_char);
  M5.Lcd.fillScreen(config.bg_color);
  M5.Lcd.setTextColor(config.text_color);

  // Status name.
  M5.Lcd.setCursor(20, 80, 2);
  M5.Lcd.setTextSize(4);
  M5.Lcd.print(config.text);

  if (config.display_progress) {
    M5.Lcd.setCursor(160, 3, 2);
    M5.Lcd.setTextSize(3);
    M5.Lcd.printf("%5.1f%%", duet_status.progress_percents);
  }

  if (config.display_z) {
    M5.Lcd.setCursor(20, 200, 2);
    M5.Lcd.setTextSize(2);
    M5.Lcd.printf("Z:%0.1f", duet_status.z_height);

  }

  if (config.display_temps) {
    M5.Lcd.setCursor(130, 200, 2);
    M5.Lcd.setTextSize(2);
    M5.Lcd.printf("%0.1fc  %0.1fc", duet_status.temp1, duet_status.temp2);
  }
}

void setup() {
  M5.begin();
  Serial.begin(115200);
  Serial.println();

  // Open SD drive.
  if (!SD.begin()) {
    drawFatalErrorScreen("SD card not found.");
    return;
  }

  // Open config file
  File file = SD.open("/duet_buddy.json");
  if (!file) {
    drawFatalErrorScreen("Config file not found.");
    return;
  }

  // Parse config file. Results are stored in config_parser.
  Serial.println("Parsing file:");
  json_parser.StartParsing(&config_parser);
  while (file.available()) {
    const char c = file.read();
    Serial.print(c);
    json_parser.ParseNextChar(c);
  }
  Serial.println();
  file.close();

  // Config json parsed ok?
  if (!config_parser.IsParsedMessageOk()) {
    drawFatalErrorScreen("Bad config file.");
    return;
  }

  // Has all required config values?
  const Config& config = config_parser.ParsedData();
  if (config.wifi_ssid.isEmpty() || config.wifi_password.isEmpty() || config.duet_ip.isEmpty()) {
    drawFatalErrorScreen("Missing required config field.");
    return;
  }

  // Setup Wifi AP.
  if (!wifiMulti.addAP(config.wifi_ssid.c_str(), config.wifi_password.c_str())) {
    drawFatalErrorScreen("Wifi setup failed.");
    return;
  }

  // Construct the duet URL string.
  duet_url.add("http://");
  duet_url.add(config.duet_ip.c_str());
  duet_url.add("/rr_status?type=3");
  Serial.printf("Duet url: [%s]\n", duet_url.c_str());

  // Initialization done OK. Next we will connect to Wifi within
  // loop().
  drawNoWifiScreen();
}


void loop() {
  // Fatal error. Stay in this screen.
  if (fatal_error) {
    delay(10000);
    return;
  }

  // No wifi connection. Try again.
  if ((wifiMulti.run(10000) != WL_CONNECTED)) {
    drawNoWifiScreen();
    delay(500);
    return;
  }

  // Wifi was just connected.
  if (!wifi_connected) {
    drawWifiConnectedScreen();
    // We keep going to the http connection.
  }

  // Connect to duet and send a Get status http request.
  //http.begin(url);
  http.begin(duet_url.c_str());
  Serial.print("[HTTP] GET ");
  Serial.println(duet_url.c_str());
  const int httpCode = http.GET();
  Serial.printf("[HTTP] GET... code: %d\n", httpCode);


  // No HTTP connection or an HTTP error status.
  if (httpCode != HTTP_CODE_OK) {
    drawNoHttpConnectionScreen(http.errorToString(httpCode).c_str());
    http.end();  // remember to close the http client.
    delay(500);
    return;
  }

  // Got http response. Try parsing the json response
  // We don't load the entire json doc into memory but
  // parse it as a stream for a smaller memory footprint.
  json_parser.StartParsing(&duet_parser);
  WiFiClient& stream = http.getStream();
  while (stream.available()) {
    int c = stream.read();
    json_parser.ParseNextChar(c);
    Serial.print(char(c));
  }

  Serial.println("Done parsing, closing http");
  http.end();

  // Duet Json parsing failed. Not an expected response.
  if (!duet_parser.IsParsedMessageOk()) {
    Serial.println("Message not ok");
    drawBadDuetResponseScreen();
    delay(1000);
    return;
  }

  // We got a valid json response.
  const DuetStatus& duet_status = duet_parser.ParsedData();
  drawInfoScreen(duet_status);
  delay(5000);
}