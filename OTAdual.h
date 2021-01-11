// This file works on ESP8266, not tested on ESP32
//
#ifdef ESP32
#include <WiFi.h>
#include <ESPmDNS.h>
#else
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#endif


#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#if defined(ESP32_RTOS) && defined(ESP32)
void taskOne( void * parameter )
{
  ArduinoOTA.handle();
  delay(3500);
}
#endif

// global variables and definitions ------------
#define WIFICLIENT  0
#define WIFIAP      1
int    wifimode;            // can be WIFICLIENT or WIFIAP
String ssidList;            // list to be included in the form to choose the available ssid
const IPAddress apIP(192, 168, 1, 1);
const char* apSSID = "esp8266_rs41_SETUP";
DNSServer dnsServer;
// ---------------------------------------------

IPAddress getAPip() {
  return(apIP);
}

int getWifiMode() {
  return(wifimode);
}

String getSsidList() {
  return(ssidList);
}

void scanNetworks(const char* current_ssid) {
  char s[30];
  delay(100);
  int n = WiFi.scanNetworks();
  delay(100);
  for (int i = 0; i < n; ++i) {
    ssidList += "<option value=\"";
    ssidList += WiFi.SSID(i);
    if (wifimode == WIFICLIENT) {
      WiFi.SSID(i).toCharArray(s,sizeof(s));
      if (strncmp(s,current_ssid,sizeof(s)) == 0) {
	ssidList += "\" selected>";
      } else {
	ssidList += "\">";
      }
    } else {
      ssidList += "\">";
    }
    ssidList += WiFi.SSID(i);
    ssidList += "</option>";
    Serial.println(WiFi.SSID(i));
  }
  delay(100);
}

void setupOTA(const char* nameprefix, const char* ssid, const char* password) {
  const int maxlen = 40;
  char fullhostname[maxlen];
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(fullhostname, maxlen, "%s-%02x%02x%02x", nameprefix, mac[3], mac[4], mac[5]);
  ArduinoOTA.setHostname(fullhostname);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() == WL_CONNECTED) {  // successfully connected as client to an AP
    Serial.print("Wifi client connection successfull to ssid: ");Serial.println(ssid);
    wifimode = WIFICLIENT;
    scanNetworks(ssid);
  } else {                 // become an access point
    wifimode = WIFIAP;
    Serial.print("Wifi client connection failed to ssid: ");Serial.println(ssid);
    WiFi.disconnect();
    scanNetworks("");
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
    WiFi.softAP(apSSID);
    dnsServer.start(53, "*", apIP);
  }

  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

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

  Serial.println("OTA Initialized");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

#if defined(ESP32_RTOS) && defined(ESP32)
  xTaskCreate(
    ota_handle,          /* Task function. */
    "OTA_HANDLE",        /* String with name of task. */
    10000,            /* Stack size in bytes. */
    NULL,             /* Parameter passed as input of the task */
    1,                /* Priority of the task. */
    NULL);            /* Task handle. */
#endif
}
