#ifndef BLE_DEIFINITIONS_H
#define BLE_DEIFINITIONS_H
#include "Arduino.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define UUID_SERVICE_MYWIFI "1ba1a021-3ca0-4c92-8739-8fd7b53473f0"
#define UUID_CHARACTERISTIC_MYWIFI_SSID "1ba1a022-3ca0-4c92-8739-8fd7b53473f0"
#define UUID_CHARACTERISTIC_MYWIFI_PASS "1ba1a023-3ca0-4c92-8739-8fd7b53473f0"
string sTheSsid = "";
string sThePwd = "";
bool deviceConnected = false;
////BLE CALL BACK
class SSID_Callbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    string value = pCharacteristic->getValue();

    if (value.length() > 0) {
      Serial.println("*********");
      Serial.print("New value: ");
      sTheSsid = value;
      for (int i = 0; i < value.length(); i++) {
        Serial.print(value[i]);
      }

      Serial.println();
      Serial.println("*********");
    }
  }
};
class PWD_Callbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    string value = pCharacteristic->getValue();

    if (value.length() > 0) {
      Serial.println("*********");
      Serial.print("New value: ");
      sThePwd = value;
      for (int i = 0; i < value.length(); i++) {
        Serial.print(value[i]);
      }

      Serial.println();
      Serial.println("*********");
    }
  }
};
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
    BLEDevice::startAdvertising();
  };

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    Serial.println("Disconnected");
  }
};
#endif