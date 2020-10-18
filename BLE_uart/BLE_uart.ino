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
#include <ArduinoJson.h>
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
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 10800;
const int daylightOffset_sec = 3600;
int timeout = 0;

BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;
uint8_t pressed = 0;
uint8_t rxCase = 0;

// RX values
struct RTC {
    int ledState;
    time_t t;
    int alarmHour;
    int alarmMinute;
    const char *ssid;
    const char *password;
};

RTC_DATA_ATTR RTC rtc;

int ledState = rtc.ledState;
time_t t = rtc.t;
int alarmHour = rtc.alarmHour;
int alarmMinute = rtc.alarmMinute;
const char *ssid = rtc.ssid;
const char *password = rtc.password;

std::string previousRxValue = "";

// sleep timer
const long sleepTime = 60000;  // go to sleep after ms
unsigned long previousMillis = 0;
// wakeup interrup touch button
#define Threshold 40

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

            Serial.println("received json : " + (String)rxValue.c_str());
           

            DynamicJsonDocument doc(1024);
            DeserializationError error =
                deserializeJson(doc, rxValue);

            if (error) {
                Serial.println("parseObject() failed");
                return;
            }
             Serial.println(doc["ledState"].as<int>());
            Serial.println(String(doc["currentTime"].as<time_t>()));
            Serial.println( doc["alarmHour"].as<int>());
            Serial.println(doc["alarmMinute"].as<int>());
            Serial.println((String)doc["ssid"].as<const char*>());
            Serial.println((String)doc["password"].as<const char*>());

            // assign parsed values
            ledState = doc["ledState"].as<int>();
            Serial.println("set LED state: " + String(ledState));
            digitalWrite(2, ledState);

            t = doc["currentTime"].as<time_t>();
            Serial.println("set time: " + String(t));
            setTime(t);

            alarmHour = doc["alarmHour"].as<int>();
            Serial.println("set hour: " + String(alarmHour));

            alarmMinute = doc["alarmMinute"].as<int>();
            Serial.println("set minute: " + String(alarmMinute));

            ssid = doc["ssid"].as<const char*>();
            password = doc["password"].as<const char*>();
            Serial.println("set credentials: " + String(ssid) + ";" +
                           String(password));

            // Connect to Wi-Fi
            connectWifi();

            rtc.ledState = ledState;
            rtc.t = t;
            rtc.alarmHour = alarmHour;
            rtc.alarmMinute = alarmMinute;
            rtc.ssid = ssid;
            rtc.password = password;
            Serial.println("Going to sleep now");
            esp_deep_sleep_start();

            //        Serial.println("Sending time confirmation");
            // pTxCharacteristic->setValue(std::string(t));
            // pTxCharacteristic->notify();

            /*
            if (deviceConnected && rxValue != previousRxValue) {
                previousRxValue = rxValue;
                Serial.println("Sending change confirmation");
                uint8_t confirm = 1;
                pTxCharacteristic->setValue(&confirm, 1);
                pTxCharacteristic->notify();
            }
            */
        }
    }
};

void connectWifi() {
    if (String(ssid).length() > 1) {
        Serial.print("Connecting to ");
        Serial.println(ssid);
        WiFi.begin(ssid, password);
        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
        }
        Serial.println("");
        Serial.println("WiFi connected.");
    }
}

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
    
    Serial.println("ledstate:" + String(ledState));
    Serial.println("time:" + String(t));
    Serial.println("alarmhour:" + String(alarmHour));
    Serial.println("alarmminute:" + String(alarmMinute));
    Serial.println("ssid:" + String(ssid));
    Serial.println("pass:" + String(password));

    setTime(t);
    t = now();

    // Connect to Wi-Fi
    connectWifi();

    if (WiFi.status() == WL_CONNECTED) {
        // get set time
        timeClient.begin();
        timeClient.setTimeOffset(gmtOffset_sec);
        timeClient.update();
        // Serial.println(timeClient.getEpochTime());
        t = timeClient.getEpochTime();
        setTime(t);
        digitalClockDisplay();
    }

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    // Serial.println("time is " + (String)hour() + ":" + (String)minute());
    // Alarm.alarmOnce(dowWednesday, 0, 0, 30, AlarmWake);

    // builtin led
    pinMode(2, OUTPUT);
    digitalWrite(2, ledState);

    // Setup interrupt on Touch Pad 0 (GPIO2)
    touchAttachInterrupt(T0, callback, Threshold);
    // Configure Touchpad as wakeup source
    esp_sleep_enable_touchpad_wakeup();
    // set sleep wakeup after us
    esp_sleep_enable_timer_wakeup(sleepTime * 1000);

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

    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_TOUCHPAD) {
        Serial.println("Checking time and going to sleep");
        esp_deep_sleep_start();
    }
}

void loop() {
    digitalClockDisplay();
    Alarm.delay(1000);  // wait one second between clock display

    // go to sleep after some time
    previousMillis = millis();
    if (previousMillis >= sleepTime) {
        Serial.println("Going to sleep now");
        // t = now();
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
