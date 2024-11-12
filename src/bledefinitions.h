#ifndef BLE_DEIFINITIONS_H
#define BLE_DEIFINITIONS_H
#include "Arduino.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define UUID_SERVICE_MYWIFI "1ba1a021-3ca0-4c92-8739-8fd7b53473f0"
#define UUID_CHARACTERISTIC_MYWIFI_SSID "1ba1a022-3ca0-4c92-8739-8fd7b53473f0"
#define UUID_CHARACTERISTIC_MYWIFI_PASS "1ba1a023-3ca0-4c92-8739-8fd7b53473f0"
static char ssid[32];
static char pswd[63];
bool deviceConnected = false;
////BLE CALL BACK
class SSID_Callbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    string value = pCharacteristic->getValue();

    if (value.length() > 0) {
      strcpy(ssid, value.c_str());
      for (int i = 0; i < value.length(); i++) {
        Serial.print(value[i]);
      }
    }
  }
};
class PWD_Callbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    string value = pCharacteristic->getValue();

    if (value.length() > 0) {
      strcpy(pswd, value.c_str());
      for (int i = 0; i < value.length(); i++) {
        Serial.print(value[i]);
      }
    }
  }
};
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
    //BLEDevice::startAdvertising();
  };

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    Serial.println("Disconnected");
  }
};
#endif