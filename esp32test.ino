#include <esp_pm.h>
#include <esp_wifi.h>
#include <esp_wifi_types.h>
#include <U8x8lib.h>
#include <LoRa.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <nvs_flash.h>
#include "WiFiManager.h"
#include <EEPROM.h>
#include <esp_task_wdt.h>

#define SX1278_PINS 5,19,27,18 // SCK,MISO,MOSI,CS
#define LORA_PINS 18,14,26 // SS,RST,DI0
#define LORA_BAND    433E6 // 433MHz
#define OLED_PINS 15,4,16 // CLK,DATA,RST
#define NTP_SERVER "us.pool.ntp.org"
#define TZ_INFO "MST7MDT6,M3.2.0/02:00:00,M11.1.0/02:00:00"
#define AP_TIMEOUT 600
#define RESET_BTN 0
#define OUTPUT_PINS 23,12,22,25 // There are 4 lines on the
// display for the marks. Fill unused pins with a 255 and they
// will show up as an X on the display
#define SD_CHANNELS 4 //4 channels of sigmadelta
#define SD_CHAN_0 {21}
#define SD_CHAN_1 {13}
#define SD_CHAN_2 {17}
#define SD_CHAN_3 {2}

const uint8_t out_pins[] = {OUTPUT_PINS};
const uint8_t sd_chan[][SD_CHANNELS] = {SD_CHAN_0, SD_CHAN_1, SD_CHAN_2, SD_CHAN_3};

U8X8_SSD1306_128X64_NONAME_SW_I2C u8x8(OLED_PINS);
WebServer server(80);
bool wifiReset = false;

void hard_restart() {
  esp_task_wdt_init(1,true);
  esp_task_wdt_add(NULL);
  while(true); 
}

void initNTP() {
// configTzTime will fire up an NTP connection and check the server every hour
  struct tm timeinfo;
  configTzTime(TZ_INFO, NTP_SERVER);
  if (getLocalTime(&timeinfo, 10000)) { // wait up to 10sec to sync
    uint8_t delay = mktime(&timeinfo) % 60;
    // Set timer to fire at next minute mark
    
//    Serial.println(&timeinfo, "Time set: %B %d %Y %H:%M:%S (%A)");
//  } else {
//    Serial.println("Time not set");
  }
}

void setTime() {
  char* ampm[2] = {"AM", "PM"} ;
  tm local;
// Fill a tm structure with the current time
  getLocalTime(&local);
// Print the date and time on the OLED. The format will be:
// 08:35 PM
// 05/31/2017
  char dtime[20];
  uint8_t hr = (local.tm_hour % 12)?(local.tm_hour %12):12;
  sprintf(dtime,"%02i:%02i",hr,local.tm_min);
  u8x8.setFont(u8x8_font_saikyosansbold8_n);  
  u8x8.draw2x2String(0,0,dtime);
  sprintf(dtime," %-4s",ampm[(local.tm_hour/12)]);
  u8x8.setFont(u8x8_font_artossans8_r);
  u8x8.drawString(10,1,dtime);
  //Serial.println(dtime);
  sprintf(dtime,"%02i/%02i/20%-6i",local.tm_mon+1,local.tm_mday,local.tm_year-100);
  //Serial.println(dtime);
  u8x8.drawString(0,2,dtime);
}

boolean checkWifi() {
  if (WiFi.status() == WL_CONNECTED) {return true;} else {return false;}
}

void initWifiClient() {
// WiFiManager will check to see if there is an SSID set.
// If so, it simply connects.  Otherwise, it sets up an AP
// that directs to a captive portal if you connect on a cell phone.
  WiFiManager wifiManager;
  wifiManager.setTimeout(AP_TIMEOUT);
  wifiManager.setDebugOutput(true);
  Serial.println("Starting WiFiManager");
  String ssid = "ESP" + String(ESP_getChipId());
// It prints the SSID on the screen for clarity.  
  u8x8.drawString(0,1,ssid.c_str());
  
  if(!wifiManager.autoConnect()) {
// If nobody connects before the timeout, it will reboot.
    Serial.println(F("failed to connect and hit timeout"));
    delay(1000);
    ESP.restart();
  }
  esp_wifi_set_ps(WIFI_PS_MODEM);
  Serial.println(WiFi.localIP());
}

