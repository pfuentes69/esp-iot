#include <Arduino.h>
#include "UtilIoT.h"
#include <TFT_eSPI.h>     // Hardware-specific library
#include <TFT_eWidget.h>  // Widget library
#include "Adafruit_Sensor.h"
#include "Adafruit_AM2320.h"

#include "splash-bmp.h"

#define LED_RED 5
#define LED_GREEN 17
#define BUTTON_RED 12
#define BUTTON_GREEN 13

Adafruit_AM2320 am2320 = Adafruit_AM2320();
TFT_eSPI tft  = TFT_eSPI();      // Invoke custom library

MeterWidget   temp  = MeterWidget(&tft);
MeterWidget   hum  = MeterWidget(&tft);

/***
 * UPDATE LEDS FOR DVSTATUS
 * Show status in LEDS
***/
void showDeviceStatus() 
{
  switch(deviceStatus) {
    case DEV_STATUS_UNDEFINED:
      digitalWrite(LED_GREEN, HIGH);
      digitalWrite(LED_RED, HIGH);
      break;
    case DEV_STATUS_ERROR:
      digitalWrite(LED_GREEN, HIGH);
      digitalWrite(LED_RED, LOW);
      break;
    case DEV_STATUS_FACTORY:
      digitalWrite(LED_GREEN, LOW);
      digitalWrite(LED_RED, LOW);
      break;
    case DEV_STATUS_OPERATIONAL:
      digitalWrite(LED_GREEN, LOW);
      digitalWrite(LED_RED, HIGH);
      break;
  }
}


/***
 * SUBSCRIBE CALLBACK
 * Print any message received for subscribed topic
***/
void callback(char* topic, byte* payload, unsigned int length) 
{
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


//////////////////////////////////////////////////////////////////////////////////////
//
// BOARD SETUP
//
//////////////////////////////////////////////////////////////////////////////////////

void setup() 
{
  delay(100);

  // Default settings
  Serial.begin(115200);
  Serial.flush();

  Serial.println();
  Serial.println("<<< SETUP START >>>");
  Serial.println();

  // Init TFT Screen
  tft.init();
  tft.setRotation(0);
//  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes(!tft.getSwapBytes());
  tft.pushImage(0, 0, 240, 240, epd_bitmap_Splash);
  
  // Configure I/O
  am2320.begin();
  pinMode(BUTTON_RED, INPUT_PULLUP);
  pinMode(BUTTON_GREEN, INPUT_PULLUP);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  showDeviceStatus();

  // Setup WiFi
  LoadWiFiConfig();
  setWiFi();

  // Setup MQTT
  loadMQTTConfig();

  if (deviceStatus == DEV_STATUS_FACTORY) {
    Serial.println("FACTORY MODE - TRYING EST PROVISIONING");
    if (estSimpleEnroll(strDeviceCSR, strDeviceCert)) {
      Serial.println("EST SIMPLEENROLL OK");
      if (ConvertP7toPEM(strDeviceCert)) {
        Serial.println("CERT EXTRACTED FROM PKCS7 OK");
        if (updateCertFile())
          deviceStatus = DEV_STATUS_OPERATIONAL;
        else {
          Serial.println("ERROR SAVING CERT TO FLASH");
          deviceStatus = DEV_STATUS_ERROR;
        }
      } else {
        Serial.println("ERROR PROCESSING PKCS7");
        deviceStatus = DEV_STATUS_ERROR;
      }
    } else {
      Serial.println("EST SIMPLEENROLL ERROR");
      deviceStatus = DEV_STATUS_ERROR;
    }
  }

  showDeviceStatus();

  if (deviceStatus <= DEV_STATUS_ERROR) {
    Serial.println("IMPOSSIBLE TO OPERATE - HALTED");
    while(1) 
      delay(1000);
  }

  if (deviceStatus == DEV_STATUS_OPERATIONAL) {
    Serial.println("OPERATIONAL MODE - STARTING MQTT");
    setMQTT();
  }
  Serial.println();
  Serial.println("<<< SETUP END >>>");
  Serial.println();

  showDeviceStatus();
  temp.analogMeter(0, 0, 40, "oC", "0", "10", "20", "30", "40");
  hum.analogMeter(0, 119, 100, "%H", "0", "25", "50", "75", "100");


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
        
        temperature = am2320.readTemperature();
        humidity = am2320.readHumidity();

        payload = "{\"temperature\":" + String(temperature) + ", \"humidity\":" + String(humidity) + "}";
        client.publish(devPubTopic.c_str(), (char*) payload.c_str());
        Serial.println("PUBLISH!");

        temp.updateNeedle(temperature, 0);
        hum.updateNeedle(humidity, 0);
      }
    }
  }
}