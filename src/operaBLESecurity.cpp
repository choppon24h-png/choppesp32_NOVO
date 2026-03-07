#include "operaBLESecurity.h"

#ifdef USAR_ESP32_UART_BLE

uint32_t generatePinFromMac() {
    // Obter o MAC do ESP32
    uint64_t macAddress = ESP.getEfuseMac();
    
    // Extrair os últimos 3 bytes (24 bits) do MAC
    // O MAC tem 6 bytes, então pegamos os bytes 3, 4 e 5 (índices 16-23 bits)
    uint32_t last3Bytes = (uint32_t)(macAddress & 0xFFFFFF);
    
    // Converter para PIN de 6 dígitos
    // Usar modulo 1000000 para garantir máximo 6 dígitos
    uint32_t pin = last3Bytes % 1000000;
    
    // Garantir que o PIN tenha sempre 6 dígitos (adicionar leading zeros se necessário)
    // Isso é importante para a apresentação, mas o valor numérico é o que importa
    
    DBG_PRINT(F("[BLE_SEC] PIN gerado: "));
    DBG_PRINTF("%06lu\n", pin);
    
    return pin;
}

String generateBleName() {
    // Obter o MAC do ESP32
    uint64_t macAddress = ESP.getEfuseMac();
    
    // Extrair os últimos 2 bytes do MAC (16 bits)
    uint16_t last2Bytes = (uint16_t)(macAddress & 0xFFFF);
    
    // Converter para string hexadecimal em maiúsculas
    String bleName = "CHOPP_";
    
    // Adicionar os 2 bytes em formato hexadecimal
    if ((last2Bytes >> 8) < 0x10) {
        bleName += "0";
    }
    bleName += String((last2Bytes >> 8), HEX);
    
    if ((last2Bytes & 0xFF) < 0x10) {
        bleName += "0";
    }
    bleName += String((last2Bytes & 0xFF), HEX);
    
    // Converter para maiúsculas
    bleName.toUpperCase();
    
    DBG_PRINT(F("[BLE_SEC] Nome BLE gerado: "));
    DBG_PRINTLN(bleName);
    
    return bleName;
}

String getMacAddress() {
    // Obter o MAC do ESP32
    uint64_t macAddress = ESP.getEfuseMac();
    
    String macStr = "";
    
    // Converter cada byte do MAC para string formatada
    for (int i = 5; i >= 0; i--) {
        uint8_t byte = (uint8_t)((macAddress >> (i * 8)) & 0xFF);
        
        if (byte < 0x10) {
            macStr += "0";
        }
        macStr += String(byte, HEX);
        
        if (i > 0) {
            macStr += ":";
        }
    }
    
    macStr.toUpperCase();
    
    DBG_PRINT(F("[BLE_SEC] MAC Address: "));
    DBG_PRINTLN(macStr);
    
    return macStr;
}

#endif
