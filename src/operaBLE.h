#include "config.h"
#if !defined(_OPERA_BLE_) && defined(USAR_ESP32_UART_BLE)
    #define _OPERA_BLE_

    // NimBLE-Arduino: unico include necessario — substitui BLEDevice.h,
    // BLEServer.h, BLEUtils.h, BLE2902.h e BLESecurity.h da biblioteca antiga.
    #include <NimBLEDevice.h>

    #include "operacional.h"

    // Nordic UART Service (NUS) — UUIDs padrao de servico BLE UART
    #define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
    #define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
    #define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

    // Flag global para rastrear autenticacao BLE
    extern bool deviceAuthenticated;

    // --- Funcoes BLE publicas ---
    void setupBLE();
    void enviaBLE(String msg);
    bool isDeviceAuthenticated();
    void setDeviceAuthenticated(bool authenticated);

    // --- Funcoes de identidade do dispositivo (antes em operaBLESecurity) ---
    // Gera PIN de 6 digitos a partir dos ultimos 3 bytes do MAC
    uint32_t generatePinFromMac();

    // Gera nome BLE unico no formato CHOPP_XXYY a partir dos ultimos 2 bytes do MAC
    String generateBleName();

    // Retorna o MAC do ESP32 formatado como string (ex: AA:BB:CC:DD:EE:FF)
    String getMacAddress();

#endif
