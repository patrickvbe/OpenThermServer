// python3 ~/.arduino15/packages/esp8266/hardware/esp8266/2.7.4/tools/espota.py -i 192.168.178.6 -p 8266 -f /tmp/arduino_build_661492/MainController.ino.bin
// LoLin (WeMos) D1 mini
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

//#define DEBUG
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
// MQTT support
//////////////////////////////////////////////////////////////
#include <ArduinoHA.h>
WiFiClient client;
HADevice device("OpenTherm");
HAMqtt mqtt(client, device);
HAHVAC hvac("thermostaat", HAHVAC::TargetTemperatureFeature);
HASensorNumber mqtt_modlevel("ModLevel", HABaseDeviceType::PrecisionP0);
HANumber mqtt_toffset("TOffset", HABaseDeviceType::PrecisionP1);
unsigned long lastFullMqttUpdate = millis();
#define MQTT_FULL_UPDATE_INTERVAL 15000

void reportFullMqttState() {
  if (ctrl.tset_reported != INVALID_TEMP) hvac.setTargetTemperature(ctrl.tset_reported/10.0F);
  if (ctrl.troom_reported != INVALID_TEMP) hvac.setCurrentTemperature(ctrl.troom_reported / 10.0F);
  if (ctrl.modlevel_reported >= 0) mqtt_modlevel.setValue(ctrl.modlevel_reported);
  mqtt_toffset.setState(ctrl.toffset/10.0F);
}

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
  // MQTT configuration
  //////////////////////////////////////////////////////////////
  device.enableSharedAvailability();
  device.enableLastWill();
  device.enableExtendedUniqueIds();
  device.setName("OpenTherm");
  mqtt.onConnected([]() { reportFullMqttState(); });
  hvac.onTargetTemperatureCommand([](HANumeric temperature, HAHVAC* sender) {
    // Nog even niets...
  });
  hvac.setMinTemp(10);
  hvac.setMaxTemp(30);
  hvac.setTempStep(0.5);
  hvac.setName("thermostaat");
  hvac.setMode(HAHVAC::HeatMode);
  mqtt_toffset.setDeviceClass("temperature");
  mqtt_toffset.setName("temperature offset");
  mqtt_toffset.setMin(-5.0);
  mqtt_toffset.setMax(5.0);
  mqtt_toffset.setStep(0.1);
  mqtt_toffset.onCommand([](HANumeric offset, HANumber* sender) {
    ctrl.toffset = offset.toFloat() * 10;
  });
  mqtt_modlevel.setName("modulation level");
  mqtt_modlevel.setUnitOfMeasurement("%");
  mqtt.begin(BROKER_ADDR, broker_login, broker_pwd);

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
  // MQTT
  //////////////////////////////////////////////////////////////
  mqtt.loop();

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
  // MQTT
  //////////////////////////////////////////////////////////////
  if (ctrl.tset_reported != ctrl.tset_received) {
    ctrl.tset_reported = ctrl.tset_received;
    hvac.setTargetTemperature(ctrl.tset_reported/10.0F);
  }
  if (ctrl.troom_reported != ctrl.troom_received ) {
    ctrl.troom_reported = ctrl.troom_received;
    hvac.setCurrentTemperature(ctrl.troom_reported / 10.0F);
  }
  if (ctrl.modlevel_reported != ctrl.modlevel_received) {
    ctrl.modlevel_reported = ctrl.modlevel_received;
    mqtt_modlevel.setValue(ctrl.modlevel_reported);
  }

  // Prevent mqtt form invalidating values when not updated.
  if ( (timestamp - lastFullMqttUpdate) > MQTT_FULL_UPDATE_INTERVAL )
  {
    reportFullMqttState();
  }

  //////////////////////////////////////////////////////////////
  // Prevent a power-sucking 100% CPU loop.
  //////////////////////////////////////////////////////////////
  delay(2);
  lastloopmillis = timestamp;
}
