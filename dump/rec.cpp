
#include <LoRaLib.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <Fonts/FreeSans9pt7b.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <EEPROM.h>

#define ss 5
#define rst 14
#define dio0 2
#define dio1 3

#define fLoc 0
#define eLoc 1
#define rLoc 2
#define tLoc 3

#define EEPROM_SIZE 8 // No. of bytes to access from EEPROM

#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID. Device 1
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

void performBluetooth();
String getID(int memLoc);
void receivePacket();
void parsePacketRX(String str);
void parsePacketTX(String str);
void transmitPacket();
void displayStatus(int capacity);

int motor = 25;
bool motor_state = false; // flag to check motor_state.
byte transmitCount = 0;

int wave = 32;
Adafruit_SH1106G display = Adafruit_SH1106G(128, 64, &Wire, -1);
SX1276 lora = new LoRa(ss, dio0, dio1);

BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;

uint8_t FV = 0;
uint8_t EV = 100;
String RXID = ""; // Receiver ID.
String TXID = ""; // Transmitter ID.


int capacity;

int dRun[5] = {0};
byte dRunCount = 0;


class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();
      char arr[3];
            
      if (rxValue.length() > 0) {
        Serial.println("*********");
        Serial.print("Received Value: ");
        for (int i = 0; i < rxValue.length(); i++){
          Serial.print(rxValue[i]);       
        }
        if(rxValue[0] == 'F' && rxValue[1] == 'V'){
          for(int i = 2; i < rxValue.length(); i++)
              arr[i-2] = rxValue[i];
          FV = atoi(arr);
          EEPROM.write(fLoc,FV);
          EEPROM.commit();
        }
        if(rxValue[0] == 'E' && rxValue[1] == 'V'){
          for(int i = 2; i < rxValue.length(); i++)
              arr[i-2] = rxValue[i];
          EV = atoi(arr);
          EEPROM.write(eLoc,EV);
          EEPROM.commit();
        }
        if(rxValue[0] == 'R' && rxValue[1] == 'X' && rxValue[2] == 'I' && rxValue[3] == 'D'){
          for(int i = 4; i < rxValue.length(); i++)
              arr[i-4] = rxValue[i];
          EEPROM.write(rLoc,atoi(arr));
          EEPROM.commit();          
        }
        if(rxValue[0] == 'T' && rxValue[1] == 'X' && rxValue[2] == 'I' && rxValue[3] == 'D'){
          for(int i = 4; i < rxValue.length(); i++)
              arr[i-4] = rxValue[i];
          EEPROM.write(tLoc,atoi(arr));
          EEPROM.commit();
        }

        Serial.print("EV: ");        
        Serial.println(EV);
        Serial.print("FV: ");        
        Serial.println(FV);
        Serial.println();
        Serial.println("*********");
      }
      vTaskDelay(500);
      
    }
};

void setup() {
  Serial.begin(9600);
  pinMode(motor,OUTPUT);
  pinMode(wave,INPUT);

  EEPROM.begin(EEPROM_SIZE);

  RXID = getID(rLoc);
  TXID = getID(tLoc);
  FV = EEPROM.read(fLoc);
  EV = EEPROM.read(eLoc);

  display.begin(0x3C, true);
  display.display();
  Serial.print(F("Initializing ... "));
  // carrier frequency:           434.0 MHz
  // bandwidth:                   125.0 kHz
  // spreading factor:            9
  // coding rate:                 7
  // sync word:                   0x12
  // output power:                17 dBm
  // current limit:               100 mA
  // preamble length:             8 symbols
  // amplifier gain:              0 (automatic gain control)
  int state = lora.begin(865.0, 10.4, 9,5 , 0x12, 17,100,8,6 );
  if (state == ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true);
  }
  digitalWrite(motor,LOW);
  // Create the BLE Device
  BLEDevice::init("Receiver_1");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(
                    CHARACTERISTIC_UUID_TX,
                    BLECharacteristic::PROPERTY_NOTIFY
                  );
                      
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(
                       CHARACTERISTIC_UUID_RX,
                      BLECharacteristic::PROPERTY_WRITE
                    );

  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");

}

