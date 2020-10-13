# EspSleep

EspSleep is an application developed targeting an audience of developers who are new to 
Bluetooth Low Energy. 
The Android application communicates with an ESP32 via bluetooth.
The ESP32 uses wifi to update it's time and sends data back to Android using bluetooth.

Uses Nordic Semiconductor's Android BLE Library as a base.
[Android BLE Library](https://github.com/NordicSemiconductor/Android-BLE-Library/) 
library can be used from View Model 
(see [Architecture Components](https://developer.android.com/topic/libraries/architecture/index.html)).



## Note

In order to scan for Bluetooth LE device the Location permission must be granted and, on some phones, 
the Location must be enabled. This app will not use the location information in any way.
