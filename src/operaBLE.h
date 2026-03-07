#include "config.h"
#if !defined(_OPERA_BLE_) && defined(USAR_ESP32_UART_BLE)
    #define _OPERA_BLE_

    // NimBLE-Arduino: substitui BLEDevice.h, BLEServer.h, BLEUtils.h, BLE2902.h e BLESecurity.h
    #include <NimBLEDevice.h>

    #include "operacional.h"
    #include "operaBLESecurity.h"

    #define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"  // UART service UUID
    #define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
    #define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

    // Flag global para rastrear autenticação BLE
    extern bool deviceAuthenticated;

    void setupBLE();
    void enviaBLE( String msg );
    bool isDeviceAuthenticated();
    void setDeviceAuthenticated(bool authenticated);

#endif
