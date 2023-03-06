#include <Arduino.h>
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

// TO BE REMOVED, SHOULD BE LOADED IF EXISTS
static const char CSR[] PROGMEM = R"EOF(-----BEGIN CERTIFICATE REQUEST-----
MIH1MIGcAgEAMDoxFzAVBgNVBAMMDmF3cy1kZXZpY2UtMDA0MQ0wCwYDVQQFEwQw
MDA0MRAwDgYDVQQKDAdXSVNlS2V5MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAE
jvM8FRm+CBod/lT09JbkBGSXn1Ipadib00TPtCbDdaUwPlny+GPE8nO/QFiyrW5Z
MvEJNHQPgigWo+rRnKAelqAAMAoGCCqGSM49BAMCA0gAMEUCIQDiYGbcwIRsdj/L
Ssrwtf2WEFmQZKVjJxGRtQTqXlj1GgIgMtwmyXS+RuWZt9b+IAGYcjLER4A/GXz/
TEuNL6GXV2c=
-----END CERTIFICATE REQUEST-----)EOF";

//
// GLOBAL VARIABLES
//

String brokerSNI = "";
String devID = "";
String devPubTopic = "";
String devSubTopic = "";

String strDeviceCert = "";
String strDeviceKey = "";
String strCACert = "";

String strESTserver = "";
String strESTusername = "";
String strESTpassword = "";

#ifdef ESP8266
X509List* x509CACert;
X509List* x509DeviceCert;
PrivateKey* deviceKey;

ESP8266WiFiMulti wifiMulti;
#endif

#ifdef ESP32
WiFiMulti wifiMulti;
#endif

WiFiClientSecure WIFIclient;
PubSubClient client(WIFIclient);


//////////////////////////////////////////////////////////////////////////////////////
//
// UTILITY FUNCTIONS
// LOAD CONFIG
//
//////////////////////////////////////////////////////////////////////////////////////

/***
 * LOAD WIFI CONFIG
***/
bool LoadWiFiConfig()
{
  String ssid, password;
  File file;
  int i = 0;

  // Mount file system.
  if (!LittleFS.begin()) {
    Serial.println("Failed to mount file system");
    return false;
  }

  file = LittleFS.open("/wifi.conf", "r");
  if(!file) {
    Serial.println("Failed to open wifi.conf file for reading");
    return false;
  }  
  
  Serial.println("WiFi Configuration:");

  while(file.available()) {
    ssid = file.readStringUntil(',');
    password = file.readStringUntil('\n');
    if (ssid[0] == '#') {
      // Terminate
      break;
    } else {
      i++;
      wifiMulti.addAP(ssid.c_str(), password.c_str());
      Serial.println("+ WiFi SSID " + String(i) + ": " + ssid);
      Serial.println("+ WiFi Password " + String(i) + ": " + password);
    }
  }

  Serial.println();

  file.close();

  return (i > 0);
}

/***
 * LOAD TLS FILES
***/
bool loadTLSFiles(String caCert, String deviceCert)
{
  String fileName;
  File file;
  char c;

  if(!LittleFS.begin()) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return false;
  }

  if (caCert != "") {
    // Load CA certificate
    fileName = "/" + caCert;
    file = LittleFS.open(fileName.c_str(), "r");

    if(!file) {
      Serial.println("Failed to open CA certifcate file for reading");
      return false;
    }

    while(file.available()){
      c = file.read();
      strCACert += c;
    }
    file.close();
    Serial.println("CA certificate [" + fileName +"]:");
    Serial.println(strCACert);
    Serial.println();

  #ifdef ESP8266
    x509CACert = new X509List(strCACert.c_str());
  #endif
  }

  // Load device certificate
  if (deviceCert != "") {
    // Load CA certificate
    fileName = "/" + deviceCert + ".crt";
    file = LittleFS.open(fileName.c_str(), "r");

    if(!file) {
      Serial.println("Failed to open device certificate file for reading");
      return false;
    }  

    while(file.available()){
      c = file.read();
      strDeviceCert += c;
    }
    file.close();
    Serial.println("Device certificate [" + fileName +"]:");
    Serial.println(strDeviceCert);
    Serial.println();

  #ifdef ESP8266
    x509DeviceCert = new X509List(strDeviceCert.c_str());
  #endif

    // Load device key  
    fileName = "/" + deviceCert + ".key";
    file = LittleFS.open(fileName.c_str(), "r");
    if(!file) {
      Serial.println("Failed to open device key file for reading");
      return false;
    }

    while(file.available()){
      c = file.read();
      strDeviceKey += c;
    }
    file.close();
    Serial.println("Device key [" + fileName +"]:");
    Serial.println(strDeviceKey);
    Serial.println();

  #ifdef ESP8266
    deviceKey = new PrivateKey(strDeviceKey.c_str());
  #endif
  }

  return true;
}

