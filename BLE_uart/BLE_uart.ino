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

BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;
uint8_t pressed = 0;
uint8_t rxCase = 0;

// RX values
RTC_DATA_ATTR int ledState = 0;
RTC_DATA_ATTR time_t t = 0;
RTC_DATA_ATTR int alarmHour = 0;
RTC_DATA_ATTR int alarmMinute = 0;
const char *ssid = "SuveSindi2";
const char *password = "pikkwifiparool";

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
                deserializeJson(doc, (String)rxValue.c_str());

            if (error) {
                Serial.println("parseObject() failed");
                return;
            }
            //assign parsed values
            const char *led = doc["ledState"];
            if (led == "+") {
                        Serial.println("recevied LED state: " + String(led));
                        ledState = 1;
                        digitalWrite(2, HIGH);
                    } else {
                        Serial.println("received LED state: " + String(led));
                        ledState = 0;
                        digitalWrite(2, LOW);
                    }
            t = doc["currentTime"].as<time_t>();
            Serial.println("recevied time: " + String(t));
            setTime(t);
            alarmHour = doc["alarmHour"];
            Serial.println("recevied hour: " + String(alarmHour));
            alarmMinute = doc["alarmMinute"];
            Serial.println("recevied minute: " + String(alarmMinute));
            ssid = doc["ssid"];
            password = doc["password"];

            Serial.println("ledstate: " + String(ledState) + " currentTime: " + String(t) + " alarmhour: " + String(alarmHour) + " alarmminute: " + String(alarmMinute));

/* 
            switch (rxCase) {
                case 0:
                    if (rxValue == "+") {
                        Serial.println("recevied LED state: " +
                                       (String)rxValue.c_str());
                        ledState = 1;
                        digitalWrite(2, HIGH);
                    } else {
                        Serial.println("received LED state: " +
                                       (String)rxValue.c_str());
                        ledState = 0;
                        digitalWrite(2, LOW);
                    }
                    rxCase++;
                    break;
                case 1:
                    Serial.println("received time: " + (String)rxValue.c_str());
                    t = atol(rxValue.c_str());
                    setTime(t);
                    rxCase++;
                    break;
                case 2:
                    Serial.println("received alarm hour: " +
                                   (String)rxValue.c_str());
                    // TODO set alarm hour
                    rxCase++;
                    break;
                case 3:
                    Serial.println("received alarm minute: " +
                                   (String)rxValue.c_str());
                    // TODO set alarm minute
                    rxCase++;
                    break;
                case 4:
                    Serial.println("received ssid : " +
                                   (String)rxValue.c_str());
                    ssid = (String)rxValue.c_str();
                    rxCase++;
                    break;
                case 5:
                    Serial.println("received password : " +
                                   (String)rxValue.c_str());
                    password = (String)rxValue.c_str();

                    rxCase++;
                    break;
                case 6:

                    rxCase = 0;
            }
 */
            //        Serial.println("Sending time confirmation");
            // pTxCharacteristic->setValue(std::string(t));
            // pTxCharacteristic->notify();
            /*
                        if (rxValue == "+") {
                            Serial.println("recevied LED state: " +
                                           (String)rxValue.c_str());
                            ledState = 1;
                            digitalWrite(2, HIGH);
                        } else if (rxValue == "-") {
                            Serial.println("received LED state: " +
                                           (String)rxValue.c_str());
                            ledState = 0;
                            digitalWrite(2, LOW);
                        } else if (rxValue.length() > 3 && isNnumber(rxValue)) {
                            Serial.println("received time: " +
               (String)rxValue.c_str()); t = atol(rxValue.c_str()); setTime(t);
                        } else if (rxValue.length() < 4 &&
                                   (String)rxValue[rxValue.length() - 1] == "h")
               { Serial.println("received alarm hour: " +
                                           (String)rxValue.c_str());
                            // TODO set alarm hour
                        } else if (rxValue.length() < 4 &&
                                   (String)rxValue[rxValue.length() - 1] == "m")
               { Serial.println("received alarm minute: " +
                                           (String)rxValue.c_str());
                            // TODO set alarm minute
                            Serial.println("Going to sleep now");
                            esp_deep_sleep_start();
                        } else if
             */

            /*
            if (deviceConnected && rxValue != previousRxValue) {
                previousRxValue = rxValue;
                Serial.println("Sending change confirmation");
                uint8_t confirm = 1;
                pTxCharacteristic->setValue(&confirm, 1);
                pTxCharacteristic->notify();
            }
            */

            // print received value
            /* Serial.print("Received value: ");
            for (int i = 0; i < rxValue.length(); i++) {
                Serial.print(rxValue[i]);
            }
            Serial.println(); */

            // sleep after alarm is set
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
    digitalClockDisplay();

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
