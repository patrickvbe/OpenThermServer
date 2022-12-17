// python3 ~/.arduino15/packages/esp8266/hardware/esp8266/2.7.4/tools/espota.py -i 192.168.178.6 -p 8266 -f /tmp/arduino_build_661492/MainController.ino.bin
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

#define DEBUG
#ifdef DEBUG
  #define DEBUGONLY(statement) statement;
#else
  #define DEBUGONLY(statement)
#endif

//////////////////////////////////////////////////////////////
// Global data
//////////////////////////////////////////////////////////////
#include "ControlValues.h"
ControlValues ctrl;
unsigned long lastloopmillis;
unsigned long lastsecmillis;

//////////////////////////////////////////////////////////////
// Open Therm
//////////////////////////////////////////////////////////////
#include "OT.h"
OT ot;

//////////////////////////////////////////////////////////////
// WiFi
//////////////////////////////////////////////////////////////
WiFiEventHandler mConnectHandler, mDisConnectHandler, mGotIpHandler;
const unsigned long WIFI_TRY_INTERVAL = 60000;
unsigned long lastWiFiTry = -WIFI_TRY_INTERVAL;

#include "WebServer.h"

/***************************************************************
 * The global initialization function.
 ***************************************************************/
void setup()
{
  system_timer_reinit ();
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
    DEBUGONLY(Serial.print(WiFi.localIP()));
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
  ot.Init(ctrl);
  webserver.Init(ctrl);
  lastloopmillis = millis();
  lastsecmillis = millis();
}

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

  ////// Some time-keeping /////////////////////////////////////
  auto timestamp = millis(); // Freeze the time
  unsigned long deltasec = (timestamp - lastsecmillis) / 1000;
  ctrl.timestampsec += deltasec;
  lastsecmillis += deltasec * 1000;

  //////////////////////////////////////////////////////////////
  // Open Therm
  //////////////////////////////////////////////////////////////
  ot.Process();

  //////////////////////////////////////////////////////////////
  // Webserver
  //////////////////////////////////////////////////////////////
  webserver.Process();
  MDNS.update();

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
  delay(2);
  lastloopmillis = timestamp;
}
