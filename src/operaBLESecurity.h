#include "config.h"
#if !defined(_OPERA_BLE_SECURITY_) && defined(USAR_ESP32_UART_BLE)
    #define _OPERA_BLE_SECURITY_

    #include <Arduino.h>
    // Nota: esp_efuse.h removido — ESP.getEfuseMac() e suficiente e
    // nao requer include direto com NimBLE-Arduino no ESP32-C3.

    /**
     * @brief Gera um PIN de 6 digitos a partir do MAC do ESP32.
     * Utiliza os ultimos 3 bytes (24 bits) do MAC para garantir
     * que o mesmo MAC sempre gera o mesmo PIN.
     *
     * @return uint32_t PIN de 6 digitos (000000 a 999999)
     */
    uint32_t generatePinFromMac();

    /**
     * @brief Gera um nome BLE unico baseado no MAC do ESP32.
     * Formato: CHOPP_XXYY onde XX e YY sao os ultimos 2 bytes do MAC em hexadecimal.
     *
     * @return String Nome BLE unico (ex: CHOPP_3A7F)
     */
    String generateBleName();

    /**
     * @brief Obtem o MAC do ESP32 como string formatada.
     *
     * @return String MAC formatado (ex: AA:BB:CC:DD:EE:FF)
     */
    String getMacAddress();

#endif
