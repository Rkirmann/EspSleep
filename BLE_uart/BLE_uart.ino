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
#include <string.h>

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org");
const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 7200;
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
    byte ledState;
    time_t t;
    byte alarmHour;
    byte alarmMinute;
    char ssid[32];
    char password[63];
    bool weekDays[7];
};

RTC_DATA_ATTR RTC rtc;

int ledState;
time_t t;
int alarmHour;
int alarmMinute;
const char *ssid;
const char *password;
bool weekDays[7];

std::string previousRxValue = "";

// sleep timer
const long sleepTime = 60000;  // go to sleep after ms
boolean sleepOn = false;
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
            DeserializationError error = deserializeJson(doc, rxValue);

            if (error) {
                Serial.println("parseObject() failed");
                return;
            }

            // assign parsed values
            ledState = doc["ledState"].as<int>();
            Serial.println("set LED state: " + String(ledState));
            digitalWrite(2, ledState);

            t = doc["currentTime"].as<time_t>();
            Serial.println("set time: " + String(t));
            setTime(t);

            alarmHour = doc["alarmHour"].as<int>();
            Serial.println("set alarm hour: " + String(alarmHour));

            alarmMinute = doc["alarmMinute"].as<int>();
            Serial.println("set alarm minute: " + String(alarmMinute));

            ssid = doc["ssid"].as<const char *>();
            password = doc["password"].as<const char *>();
            Serial.println("set credentials: " + String(ssid) + ";" +
                           String(password));

            for (int i = 0; i < 7; i++) {
                weekDays[i] = doc["weekDays"][i];
                Serial.print("day" + String(i + 1) + ": " +
                             String(weekDays[i]) + "; ");
            }
            Serial.println();

            // Connect to Wi-Fi
            connectWifi();
            WiFi.disconnect(true);
            WiFi.mode(WIFI_OFF);
            sleepOn = true;

            //        Serial.println("Sending time confirmation");
            // pTxCharacteristic->setValue(std::string(t));
            // pTxCharacteristic->notify();

            if (deviceConnected && rxValue != previousRxValue) {
                previousRxValue = rxValue;
                Serial.println("Sending change confirmation");
                uint8_t confirm = 1;
                pTxCharacteristic->setValue(&confirm, 1);
                pTxCharacteristic->notify();
            }
        }
    }
};

void connectWifi() {
    int tries = 0;
    if (String(ssid) != "") {
        Serial.print("Connecting to ");
        Serial.println(ssid);
        WiFi.begin(ssid, password);
        while (WiFi.status() != WL_CONNECTED) {
            if (tries >= 5) {
                break;
            }
            delay(500);
            Serial.print(".");
            tries++;
        }
        Serial.println("");
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("WiFi connected.");
            setTimeOnline();
        } else {
            Serial.println("unable to connect WiFi.");
        }
    } else {
        Serial.println("no WiFi credentials found");
    }
}

void setTimeOnline() {
    Serial.println("setting time");
    // get set time from server
    timeClient.begin();
    timeClient.setTimeOffset(gmtOffset_sec);
    timeClient.update();
    // Serial.println(timeClient.getEpochTime());
    t = timeClient.getEpochTime();
    setTime(t);
    digitalClockDisplay();
}

void callback() {
    // placeholder callback function
}

void checkAlarm() {
    // weekday() sunday = 1
    int index = weekday() == 1 ? 6 : weekday() - 2;
    if (ledState == 1 && weekDays[index] && alarmHour == hour() &&
        alarmMinute == minute()) {
        alarmWake();
    }
}
void alarmWake() {
    // TODO smooth transition
    Serial.println("alarm triggered");
    // digitalWrite(2, ledState);

    int brightness = 0;
    //minutes to us
    int duration = 0.5 * 60000;
    // linear fade in calc
    float R = (255 * log10(2)) / (log10(255));

    unsigned long delayIncrement = duration / 255;
    for (int fadeValue = 0; fadeValue <= 255; fadeValue++) {
        // linear brightness calculation
        brightness = pow(2, (fadeValue / R)) - 1;
        //analogWrite(2, brightness);
        ledcWrite(0, brightness);
        delay(delayIncrement);
    }
}

