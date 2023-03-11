#ifndef UTILIOT_H

#define UTILIOT_H

#include <LittleFS.h>

#ifdef ESP32
#include <WiFi.h>
#include <WiFiMulti.h>
#include <HTTPClient.h>
#endif

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#endif

#include <WiFiClientSecure.h>
#include <PubSubClient.h>

//
// GLOBAL CONSTANTS 
//

const u_int8_t DEV_STATUS_UNDEFINED = 0;
const u_int8_t DEV_STATUS_ERROR = 1;
const u_int8_t DEV_STATUS_FACTORY = 2;
const u_int8_t DEV_STATUS_OPERATIONAL = 3;

//
// GLOBAL VARIABLES
//

extern u_int8_t deviceStatus;

extern String brokerSNI;
extern String devID;
extern String devPubTopic;
extern String devSubTopic;

extern String strDeviceCert;
extern String strDeviceKey;
extern String strDeviceCSR;
extern String strCACert;

extern String strCAFileName;
extern String strCertFileName;

extern String strESTserver;
extern String strESTusername;
extern String strESTpassword;

#ifdef ESP8266
extern X509List* x509CACert;
extern X509List* x509DeviceCert;
extern PrivateKey* deviceKey;

extern ESP8266WiFiMulti wifiMulti;
#endif

#ifdef ESP32
extern WiFiMulti wifiMulti;
#endif

extern WiFiClientSecure WIFIclient;
extern PubSubClient client;

///////////////////////////////
//
// FUNTION DECLARATION
//
///////////////////////////////

bool LoadWiFiConfig();
bool loadTLSFiles(String caCert, String deviceCert);
bool updateCertFile();
bool loadMQTTConfig();
bool estSimpleEnroll(String strCsr, String &strCert);
bool ConvertP7toPEM(String &str);
void setWiFi();
void setClock();
void setMQTT();
void reconnect(); 

extern void callback(char* topic, byte* payload, unsigned int length); // MUST BE IN MAIN MODULE

#endif