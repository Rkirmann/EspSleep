/*
    Video: https://www.youtube.com/watch?v=oCMOYS71NIU
    Based on Neil Kolban example for IDF:
   https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
    Ported to Arduino ESP32 by Evandro Copercini

   Create a BLE server that, once we receive a connection, will send periodic
   notifications. The service advertises itself as:
   6E400001-B5A3-F393-E0A9-E50E24DCCA9E Has a characteristic of:
   6E400002-B5A3-F393-E0A9-E50E24DCCA9E - used for receiving data with "WRITE"
   Has a characteristic of: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E - used to send
   data with  "NOTIFY"

   The design of creating the BLE server is:
   1. Create a BLE Server
   2. Create a BLE Service
   3. Create a BLE Characteristic on the Service
   4. Create a BLE Descriptor on the characteristic
   5. Start the service.
   6. Start advertising.

   In this example rxValue is the data received (only accessible inside that
   function). And txValue is the data to be sent, in this example just a byte
   incremented every second.
*/
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <NTPClient.h>
#include <Time.h>
#include <TimeAlarms.h>
#include <WiFi.h>
#include <WiFiUdp.h>
// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
const char *ssid = "SuveSindi2";
const char *password = "pikkwifiparool";
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 10800;
const int daylightOffset_sec = 3600;

BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;
uint8_t pressed = 0;
RTC_DATA_ATTR time_t t = 0;
std::string previousRxValue = "";
#define Threshold 40
RTC_DATA_ATTR int ledState = 0;

// sleep timer
const long sleepTime = 60000;  // go to sleep after ms
unsigned long previousMillis = 0;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID \
    "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"  // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *pServer) { deviceConnected = true; };

    void onDisconnect(BLEServer *pServer) { deviceConnected = false; }
};
// BLE communicate
class MyCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        std::string rxValue = pCharacteristic->getValue();

        if (rxValue.length() > 0) {
            previousMillis = 0;

            if (rxValue == "1") {
                ledState = 1;
                digitalWrite(2, HIGH);
            } else if (rxValue == "0") {
                ledState = 0;
                digitalWrite(2, LOW);
            } else {
                //  Serial.println("time: " + (String) rxValue.c_str());
                //t = atol(rxValue.c_str());
                //setTime(t);
                ;
            }

            if (deviceConnected && rxValue != previousRxValue) {
                previousRxValue = rxValue;
                Serial.println("Sending change confirmation");
                uint8_t confirm = 1;
                pTxCharacteristic->setValue(&confirm, 1);
                pTxCharacteristic->notify();
            }

            // print received value
            Serial.println("*********");
            Serial.print("Received Value: ");
            for (int i = 0; i < rxValue.length(); i++) {
                Serial.print(rxValue[i]);
            }
            Serial.println();
            Serial.println("*********");

            // sleep after alarm is set
            Serial.println("Going to sleep now");
            esp_deep_sleep_start();
        }
    }
};

void callback() {
    // placeholder callback function
}
void AlarmWake() { Serial.println("alarm triggered at " + (String)millis()); }
void digitalClockDisplay() {
    // digital clock display of the time
    Serial.print(hour());
    printDigits(minute());
    printDigits(second());
    printDigits(day());
    printDigits(month());
    printDigits(year());
    Serial.println();
}
void printDigits(int digits) {
    Serial.print(":");
    if (digits < 10) Serial.print('0');
    Serial.print(digits);
}

void setup() {
    Serial.begin(115200);
    // setTime(t);
    // t = now();

    // Connect to Wi-Fi
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected.");

    // get set time
    timeClient.begin();
    timeClient.setTimeOffset(gmtOffset_sec);
    timeClient.update();
    // Serial.println(timeClient.getEpochTime());
    t = timeClient.getEpochTime();
    setTime(t);

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    // Serial.println("time is " + (String)hour() + ":" + (String)minute());
    // Alarm.alarmOnce(dowWednesday, 0, 0, 30, AlarmWake);
    // set sleep wakeup after us
    esp_sleep_enable_timer_wakeup(sleepTime*1000);

    // builtin led
    pinMode(2, OUTPUT);
    digitalWrite(2, ledState);
    // Setup interrupt on Touch Pad 0 (GPIO2)
    touchAttachInterrupt(T0, callback, Threshold);
    // Configure Touchpad as wakeup source
    esp_sleep_enable_touchpad_wakeup();

    // START BLUETOOTH
    // Create the BLE Device
    BLEDevice::init("UART Service");
    // Create the BLE Server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    // Create the BLE Service
    BLEService *pService = pServer->createService(SERVICE_UUID);
    // Create a BLE Characteristic
    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
    pTxCharacteristic->addDescriptor(new BLE2902());
    BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
    pRxCharacteristic->setCallbacks(new MyCallbacks());
    // Start the service
    pService->start();
    // Start advertising
    pServer->getAdvertising()->start();
    Serial.println("Waiting a client connection to notify...");
}

void loop() {
    digitalClockDisplay();
    Alarm.delay(1000);  // wait one second between clock display

    //previousMillis = millis();
    
    if (previousMillis >= sleepTime) {
        // Go to sleep now
        Serial.println("Going to sleep now");
        //t = now();
        esp_deep_sleep_start();
    }
    

    /*
        if (deviceConnected) {
          uint8_t btn = touchRead(T0);
          btn > 40 ? pressed = 0 : pressed = 1;
          //Serial.println(btn);
            //pTxCharacteristic->setValue(&txValue, 1);
            pTxCharacteristic->setValue(&pressed, 1);
            pTxCharacteristic->notify();
            //txValue++;

                    delay(10); // bluetooth stack will go into congestion, if
       too many packets are sent
            }
    */

    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(500);  // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising();  // restart advertising
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
        // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }
}