void saveData() {
    Serial.println("Saving data");
    rtc.ledState = ledState;
    rtc.t = now();
    rtc.alarmHour = alarmHour;
    rtc.alarmMinute = alarmMinute;
    for (int i = 0; i < 32; i++) {
        rtc.ssid[i] = ssid[i];
    }
    for (int i = 0; i < 63; i++) {
        rtc.password[i] = password[i];
    }
    // strcpy(rtc.ssid, ssid);
    // strcpy(rtc.password, password);

    // memcpy(rtc.weekDays, weekDays, 7);
    for (int i = 0; i < 7; i++) {
        rtc.weekDays[i] = weekDays[i];
    }

    Serial.println("rtc credentials:");
    Serial.println(rtc.ssid);
    Serial.println(rtc.password);
}

void digitalClockDisplay() {
    // digital clock display of the time
    Serial.print(hour());
    printDigits(minute());
    printDigits(second());
    Serial.print(" ");
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
    ledState = rtc.ledState;
    t = rtc.t;
    alarmHour = rtc.alarmHour;
    alarmMinute = rtc.alarmMinute;
    ssid = rtc.ssid;
    password = rtc.password;
    for (int i = 0; i < 7; i++) {
        weekDays[i] = rtc.weekDays[i];
    }

    Serial.begin(115200);

    // builtin led
    // pinMode(2, OUTPUT);
    // configure LED PWM functionalitites
    ledcSetup(0, 5000, 8);
    // attach the channel to the GPIO to be controlled
    ledcAttachPin(2, 0);

    Serial.println("ledstate:" + String(ledState));
    Serial.println("time:" + String(t));
    Serial.println("alarmhour:" + String(alarmHour));
    Serial.println("alarmminute:" + String(alarmMinute));
    Serial.println("ssid:" + String(ssid));
    Serial.println("pass:" + String(password));
    for (int i = 0; i < 7; i++) {
        Serial.print("day" + String(i + 1) + ": " + String(weekDays[i]) + "; ");
    }
    Serial.println();

    // Setup interrupt on Touch Pad 0 (GPIO2)
    touchAttachInterrupt(T0, callback, Threshold);
    // Configure Touchpad as wakeup source
    esp_sleep_enable_touchpad_wakeup();
    // set sleep wakeup after us
    esp_sleep_enable_timer_wakeup(sleepTime * 1000);

    if (esp_sleep_get_wakeup_cause() != ESP_SLEEP_WAKEUP_TOUCHPAD) {
        Serial.println("Checking time and going to sleep");
        setTime(t + 60);
        connectWifi();
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        checkAlarm();
        esp_deep_sleep_start();
    } else {
        // TODO figure out how to set time offline when button wakeup
        setTime(t);
        connectWifi();
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        checkAlarm();
    }

    digitalClockDisplay();

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

    // if (deviceConnected) {
    //       uint8_t btn = touchRead(T0);
    //       btn > 40 ? pressed = 0 : pressed = 1;
    //       //Serial.println(btn);
    //         //pTxCharacteristic->setValue(&txValue, 1);
    //         pTxCharacteristic->setValue(&pressed, 1);
    //         pTxCharacteristic->notify();
    //         //txValue++;
    //         }
}

void loop() {
    digitalClockDisplay();
    Alarm.delay(1000);  // wait one second between clock display

    // go to sleep after some time after received bluetooth data
    previousMillis = millis();
    if ((previousMillis >= sleepTime || sleepOn) /* && second() == 0 */) {
        // t = now();
        saveData();
        digitalClockDisplay();
        checkAlarm();
        Serial.println("Going to sleep now");
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
