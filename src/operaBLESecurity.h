#include "config.h"
#if !defined(_OPERA_BLE_SECURITY_) && defined(USAR_ESP32_UART_BLE)
    #define _OPERA_BLE_SECURITY_

    #include <Arduino.h>
    #include <esp_efuse.h>

    /**
     * @brief Gera um PIN de 6 dígitos a partir do MAC do ESP32
     * Utiliza os últimos 3 bytes (24 bits) do MAC para garantir
     * que o mesmo MAC sempre gera o mesmo PIN
     * 
     * @return uint32_t PIN de 6 dígitos (000000 a 999999)
     */
    uint32_t generatePinFromMac();

    /**
     * @brief Gera um nome BLE único baseado no MAC do ESP32
     * Formato: CHOPP_XXYY onde XX e YY são os últimos 2 bytes do MAC em hexadecimal
     * 
     * @return String Nome BLE único (ex: CHOPP_3A7F)
     */
    String generateBleName();

    /**
     * @brief Obtém o MAC do ESP32 como string formatada
     * 
     * @return String MAC formatado (ex: AA:BB:CC:DD:EE:FF)
     */
    String getMacAddress();

#endif