/***
 * LOAD MQTT CONFIG
***/
bool loadMQTTConfig()
{
  String buf, strCAFileName, strCertFileName;
  File file;

  // Mount file system.
  if (!LittleFS.begin()) {
    Serial.println("Failed to mount file system");
    return false;
  }

  file = LittleFS.open("/mqtt.conf", "r");
  if(!file) {
    Serial.println("Failed to open file for reading");
    return false;
  }  

  // Load mandatory items  
  brokerSNI = file.readStringUntil('\n');
  devID = file.readStringUntil('\n');
  devPubTopic = file.readStringUntil('\n');
  devSubTopic = file.readStringUntil('\n');

  Serial.println("MQTT Configuration:");
  Serial.println("+ MQTT Broker SNI: " + brokerSNI);
  Serial.println("+ Device ID: " + devID);
  Serial.println("+ Device Publish Topic: " + devPubTopic);
  Serial.println("+ Device Subscribe Topic: " + devSubTopic);

  // Load optional items
  // Try to load CA cert file name
  strCAFileName  = file.readStringUntil('\n');
  strCAFileName.trim();
  if (strCAFileName[0] != '#') {
    if (strCAFileName == "INSECURE") {
      strCAFileName = "";
      Serial.println("+ No CA certificate validation used");
    } else {
      Serial.println("+ CA certificate file name: " + strCAFileName);
    }
    // Try to load device certificate file name
    strCertFileName  = file.readStringUntil('\n');
    strCertFileName.trim();
    if (strCertFileName[0] != '#') {
      Serial.println("+ Device certificate file name: " + strCertFileName);
    } else {
      strCertFileName = "";
      Serial.println("+ No certificate authentication used");
    }
  } else {
      Serial.println("+ No TLS config found");
  }

  // Try to load the EST settings
  strESTserver  = file.readStringUntil(',');
  strESTserver.trim();
  if (strESTserver[0] != '#') {
    Serial.println("+ EST Server: " + strESTserver);
    // Try to read EST authentication
    strESTusername  = file.readStringUntil(',');
    strESTusername.trim();
    if (strESTusername[0] != '#') {
      if (strESTusername.length() > 0)
        Serial.println("+ EST username: " + strESTusername);
      strESTpassword  = file.readStringUntil('\n');
      strESTpassword.trim();
      if ((strESTpassword[0] != '#') && (strESTpassword.length() > 0)) {
        Serial.println("+ EST password: " + strESTpassword);
      } else {
        strESTusername = "";
        strESTpassword = "";
        Serial.println("+ EST password missing, certificate authentication to be used");
      }
    } else {
      strESTusername = "";
      strESTpassword = "";
      Serial.println("+ EST certificate authentication to be used");
    }
  } else {
    strESTserver = "";
    Serial.println("+ No EST config found");
  }

  Serial.println();

  file.close();

  return loadTLSFiles(strCAFileName, strCertFileName);
}


