#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <MQTT.h>

// create MQTT object

#define CLIENT_ID "smartmeter"
MQTT myMqtt(CLIENT_ID, "192.168.0.2", 1883);
boolean bIsConnected = false;

// MQTT stuff end

const char* ssid = "";
const char* password = "";

IPAddress ip(192, 168, 0, 250);
IPAddress dns(192, 168, 0, 1);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

byte inByte;
byte smlMessage[1000];
const byte startSequence[] = { 0x1B, 0x1B, 0x1B, 0x1B, 0x01, 0x01, 0x01, 0x01 };
const byte stopSequence[]  = { 0x1B, 0x1B, 0x1B, 0x1B, 0x1A };

int smlIndex;
int startIndex;
int stopIndex;
int stage;

void setup() {

  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output

  Serial.begin(9600);
  WiFi.config(ip, dns, gateway, subnet);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    delay(5000);
    ESP.restart();
  }
  ArduinoOTA.setHostname(CLIENT_ID);
  //ArduinoOTA.setPassword((const char *)"0000");
  ArduinoOTA.onStart([]() {});
  ArduinoOTA.onEnd([]() {});
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {});
  ArduinoOTA.onError([](ota_error_t error) {});
  ArduinoOTA.begin();

  // setup callbacks
  myMqtt.onConnected(myConnectedCb);
  myMqtt.onDisconnected(myDisconnectedCb);
  myMqtt.onPublished(myPublishedCb);
  myMqtt.onData(myDataCb);
}

void loop() {
  ArduinoOTA.handle();
  if (bIsConnected) {
    switch (stage) {
      case 0:
        findStartSequence();
        break;
      case 1:
        findStopSequence();
        break;
      case 2:
        publishMessage();
        break;
    }
  } else {
    // try to connect to mqtt server
    myMqtt.connect();
  }
}

void findStartSequence() {
  while (Serial.available())
  {
    inByte = Serial.read();
    if (inByte == startSequence[startIndex])
    {
      smlMessage[startIndex] = inByte;
      startIndex++;
      if (startIndex == sizeof(startSequence))
      {
        stage = 1;
        smlIndex = startIndex;
        startIndex = 0;
      }
    }
    else {
      startIndex = 0;
    }
  }
}

void findStopSequence() {
  while (Serial.available())
  {
    inByte = Serial.read();
    smlMessage[smlIndex] = inByte;
    smlIndex++;

    if (inByte == stopSequence[stopIndex])
    {
      stopIndex++;
      if (stopIndex == sizeof(stopSequence))
      {
        stage = 2;
        stopIndex = 0;

        // after the stop sequence passed, there are sill 3 bytes to come.
        // One for the amount of fillbytes plus two bytes for calculating CRC.
        delay(3); // wait for the 3 bytes to be received. 
        for (int c = 0 ; c < 3 ; c++) {
          smlMessage[smlIndex++] = Serial.read();
        }
        // smlIndex is at this point one bigger than the amount of stored inBytes because it is incremented evreytime after reading.
        // To store the real smlIndex, we have to substract the last incrementation.
        smlIndex--;
      }
    }
    else {
      stopIndex = 0;
    }
  }
}

void publishMessage() {

    int charArraySize = 2 * smlIndex + 1;
    char smlMessageAsString[charArraySize];
    char *myPtr = &smlMessageAsString[0]; // Load start address to pointer
  
    for (int i = 0; i <= smlIndex; i++) {
      snprintf(myPtr, 3, "%02x", smlMessage[i]); //convert a byte to character string, and save 2 characters (+null) to smlMessageAsString;
      myPtr += 2; //increment the pointer by two characters in charArr so that next time the null from the previous go is overwritten.
    }

  publishMqtt((String)smlMessageAsString);
  memset(smlMessage, 0, sizeof(smlMessage)); // clear the buffer
  smlIndex = 0;
  stage = 0; // start over
}


void publishMqtt(String strPayload) {
  myMqtt.publish(CLIENT_ID "/energy/raw", strPayload);
}

void myConnectedCb() {
  bIsConnected = true;
}

void myDisconnectedCb() {
  bIsConnected = false;
}

void myPublishedCb() {
}

void myDataCb(String & topic, String & data) {
}
