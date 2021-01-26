// python3 ~/.arduino15/packages/esp8266/hardware/esp8266/2.7.4/tools/espota.py -i 192.168.178.115 -p 8266 -f /tmp/arduino_build_661492/MainController.ino.bin
// Going OTA...
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "network_secrets.h"
//#define STASSID "your-ssid"
//#define STAPSK  "your-password"
const char* ssid = STASSID;
const char* password = STAPSK;
bool doingota = false;

#define DEBUG;
#ifdef DEBUG
  #define DEBUGONLY(statement) statement;
#else
  #define DEBUGONLY(statement)
#endif

//////////////////////////////////////////////////////////////
// Pin settings
//////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////
// Optional elements
//////////////////////////////////////////////////////////////
#define INCLUDE_MONITOR

// WiFi
WiFiEventHandler mConnectHandler, mDisConnectHandler, mGotIpHandler;
const unsigned long WIFI_TRY_INTERVAL = 60000;
unsigned long lastWiFiTry = -WIFI_TRY_INTERVAL;

#include "WebServer.h"

/***************************************************************
 * The global initialization function.
 ***************************************************************/
void setup()
{
  Serial.begin(115200);

  //////////////////////////////////////////////////////////////
  // WiFi setup
  //////////////////////////////////////////////////////////////
  WiFi.mode(WIFI_STA);
  WiFi.disconnect() ;
  WiFi.persistent(false);
  mDisConnectHandler = WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected&)
  {
    ctrl.wifiStatus = '-';
  });
  mConnectHandler = WiFi.onStationModeConnected([](const WiFiEventStationModeConnected&)
  {
    ctrl.wifiStatus = '+';
  });
  mGotIpHandler = WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP&)
  {
    ctrl.wifiStatus = '#';
    ArduinoOTA.begin();
    MDNS.begin("OTServer");
  });

  //////////////////////////////////////////////////////////////
  // OTA setup
  //////////////////////////////////////////////////////////////
#ifdef DEBUG
  ArduinoOTA.setHostname("DevOTServer");
#else
  ArduinoOTA.setHostname("OTServer");
#endif
  ArduinoOTA.onStart([]() {
    doingota = true;
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
  });
  ArduinoOTA.onEnd([]() {
    doingota = false;
  });
  ArduinoOTA.onError([](ota_error_t error) {
    doingota = false;
  });

  //////////////////////////////////////////////////////////////
  // 
  //////////////////////////////////////////////////////////////
  webserver.Init(ctrl);
  lastloopmillis = millis();
}

/***************************************************************
 * Log time since startup in a nicely readable format.
 ***************************************************************/
#ifdef DEBUG
void LogTime()
{
  unsigned long totalseconds = millis()/1000;
  unsigned long seconds = totalseconds % 60;
  unsigned long minutes = totalseconds / 60;
  Serial.print(minutes);
  Serial.print(':');
  if ( seconds < 10 ) Serial.print('0');
  Serial.print(seconds);
  Serial.print(' ');
}
#endif

/***************************************************************
 * Main loop
 ***************************************************************/
void loop()
{
  //////////////////////////////////////////////////////////////
  // OTA
  //////////////////////////////////////////////////////////////
  ArduinoOTA.handle();
  if ( doingota ) return;

  //////////////////////////////////////////////////////////////
  // Webserver
  //////////////////////////////////////////////////////////////
  webserver.Process();
  MDNS.update();

  auto timestamp = millis(); // Freeze the time

  //////////////////////////////////////////////////////////////
  // Try to stay connected to the WiFi.
  //////////////////////////////////////////////////////////////
  if (WiFi.status() != WL_CONNECTED && ctrl.wifiStatus != '#' && timestamp - lastWiFiTry > WIFI_TRY_INTERVAL)
  { 
    lastWiFiTry = timestamp;
    WiFi.disconnect() ;
    WiFi.begin ( ssid, password );  
  }

  //////////////////////////////////////////////////////////////
  // Prevent a power-sucking 100% CPU loop.
  //////////////////////////////////////////////////////////////
  delay(10);
  lastloopmillis = timestamp;
}
