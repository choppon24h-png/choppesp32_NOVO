#include "operaBLE.h"

#ifdef USAR_ESP32_UART_BLE

// ============================================================
// Funcoes de identidade do dispositivo
// Antes separadas em operaBLESecurity.cpp — agora consolidadas aqui.
// Nao dependem de nenhuma API BLE especifica, apenas de Arduino/ESP-IDF.
// ============================================================

/**
 * Gera um PIN de 6 digitos a partir do MAC do ESP32.
 * Utiliza os ultimos 3 bytes (24 bits) do MAC para garantir
 * que o mesmo hardware sempre gera o mesmo PIN.
 */
uint32_t generatePinFromMac() {
    // PIN fixo definido pelo usuario — nao depende do MAC.
    const uint32_t pin = 259087;
    DBG_PRINT(F("[BLE_SEC] PIN gerado: "));
    DBG_PRINTF("%06lu\n", pin);
    return pin;
}

/**
 * Gera um nome BLE unico baseado no MAC do ESP32.
 * Formato: CHOPP_XXYY onde XX e YY sao os ultimos 2 bytes do MAC em hex.
 * Exemplo: CHOPP_3A7F
 */
String generateBleName() {
    uint64_t macAddress = ESP.getEfuseMac();
    uint16_t last2Bytes = (uint16_t)(macAddress & 0xFFFF);
    String bleName = "CHOPP_";
    if ((last2Bytes >> 8) < 0x10) bleName += "0";
    bleName += String((last2Bytes >> 8), HEX);
    if ((last2Bytes & 0xFF) < 0x10) bleName += "0";
    bleName += String((last2Bytes & 0xFF), HEX);
    bleName.toUpperCase();
    DBG_PRINT(F("[BLE_SEC] Nome BLE gerado: "));
    DBG_PRINTLN(bleName);
    return bleName;
}

/**
 * Retorna o MAC do ESP32 como string formatada.
 * Exemplo: AA:BB:CC:DD:EE:FF
 */
String getMacAddress() {
    uint64_t macAddress = ESP.getEfuseMac();
    String macStr = "";
    for (int i = 5; i >= 0; i--) {
        uint8_t b = (uint8_t)((macAddress >> (i * 8)) & 0xFF);
        if (b < 0x10) macStr += "0";
        macStr += String(b, HEX);
        if (i > 0) macStr += ":";
    }
    macStr.toUpperCase();
    DBG_PRINT(F("[BLE_SEC] MAC Address: "));
    DBG_PRINTLN(macStr);
    return macStr;
}

// ============================================================
// Variaveis globais NimBLE
// ============================================================
NimBLEServer          *pServer           = NULL;
NimBLECharacteristic  *pTxCharacteristic = NULL;
bool deviceConnected    = false;
bool deviceAuthenticated = false;

// ============================================================
// Callback de seguranca BLE (NimBLE)
// Substitui BLESecurityCallbacks + setSecurityCallbacks da lib antiga.
// NimBLE unifica todos os eventos de seguranca em NimBLESecurityCallbacks.
// ============================================================
class MySecurity : public NimBLESecurityCallbacks {

    // Chamado quando o dispositivo remoto solicita o passkey exibido localmente
    uint32_t onPassKeyRequest() override {
        uint32_t pin = generatePinFromMac();
        DBG_PRINT(F("\n[BLE_SEC] onPassKeyRequest -> PIN: "));
        DBG_PRINTF("%06lu\n", pin);
        return pin;
    }

    // Chamado para confirmar o passkey (modo de confirmacao numerica)
    bool onConfirmPIN(uint32_t pin) override {
        DBG_PRINT(F("\n[BLE_SEC] onConfirmPIN: "));
        DBG_PRINTF("%06lu\n", pin);
        return true; // Aceita sempre — PIN validado pelo modo DISPLAY_ONLY
    }

    // Chamado ao concluir o processo de autenticacao.
    // NimBLE usa ble_gap_conn_desc* no lugar de esp_ble_auth_cmpl_t da lib antiga.
    void onAuthenticationComplete(ble_gap_conn_desc *desc) override {
        if (desc->sec_state.encrypted) {
            deviceAuthenticated = true;
            DBG_PRINTLN(F("\n[BLE_SEC] Autenticacao concluida com sucesso!"));
            enviaBLE("AUTH:OK");
        } else {
            deviceAuthenticated = false;
            DBG_PRINTLN(F("\n[BLE_SEC] Falha na autenticacao"));
        }
    }
};

// ============================================================
// Callbacks de conexao/desconexao do servidor BLE
// Substitui BLEServerCallbacks da biblioteca antiga.
// ============================================================
class MyServerCallbacks : public NimBLEServerCallbacks {