void loop() {
        Serial.print("FV: ");        
        Serial.println(FV);
        Serial.print("EV: ");        
        Serial.println(EV);
        Serial.print("TXID: ");        
        Serial.println(TXID);
        Serial.print("RXID: ");        
        Serial.println(RXID);

  performBluetooth(); // keep performing bluetooth. Can have some disabling logic later on.
  
  receivePacket(); // Receive packet continuously.
//
//  if(motor_state && transmitCount < 3)
//  {
//    // transmit packet three times if motor is on.
//    transmitPacket();
//    transmitCount ++;
//  }
//  else if (!motor_state)
//  {
//    transmitCount = 0;
//  }
//  if(isInDryRun() && motor_state)
//  {
//    digitalWrite(motor,LOW);
//    Serial.println("MOTOR OFF");
//    motor_state = false;
//  }
//  delay(1000);
}

void performBluetooth()
{
    Serial.println("Inside Perform Bluetooth");
  if (deviceConnected) 
  {
    //Do Actions only when Bluetooth is connected
  }
  // disconnecting
  if (!deviceConnected && oldDeviceConnected) 
  {
    delay(500); // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
    Serial.println("start advertising");
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected) 
  {
    // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }
}

String getID(int memLoc)
{
  String id = "";
  if(memLoc == rLoc){
    id = "R";
  }
  else if(memLoc == tLoc){
    id = "T";
  }

  String temp = static_cast<String>(EEPROM.read(memLoc));

  if(temp.length() == 3)
  {
    id = id + temp;
  }
  else if(temp.length() == 2)
  {
    id = id + "0" + temp;
  }
  else if(temp.length() == 1)
  {
    id = id + "00" + temp;
  }
  return id;
}

void setID(int memLoc, int ID)
{
  EEPROM.write(memLoc,ID);
  EEPROM.commit();
}

void receivePacket()
{
  Serial.println("Inside Receive Packet");
  Serial.print(F("Waiting for incoming transmission ... "));

  String str;
  int state = lora.receive(str);
  Serial.println(state);
  Serial.println(str);
  if (state == ERR_NONE) 
  {
    String key_val = str.substring(0,4);

    if(key_val == RXID)
    {
      parsePacketRX(str);
    }
    
    else if(key_val == TXID)
    {
      parsePacketTX(str.substring(4,7));
    }
  }
  else if (state == ERR_RX_TIMEOUT) 
  {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SH110X_WHITE);
      display.setFont(&FreeSans9pt7b);
      display.setCursor(6,15);
      display.fillRect(0, 0, 127, 20,1);
      display.setTextColor(SH110X_BLACK);
      display.print("Not Connected\n");
      display.display();
      
      // timeout occurred while waiting for a packet
      Serial.println(F("timeout!"));
  }   
  else if (state == ERR_CRC_MISMATCH) 
  {
      // packet was received, but is malformed
      Serial.println(F("CRC error!"));
  }



}

void parsePacketRX(String str)
{
  // Switch motor ON if Motor ON packet is received.
  String key = RXID + "1";
  if(str == key && !motor_state)
  {
    digitalWrite(motor,HIGH);
    Serial.println("MOTOR ONNNNNNNNNNN");
  }
}

void parsePacketTX(String str) 
{
  // packet was successfully received
  Serial.println(F("success!"));

  // print data of the packet
  Serial.print(F("Data:\t\t\t"));
  Serial.println(str);
  int cap = (str.substring(4,str.length())).toInt();
  capacity = map(cap,FV,EV,100,0);

  // print RSSI (Received Signal Strength Indicator)
  // of the last received packet
  Serial.print(F("RSSI:\t\t\t"));
  Serial.print(lora.getRSSI());
  Serial.println(F(" dBm"));

  // print SNR (Signal-to-Noise Ratio)
  // of the last received packet
  Serial.print(F("SNR:\t\t\t"));
  Serial.print(lora.getSNR());
  Serial.println(F(" dB"));

  // print frequency error
  // of the last received packet
  Serial.print(F("Frequency error:\t"));
  Serial.print(lora.getFrequencyError());
  Serial.println(F(" Hz"));
  //
  if (digitalRead(wave) == HIGH)
  {
      digitalWrite(motor,HIGH);
      Serial.println("MOTOR ONNNNNNNNNNN");
  }
  else
  {
  digitalWrite(motor,LOW);
  Serial.println("MOTOR OFF");
  }
  displayStatus(capacity);
}

