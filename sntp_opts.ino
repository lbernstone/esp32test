#include <WiFi.h>

//#define MYSSID // add your wifi ssid
//#define PASSWD // add your password
#define NTP_SERVER "pool.ntp.org"
#define TZ_INFO "MST7MDT6,M3.2.0/02:00:00,M11.1.0/02:00:00"
// see http://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html

void initNTP() {
// configTzTime will fire up an NTP connection and check the server every hour
  struct tm timeinfo;
  configTzTime(TZ_INFO, NTP_SERVER);
  if (getLocalTime(&timeinfo, 10000)) { // wait up to 10sec to sync
    Serial.println(&timeinfo, "Time set: %B %d %Y %H:%M:%S (%A)");
  } else {
    Serial.println("Time not set");
  }
}

void initWifi() {
#ifdef MYSSID
  WiFi.begin(MYSSID,PASSWD);
#else
  WiFi.begin();  
#endif
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println(WiFi.localIP());
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  initWifi();
  delay(250);
  initNTP();
}

void loop() {}