    void onConnect(NimBLEServer *pServer) override {
        digitalWrite(PINO_STATUS, LED_STATUS_ON);
        DBG_PRINT(F("\n[BLE] Conectado"));
        deviceConnected     = true;
        deviceAuthenticated = false; // Sera confirmado no callback de seguranca
    }

    void onDisconnect(NimBLEServer *pServer) override {
        digitalWrite(PINO_STATUS, !LED_STATUS_ON);
        DBG_PRINT(F("\n[BLE] Desconectado"));
        deviceConnected     = false;
        deviceAuthenticated = false;
        delay(500);
        // NimBLE: reinicia advertising via NimBLEDevice (nao via pServer->startAdvertising)
        NimBLEDevice::startAdvertising();
    }
};

// ============================================================
// Callback de escrita na characteristic RX (App -> ESP32)
// Substitui BLECharacteristicCallbacks da biblioteca antiga.
// ============================================================
class MyCallbacks : public NimBLECharacteristicCallbacks {

    void onWrite(NimBLECharacteristic *pCharacteristic) override {
        std::string rxValue = pCharacteristic->getValue();
        DBG_PRINT(F("\n[BLE] Recebido: "));

        if (rxValue.length() > 0) {
            String cmd = "";
            for (size_t i = 0; i < rxValue.length(); i++) {
                cmd += (char)rxValue[i];
            }
            DBG_PRINT(cmd);

            // Bloquear comandos de dispositivos nao autenticados
            if (!deviceAuthenticated) {
                DBG_PRINTLN(F("\n[BLE_SEC] Comando bloqueado - dispositivo nao autenticado"));
                enviaBLE("ERROR:NOT_AUTHENTICATED");
                return;
            }

            executaOperacao(cmd);
        }
    }
};

// ============================================================
// setupBLE()
// Inicializa o stack NimBLE, configura seguranca com PIN derivado
// do MAC, cria o servico UART (NUS) com characteristics RX e TX
// e inicia o advertising.
// ============================================================
void setupBLE() {

    String   bleName = generateBleName();
    uint32_t pin     = generatePinFromMac();

    // Inicializar NimBLE com o nome unico gerado pelo MAC
    NimBLEDevice::init(bleName.c_str());

    // Seguranca NimBLE — substitui o bloco BLESecurity da biblioteca antiga:
    //   pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND)
    //   pSecurity->setCapability(ESP_IO_CAP_OUT)
    //   pSecurity->setInitEncryptionKey(...)
    //   pSecurity->setStaticPIN(pin)
    NimBLEDevice::setSecurityAuth(true, true, true); // bonding, MITM, Secure Connections
    NimBLEDevice::setSecurityPasskey(pin);           // PIN estatico derivado do MAC
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY); // Apenas exibe PIN
    NimBLEDevice::setSecurityCallbacks(new MySecurity());

    // Servidor BLE
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Servico UART (Nordic UART Service — NUS)
    NimBLEService *pService = pServer->createService(SERVICE_UUID);

    // Characteristic TX: ESP32 -> App (notificacao)
    // NimBLE adiciona o CCCD (BLE2902) automaticamente — nao precisa addDescriptor
    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        NIMBLE_PROPERTY::NOTIFY
    );

    // Characteristic RX: App -> ESP32 (escrita)
    NimBLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        NIMBLE_PROPERTY::WRITE
    );
    pRxCharacteristic->setCallbacks(new MyCallbacks());

    pService->start();

    // Advertising com UUID do servico para que o app encontre o dispositivo
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->start();

    DBG_PRINT(F("\n[BLE] Aguardando conexao"));
    DBG_PRINT(F("\n[BLE] Nome: "));
    DBG_PRINTLN(bleName);
    DBG_PRINT(F("[BLE_SEC] PIN: "));
    DBG_PRINTF("%06lu\n", pin);
}

// ============================================================
// enviaBLE() — envia string via characteristic TX com notificacao.
// Interface publica identica a versao anterior.
// ============================================================
void enviaBLE(String msg) {
    if (deviceConnected && pTxCharacteristic != NULL) {
        msg += '\n';
        pTxCharacteristic->setValue(msg.c_str());
        pTxCharacteristic->notify();
    }
}

// ============================================================
// Controle de autenticacao — interface publica inalterada.
// ============================================================
bool isDeviceAuthenticated() {
    return deviceAuthenticated;
}

void setDeviceAuthenticated(bool authenticated) {
    deviceAuthenticated = authenticated;
}

#endif