void transmitPacket() // can just be an inline function.
{
  Serial.print(F("Sending packet ... "));

  int state = lora.transmit(RXID + "1");
  
  // Below code is only for checking purposes.
  if (state == ERR_NONE) {
  // the packet was successfully transmitted
  Serial.println(F(" success!"));

  // print measured data rate
  Serial.print(F("Datarate:\t"));
  Serial.print(lora.getDataRate());
  Serial.println(F(" bps"));

  } else if (state == ERR_PACKET_TOO_LONG) {
  // the supplied packet was longer than 256 bytes
  Serial.println(F(" too long!"));

  } else if (state == ERR_TX_TIMEOUT) {
  // timeout occurred while transmitting packet
  Serial.println(F(" timeout!"));

  }
}

void displayStatus(int capacity)
{
  //////////////////////////////////
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setFont(&FreeSans9pt7b);
  display.setCursor(6,15);
  //display.fillRect(0, 0, 127, 20,1);
  //display.setTextColor(SH110X_BLACK);
  //display.print("Tank Capacity\n");
  
  display.drawLine(0, display.height(), 0, 11, SH110X_WHITE);//    ......................|
  display.drawLine(0, display.height()-1, 60, display.height()-1, SH110X_WHITE); //.........__
  display.drawLine(60, display.height(), 60, 11, SH110X_WHITE);//  .........................|
  display.drawLine(0, 11, 20, 1, SH110X_WHITE); //......................................../
  display.fillRect(20, 0, 20, 3,1);//....................................................==
  display.drawLine(40, 1, 60, 11, SH110X_WHITE); //.........................\\
  display.drawLine(1, 11, 59, 11, SH110X_WHITE); //.........---
  
  display.drawLine(85, 52, display.width(), 52, SH110X_WHITE);
  display.display();
  display.fillRect(65, 0, 66, 20,1);
  display.setCursor(67, 15);
  display.setTextColor(SH110X_BLACK);
  display.print("Volume");
  display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);
  if(capacity<10)
  display.setCursor(84, 47);
  else if (capacity <100)
  display.setCursor(72, 47);
  else
  display.setCursor(60, 47);
  display.print(capacity);
  int tankfill = capacity/2;
  display.fillRect(2,62-tankfill , 57, tankfill,1);
  //display.fillRect(2,12 , 56, 50,1);
  display.setFont();
  display.setTextSize(1);
  display.println("%");
  display.display();
  ///////
  display.setCursor(90,40);
  //display.print(str);
  display.setTextSize(1);
  display.setFont();
  /* if (state == 0)
  {
  display.setCursor(0,54);
  display.print("Connected");
  }
  else
  { 
  display.setCursor(0,54);
  display.print("Not Connected");
  }*/
  display.drawLine(85, 52, 85, display.height(), SH110X_WHITE);
  display.drawLine(85, 52, display.width(), 52, SH110X_WHITE);
  display.fillRect(65, 52, 18, 20,1);
  display.setCursor(68,54);
  display.setTextColor(SH110X_BLACK);
  display.println("SG");
  display.setTextColor(SH110X_WHITE);
  display.setCursor(90,54);
  //display.print("\nRSSI ");
  display.println(lora.getRSSI());
  display.display();
  //////////////////////////////////
}
bool isInDryRun()
{
  dRun[dRunCount] = capacity;
  
  if(dRunCount >=4)
  {
    int sum = 0;
    float avg;
    for(int i = 0; i < 5; i++)
    {
      sum = sum + dRun[i];
    }
    avg = sum/5.0;
    if (capacity - avg <= 0.1)
    {
      return true;
    }
    else
    {
      dRunCount = 0;
    }

  }
  return false;
}
