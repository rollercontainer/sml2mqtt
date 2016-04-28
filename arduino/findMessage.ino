#define RX_PIN 10
#define TX_PIN 11

#include <SoftwareSerial.h>

SoftwareSerial infraredHead(RX_PIN, TX_PIN);

byte inByte;
byte smlMessage[1000];
const byte startSequence[] = { 0x1B, 0x1B, 0x1B, 0x1B, 0x01, 0x01, 0x01, 0x01 };
const byte stopSequence[]  = { 0x1B, 0x1B, 0x1B, 0x1B, 0x1A };

int smlIndex;
int startIndex;
int stopIndex;
int stage;

void setup() {

  Serial.begin(115200);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }
  infraredHead.begin(9600);
}

void loop() {

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
        
        // after the stop sequence, there are sill 3 bytes to come. 
        // One for the amount of fillbytes plus two bytes for calculating CRC.
        
        for (int c = 0 ; c < 3 ; c++) {
          smlMessage[smlIndex++] = infraredHead.read();
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

    int charArraySize = 2 * smlIndex + 1; // two ASCII's needed for one byte: EF => {'E','F'} + 1 for termination
    char smlMessageAsString[charArraySize];
    char *myPtr = &smlMessageAsString[0]; //or just myPtr=charArr; but the former described it better.
  
    for (int i = 0; i <= smlIndex; i++) {
      snprintf(myPtr, 3, "%02x", smlMessage[i]); //convert a byte to character string, and save 2 characters (+null) to charArr;
      myPtr += 2; //increment the pointer by two characters in charArr so that next time the null from the previous go is overwritten.
    }

  Serial.println(smlMessageAsString);
  memset(smlMessage, 0, sizeof(smlMessage)); // clear the byte buffer
  smlIndex = 0;
  stage = 0; // start over
}
