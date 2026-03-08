#include "operaBLE.h"

#ifdef USAR_ESP32_UART_BLE

// Ponteiros globais NimBLE
NimBLEServer *pServer = NULL;
NimBLECharacteristic *pTxCharacteristic = NULL;

bool deviceConnected = false;
bool deviceAuthenticated = false;


// ============================================================
// FUNÇÕES AUXILIARES (ANTES FALTAVAM NO PROJETO)
// ============================================================

uint32_t generatePinFromMac() {

    uint64_t mac = ESP.getEfuseMac();

    uint32_t pin = (mac & 0xFFFFFF) % 1000000;

    if(pin < 100000) {
        pin += 100000;
    }

    return pin;
}

String generateBleName() {

    uint64_t mac = ESP.getEfuseMac();

    char name[20];

    sprintf(name, "CHOPP_%04X", (uint16_t)(mac & 0xFFFF));

    return String(name);
}


// ============================================================
// Callback de segurança BLE
// ============================================================

class MySecurity : public NimBLESecurityCallbacks {

    uint32_t onPassKeyRequest() override {

        uint32_t pin = generatePinFromMac();

        DBG_PRINT(F("\n[BLE_SEC] onPassKeyRequest -> PIN: "));
        DBG_PRINTF("%06lu\n", pin);

        return pin;
    }

    void onPassKeyNotify(uint32_t pass_key) override {

        DBG_PRINT(F("\n[BLE_SEC] onPassKeyNotify: "));
        DBG_PRINTF("%06lu\n", pass_key);

    }

    bool onSecurityRequest() override {

        DBG_PRINTLN(F("\n[BLE_SEC] onSecurityRequest"));

        return true;
    }

    bool onConfirmPIN(uint32_t pin) override {

        DBG_PRINT(F("\n[BLE_SEC] onConfirmPIN: "));
        DBG_PRINTF("%06lu\n", pin);

        return true;
    }

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
// Callback de conexao/desconexao BLE
// ============================================================

class MyServerCallbacks : public NimBLEServerCallbacks {

    void onConnect(NimBLEServer *pServer) override {

        digitalWrite(PINO_STATUS, LED_STATUS_ON);

        DBG_PRINT(F("\n[BLE] Conectado"));

        deviceConnected = true;

        deviceAuthenticated = false;
    }

    void onDisconnect(NimBLEServer *pServer) override {

        digitalWrite(PINO_STATUS, !LED_STATUS_ON);

        DBG_PRINT(F("\n[BLE] Desconectado"));

        deviceConnected = false;

        deviceAuthenticated = false;

        delay(500);

        NimBLEDevice::startAdvertising();
    }
};


// ============================================================
// Callback RX
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
// Setup BLE
// ============================================================

void setupBLE() {

    String bleName = generateBleName();

    uint32_t pin = generatePinFromMac();

    NimBLEDevice::init(bleName.c_str());

    NimBLEDevice::setSecurityAuth(true, true, true);

    NimBLEDevice::setSecurityPasskey(pin);

    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);

    NimBLEDevice::setSecurityCallbacks(new MySecurity());

    pServer = NimBLEDevice::createServer();

    pServer->setCallbacks(new MyServerCallbacks());

    NimBLEService *pService = pServer->createService(SERVICE_UUID);

    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        NIMBLE_PROPERTY::NOTIFY
    );

    NimBLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        NIMBLE_PROPERTY::WRITE
    );

    pRxCharacteristic->setCallbacks(new MyCallbacks());

    pService->start();

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
// Envio BLE
// ============================================================

void enviaBLE(String msg) {

    if (deviceConnected && pTxCharacteristic != NULL) {

        msg += '\n';

        pTxCharacteristic->setValue(msg.c_str());

        pTxCharacteristic->notify();
    }
}


// ============================================================
// Controle autenticacao
// ============================================================

bool isDeviceAuthenticated() {
    return deviceAuthenticated;
}

void setDeviceAuthenticated(bool authenticated) {
    deviceAuthenticated = authenticated;
}

#endif