//////////////////////////////////////////////////////////////////////////////////////
//
// UTILITY FUNCTIONS
// EST PROTOCOL PROVISIONING
//
//////////////////////////////////////////////////////////////////////////////////////

/***
 * EST SIMPLE ENROLL
***/
bool estSimpleEnroll(String strCsr, String &strCert)
{
  HTTPClient httpClient;
  WiFiClientSecure* WIFIclient;
  String payload;
  int statusCode;
  bool requestResult = false;

  WIFIclient = new WiFiClientSecure;
  WIFIclient->setInsecure();
//  WIFIclient->setCACert(CA_CERT);

  Serial.println("POST DATA:");
  Serial.println(strCsr);
  Serial.println("POST LENGTH:");
  Serial.println(strCsr.length());  

  httpClient.begin(*WIFIclient,  "wksa-est.certifyiddemo.com", 443, "/.well-known/est/simpleenroll/", true);

//  httpClient.setAuthorization("90af2231057f4505a92185e134d3fbe4", "d847f238c8e1453cadc1ddf0f6ba491d");
//  httpClient.addHeader("Content-Type", "application/raw");
//  httpClient.addHeader("Host", "wksa-est.certifyiddemo.com");
//  httpClient.addHeader("Content-Length", "284");

  statusCode = httpClient.POST((uint8_t *)CSR, sizeof(CSR) - 1);

  if (statusCode > 0) {
    requestResult = (statusCode == HTTP_CODE_OK);
    Serial.printf("Got HTTP status: %d\n", statusCode);
    payload = httpClient.getString();
    Serial.println("Payload received:");
    Serial.println(payload);
  }
  else {
    Serial.printf("Error occurred while sending HTTP POST: %s\n", httpClient.errorToString(statusCode).c_str());
    requestResult = false;
  }

  // Release the resources used
  httpClient.end();
  delete WIFIclient;

  if (!requestResult)
    payload = "";

  strCert = payload;
  return requestResult;
}


/***
 * EXTRACT CERTIFICATE FROM PKCS7
***/
bool ConvertP7toPEM(String &str)
{
  HTTPClient httpClient;
  WiFiClient* WIFIclient;
  String payload;
  int statusCode, i;
  bool requestResult = false;

  WIFIclient = new WiFiClient;

  payload = str;

  // Clean the input
  payload.trim();
  while((i = payload.indexOf('\n')) > 0) {
    payload.remove(i, 1);
  }
  while((i = payload.indexOf('\r')) > 0) {
    payload.remove(i, 1);
  }

  Serial.println("POST DATA:");
  Serial.println(payload);
  Serial.println("POST LENGTH:");
  Serial.println(payload.length());  

//  httpClient.begin("http://hotline.retrobyt.es:1880/opensslp7");
  httpClient.begin(*WIFIclient,  "hotline.retrobyt.es", 1880, "/opensslp7/", false);

  statusCode = httpClient.POST((uint8_t *)payload.c_str(), payload.length());

  if (statusCode > 0) {
    requestResult = (statusCode == HTTP_CODE_OK);
    Serial.printf("Got HTTP status: %d\n", statusCode);
    payload = httpClient.getString();
    Serial.println("Payload received:");
    Serial.println(payload);
  }
  else {
    Serial.printf("Error occurred while sending HTTP POST: %s\n", httpClient.errorToString(statusCode).c_str());
    requestResult = false;
  }

  // Release the resources used
  httpClient.end();
  delete WIFIclient;

  if (requestResult)
    str = payload;

  return requestResult;

}


//////////////////////////////////////////////////////////////////////////////////////
//
// UTILITY FUNCTIONS
// CONNECTIVITY
//
//////////////////////////////////////////////////////////////////////////////////////

/***
 * ACTIVATE WIFI
***/
void setWiFi()
{

  Serial.println();
  Serial.print("Connecting to WiFi");

  while (wifiMulti.run() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }

  Serial.println("WiFi connected");
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("BSSID: ");
  Serial.println(WiFi.BSSIDstr());
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println();
}