void IRAM_ATTR sysReset() {
// When the reset button is pressed, the wifi credentials are
// reset, and the system restarts as an AP.
   wifiReset = true;
}

void resetStaInfo() {
// WifiManager is supposed to clear the IP address, but there is
// a current bug (issue #400), so it has to be forced with nvs
    esp_err_t err;
    nvs_handle nvs_mqtt;    
    Serial.println(F("Wifi reset"));
    err = nvs_flash_init();
    Serial.println("nvs_flash_init: " + err);
    err = nvs_open("nvs.net80211", NVS_READWRITE, &nvs_mqtt);    
    Serial.println("nvs_open: " + err);
    err = nvs_erase_key(nvs_mqtt, "sta.ssid");
    Serial.println("nvs_erase_ssid: " + err);
    err = nvs_erase_key(nvs_mqtt, "sta.pswd");
    Serial.println("nvs_erase_pswd: " + err);
    u8x8.clearDisplay();
    hard_restart();
}

void startOTA () {
  //ArduinoOTA.setHostname("myesp32");
  ArduinoOTA.onStart([]() {
    String type;
    type = (ArduinoOTA.getCommand() == U_FLASH)?"filesystem":"sketch";
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();  
}

void setMarks() {
// Column 15 on each line will have a mark.
// Empty O for Off, filledO for on, X for unused.  
  uint8_t filledO[8] = {62,127,127,127,127,127,62,0};
  for (int x=0; x<sizeof(out_pins); x++) {
    if (out_pins[x]>39) {
      u8x8.drawString(15,x,"X");
    } else {
      if (digitalRead(out_pins[x])) {
        u8x8.drawTile(15,x,1,filledO);
      } else {
        u8x8.drawString(15,x,"O");
      }
    }
  }
  for (int x=0; x<sizeof(SD_CHANNELS); x++) {
    uint8_t fill = pow(2,sigmaDeltaRead(x)/32) - 1;
    uint8_t bars[8] = {fill,fill,fill,fill,fill,fill,fill,fill};
    u8x8.drawTile(15,x+4,1,bars);
  }
}

void handleRoot() {
// A simple web page with checkboxes to turn on and off the output pins
  String result;
  result = F("<html><head></head><body>");
  //result += F("<br>Uptime: ");
  //result += String((int)(millis()/1000));
  result += F("<h1>Pin Control</h1>");
// Submit will call the /set URI
  result += F("<form method='get' action='set'>");
  for (int x=0; x<sizeof(out_pins); x++) {
    if (out_pins[x]<40) {
      result += F("Pin ");
      result += String(x);
      result += F(": <input type='checkbox' name='pin");
      result += String(x) + "'";
      if (digitalRead(out_pins[x])) {result += " checked";}
      result += F("><br>");
    }
  }
  result += F("<h1>PWM control</h1>");
  for (int x=0; x<SD_CHANNELS; x++) {
    result += F("Channel ");
    result += String(x);
    result += F(": <input type='range' name='pwm");
    result += String(x) + F("' min='0' max='255' value='");
    result += String(sigmaDeltaRead(x));
    result += F("'><br>");   
  }
  result += F("<br><input type='submit' value='Set Pins'/>");
  result += F("</form><p><a href='/'>Refresh</a><p>");
  result += F("<a href='/reset'>Reset</a>");
  result += F("</p></body></html>");
  server.send(200, "text/html", result);
}

void handleSetPins() {
// Loop through the arguments to see what pins should be high
  //Serial.println(server.args());
  for (uint8_t x=0; x<sizeof(out_pins); x++) {
    char pin[6];
    sprintf(pin,"pin%i",x);
    if (server.hasArg(pin)) {
      digitalWrite(out_pins[x],HIGH);
    } else {
      digitalWrite(out_pins[x],LOW);
    }
  }
  for (uint8_t x=0; x<SD_CHANNELS; x++) {
    char pwm[6];
    sprintf(pwm,"pwm%i",x);
    if (server.hasArg(pwm)) {
      sigmaDeltaWrite(x,server.arg(pwm).toInt());
    }
  }
// Update the display with the marks.
  setMarks();
  delay(50);
// Redirect to root afterwords
  server.sendHeader("Location", String("/"), true);
  server.send ( 302, "text/plain", "");
}

void handleReset() {
  server.sendHeader("Location", String("/"), true);
  server.sendHeader("Refresh", String("8"), true);
  server.send ( 302, "text/plain", "Please wait while the server restarts");
  sysReset();
}

void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void initWebserver() {
  server.on("/", handleRoot);
  server.on("/set", handleSetPins);
  server.on("/reset", handleReset);
  server.onNotFound ( handleNotFound );
  server.begin();
}

void initReset() {
// Puts an interrupt on pin 0 (the top button on LoRa cards)
  Serial.println(F("init Sensors"));
  pinMode(RESET_BTN, INPUT);
// Call sysReset to clear the wifi info when pressed
  attachInterrupt(digitalPinToInterrupt(RESET_BTN), sysReset, RISING);
}

void initPins() {
  for (int x=0; x<sizeof(out_pins); x++) {
    if (out_pins[x]>=0) {
      pinMode(out_pins[x],OUTPUT);
      digitalWrite(out_pins[x],LOW);
    }
  }
  for (int x=0; x<SD_CHANNELS; x++) {
    sigmaDeltaSetup(x, 100000);
    for (int y=0; y<sizeof(sd_chan[x]); y++) {
      if (sd_chan[x][y]) sigmaDeltaAttachPin(sd_chan[x][y],x);
    }
    sigmaDeltaWrite(x, 0);
  }
  setMarks();
}

void setup() {
  Serial.begin(115200);
  initReset();
  delay(100);

  SPI.begin(SX1278_PINS);
  LoRa.setPins(LORA_PINS);
  pinMode(26, INPUT);
  u8x8.begin();
  u8x8.setFont(u8x8_font_artossans8_r);

  initPins();
//  WiFi.begin("larryb","clownfish");
  if (!checkWifi()) {initWifiClient();}
  delay(250);
  initNTP();

  char localip[15];
  WiFi.localIP().toString().toCharArray(localip, 15);
  u8x8.drawString(0, 3, localip);
  startOTA();
  initWebserver();
/*
  u8x8.drawString(0, 4, "LoRa Receiver");

  if (!LoRa.begin(LORA_BAND)) {
    Serial.println("Starting LoRa failed!");
    u8x8.drawString(0, 5, "LoRa failed!");
    while (1);
  }
*/
}

void loop() {
/*  
  String receivedText;
  String receivedRssi;

  // try to parse packet
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    // received a packet
    Serial.print("Received packet '");
    u8x8.drawString(0, 5, "PacketID");

    // read packet
    while (LoRa.available()) {
      receivedText = (char)LoRa.read();
      Serial.print(receivedText);
      char currentid[64];
      receivedText.toCharArray(currentid, 64);
      u8x8.drawString(9, 5, currentid);
    }

    // print RSSI of packet
    Serial.print("' with RSSI ");
    Serial.println(LoRa.packetRssi());
    u8x8.drawString(0, 6, "PacketRS");
    receivedRssi = LoRa.packetRssi();
    char currentrs[64];
    receivedRssi.toCharArray(currentrs, 64);
    u8x8.drawString(9, 6, currentrs);
  }
*/
  if (wifiReset) resetStaInfo();
  setTime();
  ArduinoOTA.handle();
  server.handleClient();
  delay(500);
}
