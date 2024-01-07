#include <SPI.h>
#include <LoRa.h>
#include "BluetoothSerial.h"

//#define USE_PIN // Uncomment this to use PIN during pairing. The pin is specified on the line below
const char *pin = "1234";  // Change this to more secure PIN.

String device_name = "LoRa_Tranceiver";

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

#if !defined(CONFIG_BT_SPP_ENABLED)
#error Serial Bluetooth not available or not enabled. It is only available for the ESP32 chip.
#endif

BluetoothSerial SerialBT;

#define HSPI_MISO MISO
#define HSPI_MOSI MOSI
#define HSPI_SCLK SCK
#define HSPI_SS SS

#define HSPI_MISO 12
#define HSPI_MOSI 13
#define HSPI_SCLK 14
#define HSPI_SS 15

static const int spiClk = 1000000;  // 1 MHz

//uninitalised pointers to SPI objects
SPIClass *hspi = NULL;

int count, counter;
//define the pins used by the transceiver module
#define ss 15
#define rst 2
#define dio0 4

String outgoing;           // outgoing message
byte msgCount = 0;         // count of outgoing messages
byte localAddress = 0xAA;  // address of this device
byte destination = 0xBB;   // destination to send to
long lastSendTime = 0;     // last send time
int interval = 2000;       // interval between sends

void setup() {

  SerialBT.begin(device_name);  //Bluetooth device name
  Serial.printf("The device with name \"%s\" is started.\nNow you can pair it with Bluetooth!\n", device_name.c_str());
//Serial.printf("The device with name \"%s\" and MAC address %s is started.\nNow you can pair it with Bluetooth!\n", device_name.c_str(), SerialBT.getMacString()); // Use this after the MAC method is implemented
#ifdef USE_PIN
  SerialBT.setPin(pin);
  Serial.println("Using PIN");
#endif
  //initialise two instances of the SPIClass attached to VSPI and HSPI respectively
  hspi = new SPIClass(HSPI);
  //initialise hspi with default pins
  //SCLK = 14, MISO = 12, MOSI = 13, SS = 15
  hspi->begin();
  pinMode(hspi->pinSS(), OUTPUT);  //HSPI SS

  Serial.begin(115200);
  delay(2000);

  Serial.println("LoRa Tranceiver");
  LoRa.setSPI(*hspi);
  LoRa.setPins(ss, rst, dio0);
  while (!LoRa.begin(433E6)) {
    Serial.print(".");
    delay(500);
  }
  // Change sync word (0xF3) to match the receiver
  // The sync word assures you don't get LoRa messages from other LoRa transceivers
  // ranges from 0-0xFF
  // LoRa.setSyncWord(0xF3);
  // Serial.println("LoRa Initializing OK!");

  // LoRa.onReceive(onReceive);
  LoRa.receive();
  Serial.println("LoRa init succeeded.");
}

void sendMessage(String outgoing) {
  LoRa.beginPacket();             // start packet
  LoRa.write(destination);        // add destination address
  LoRa.write(localAddress);       // add sender address
  LoRa.write(msgCount);           // add message ID
  LoRa.write(outgoing.length());  // add payload length
  LoRa.print(outgoing);           // add payload
  LoRa.endPacket();               // finish packet and send it
  msgCount++;                     // increment message ID
}

void onReceive(int packetSize) {
  if (packetSize == 0) return;  // if there's no packet, return

  // read packet header bytes:
  int recipient = LoRa.read();        // recipient address
  byte sender = LoRa.read();          // sender address
  byte incomingMsgId = LoRa.read();   // incoming msg ID
  byte incomingLength = LoRa.read();  // incoming msg length

  String incoming = "";  // payload of packet

  while (LoRa.available()) {        // can't use readString() in callback, so
    incoming += (char)LoRa.read();  // add bytes one by one
  }

  if (incomingLength != incoming.length()) {  // check length for error
    Serial.println("error: message length does not match length");
    return;  // skip rest of function
  }

  // if the recipient isn't this device or broadcast,
  if (recipient != localAddress && recipient != 0xFF) {
    Serial.println("This message is not for me.");
    return;  // skip rest of function
  }

  // if message is for this device, or broadcast, print details:
  Serial.println("Received from: 0x" + String(sender, HEX));
  Serial.println("Sent to: 0x" + String(recipient, HEX));
  Serial.println("Message ID: " + String(incomingMsgId));
  Serial.println("Message length: " + String(incomingLength));
  Serial.println("Message: " + incoming);
  Serial.println("RSSI: " + String(LoRa.packetRssi()));
  Serial.println("Snr: " + String(LoRa.packetSnr()));
  Serial.println();
  SerialBT.print(incoming);
}

void loop() {
  // LoRa.receive();
  String FromLoRa_2_BT = "";

  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    // received a packet
    Serial.print("Received packet ");

    // read packet
    while (LoRa.available()) {
      FromLoRa_2_BT += (char)LoRa.read();
      // SerialBT.print(LoRa.read());
    }
    
    Serial.print("Received packet ");
    Serial.print(FromLoRa_2_BT);
    // SerialBT.print("some shit");
    for (int i = 0; i<FromLoRa_2_BT.length(); i++)
    {
      SerialBT.write(FromLoRa_2_BT[i]);
    }
    // SerialBT.write(FromLoRa_2_BT);
    // print RSSI of packet
    Serial.print("' with RSSI ");
    Serial.println(LoRa.packetRssi());
  }

  String FromBT_2_LoRa = "";

  if (SerialBT.available()) {
    while (SerialBT.available()) {
      FromBT_2_LoRa += (char)SerialBT.read();
      // if (SerialBT.read() == 10);
    }
    Serial.println("Received via Bluetooth:");
    Serial.println(FromBT_2_LoRa);
    LoRa.beginPacket();
    LoRa.print(FromBT_2_LoRa);
    LoRa.print(counter);
    LoRa.endPacket();
    // delay(1000);
    counter++;
  }

  // String message = "";
  // if (SerialBT.available()) {
  //   message = (SerialBT.read());
  // }
  // sendMessage(message);
  // // delay(1000);
  // Serial.println("Sending " + message);
  // //
  // LoRa.receive();  // go back into receive mode
  // delay(1000);
}