/***
 * SET TIME VIA NTP
***/
void setClock() {
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println("");
  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.println(asctime(&timeinfo));
}

/***
 * SUBSCRIBE CALLBACK
 * Print any message received for subscribed topic
***/
void callback(char* topic, byte* payload, unsigned int length) {
  String sTopic = topic;
  String sCommand = sTopic.substring(sTopic.lastIndexOf("/") + 1);

  Serial.println();
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  String sPayload = "";
  for (unsigned int i=0; i < length; i++) {
    sPayload += (char)payload[i];
  }
  Serial.println(sPayload);

  Serial.println("Command: " + sCommand);
}


/***
 * SETUP MQTT CONNECTION
***/
void setMQTT()
{
  if (strCACert.length() > 0) {
    // Load CA cert into trust store
    Serial.println("MQTT Connect: Enable CA validation");
#ifdef ESP8266  
    WIFIclient.setTrustAnchors(x509CACert);
#endif
#ifdef ESP32
    WIFIclient.setCACert(strCACert.c_str());
#endif
  } else {
    // Skip CA Validation
    Serial.println("MQTT Connect: Skip CA validation");
    WIFIclient.setInsecure();
  }

//  Enable self-signed cert support
  //  WIFIclient.allowSelfSignedCerts();

// Optional: Broker certificate validation
  //  WIFIclient.setFingerprint(BROKER_FINGERPRINT);

// Set Client certificate
#ifdef ESP32
  WIFIclient.setCertificate(strDeviceCert.c_str());
  WIFIclient.setPrivateKey(strDeviceKey.c_str());
#endif

#ifdef ESP8266
  if (deviceKey->isEC()) {
    WIFIclient.setClientECCert(x509DeviceCert, deviceKey, BR_KEYTYPE_KEYX | BR_KEYTYPE_SIGN, BR_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256);
  } else {
    WIFIclient.setClientRSACert(x509DeviceCert, deviceKey);
  }
#endif

  //connect to MQTT server
  client.setServer(brokerSNI.c_str(), 8883);
// Alternative for non-TLS connections
  //  WIFIclient.setServer(mqtt_server, 1883);

// Configure SUB callback
  client.setCallback(callback);
}

//
// RECONNECT TO BROKER
//
void reconnect() 
{
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(devID.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(devPubTopic.c_str(),"{\"status\":\"Connected!\"}");
      // ... and resubscribe
      client.subscribe(devSubTopic.c_str());
  client.setCallback(callback);
      Serial.println("Subscribed to " + devSubTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}



//////////////////////////////////////////////////////////////////////////////////////
//
// BOARD SETUP
//
//////////////////////////////////////////////////////////////////////////////////////

void setup() 
{
  delay(1000);

  // Default settings
  Serial.begin(115200);

  // Setup WiFi
  LoadWiFiConfig();
  setWiFi();

  // Setup MQTT
  loadMQTTConfig();
  setMQTT();
}


//////////////////////////////////////////////////////////////////////////////////////
//
// MAIN PROCESS LOOP
//
//////////////////////////////////////////////////////////////////////////////////////

void loop() 
{
  static long lastMsg = 0;
  String payload;
  float temperature = 0;
  float humidity = 0;

  // Block until we are able to connect to the WiFi access point
  if (wifiMulti.run() == WL_CONNECTED) {
    if (!client.connected()) {
      Serial.println("Reconnect to broker");
      reconnect();
    } else {
      client.loop();
      // Only publish in 5 sec. intervals
      long now = millis();
      if (now - lastMsg > 5000) {
        lastMsg = now;
        
        temperature = 123.45f;
        humidity = 56.78f;

        payload = "{\"temperature\":" + String(temperature) + ", \"humidity\":" + String(humidity) + "}";
        client.publish(devPubTopic.c_str(), (char*) payload.c_str());
        Serial.println("PUBLISH!");
      }
    }
  }
}