/*
   Program for isolating and publishing smartmeter sml messages to a mqtt broker by using a esp8266.
   Version 1 for nodeMCU or Adafruit Huzzah boards

   @author Tim Abels <rollercontainer@googlemail.com>
   @see The GNU Public License (GPL)

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
   for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, see <http://www.gnu.org/licenses/>.
*/


#define RX_PIN 4 // for NodeMCU: GPIO4 = D1 
#define TX_PIN 5 // for NodeMCU: GPIO5 = D2
#define MQTT_MAX_PACKET_SIZE 1024 // Maximum packet size (mqtt max = 4kB)
#define MQTT_KEEPALIVE 120 // keepAlive interval in Seconds

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

// Software serial is needed for debuging reasons.
// esp8266 has only one real hardware serial which is connected to usb
#include <SoftwareSerial.h> // https://github.com/plerup/espsoftwareserial
#include <PubSubClient.h>   // https://github.com/knolleary/pubsubclient/blob/master/examples/mqtt_esp8266/mqtt_esp8266.ino

#include "Config.h" // make your own config file or remove this line and use the following lines
//const char* clientId = "smartmeter";
//const char* mqtt_server = "192.168.x.y";
//const char* ssid = "PUT-YOUR-SSID-HERE";
//const char* password = "PUT-YOUR-WIFI-PASSWORD-HERE";
//IPAddress ip(192, 168, x, y); // Static IP
//IPAddress dns(192, 168, x, y); // most likely your router
//IPAddress gateway(192, 168, x, y); // most likely your router
//IPAddress subnet(255, 255, 255, 0);

byte inByte; // for reading from serial
byte smlMessage[700]; // for storing the the isolated message. Mine was 280 bytes, but may vary...
const byte startSequence[] = { 0x1B, 0x1B, 0x1B, 0x1B, 0x01, 0x01, 0x01, 0x01 }; // see sml protocol
const byte stopSequence[]  = { 0x1B, 0x1B, 0x1B, 0x1B, 0x1A };

bool connectedToMQTT = false;

int smlIndex;     // represents the actual position in smlMessage
int startIndex;   // for counting startSequence hits
int stopIndex;    // for counting stopSequence hits
int stage;        // defines what to do next. 0 = searchStart, 1 = searchStop, 2 = publish message

SoftwareSerial infraredHead( RX_PIN, TX_PIN, false, 256); // RX, TX, Inverse, Buffer
WiFiClient espClient;
PubSubClient mqttClient(espClient);

void setup_wifi() {
  delay(10);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.config(ip, dns, gateway, subnet);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // not used in this example
}

void mqttReconnect() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect(clientId)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      char* topic = "/energy/status";
      char* path = (char *) malloc(1 + strlen(clientId) + strlen(topic) );
      strcpy(path, clientId);
      strcat(path, topic);
      mqttClient.publish(path, "online");
      // ... and resubscribe
      //mqttClient.subscribe("smartmeter/inTopic");
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nHardware serial started");
  infraredHead.begin(9600);
  Serial.println("\nSoftware serial started");
  setup_wifi();
  mqttClient.setServer(mqtt_server, 1883);
  mqttClient.setCallback(mqttCallback);
  // --------------------------------------------------------------------- OTA

  // Port defaults to 8266
  // ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(clientId);

  // No authentication by default
  ArduinoOTA.setPassword((const char *)"08154711");

  ArduinoOTA.onStart([]() {
    Serial.println("Start");
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

void loop() {
  ArduinoOTA.handle();
  if (!mqttClient.connected()) {
    mqttReconnect();
  }
  mqttClient.loop();

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
}
void findStartSequence() {
  while (infraredHead.available())
  {
    inByte = infraredHead.read();
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
  while (infraredHead.available())
  {
    inByte = infraredHead.read();
    smlMessage[smlIndex] = inByte;
    smlIndex++;

    if (inByte == stopSequence[stopIndex])
    {
      stopIndex++;
      if (stopIndex == sizeof(stopSequence))
      {
        stage = 2;
        stopIndex = 0;

        // after the stop sequence, ther are sill 3 bytes to come.
        // One for the amount of fillbytes plus two bytes for calculating CRC.
        delay(30); // wait for the 3 bytes
        for (int c = 0 ; c < 3 ; c++) {
          smlMessage[smlIndex++] = infraredHead.read();
        }
        smlIndex--;
      }
    }
    else {
      stopIndex = 0;
    }
  }
}

void publishMessage() {

  int arrSize = 2 * smlIndex + 1;
  char smlMessageAsString[arrSize];
  char *myPtr = &smlMessageAsString[0]; //or just myPtr=charArr; but the former described it better.

  for (int i = 0; i <= smlIndex; i++) {
    snprintf(myPtr, 3, "%02x", smlMessage[i]); //convert a byte to character string, and save 2 characters (+null) to charArr;
    myPtr += 2; //increment the pointer by two characters in charArr so that next time the null from the previous go is overwritten.
  }

  //Serial.println(smlMessageAsString); // for debuging
  char* topic = "/energy/sml";
  char* path = (char *) malloc(1 + strlen(clientId) + strlen(topic) );
  strcpy(path, clientId);
  strcat(path, topic);
  mqttClient.publish(path, smlMessageAsString);
  memset(smlMessage, 0, sizeof(smlMessage)); // clear the buffer
  smlIndex = 0;
  stage = 0; // start over
}

