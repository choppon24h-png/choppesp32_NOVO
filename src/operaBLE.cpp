#include "operaBLE.h"

#ifdef USAR_ESP32_UART_BLE

// ============================================================
// Variáveis globais NimBLE
// ============================================================

NimBLEServer *pServer = NULL;
NimBLECharacteristic *pTxCharacteristic = NULL;

bool deviceConnected = false;
bool deviceAuthenticated = false;


// ============================================================
// Identidade do dispositivo
// ============================================================

uint32_t generatePinFromMac() {

    uint64_t macAddress = ESP.getEfuseMac();
    uint32_t last3Bytes = (uint32_t)(macAddress & 0xFFFFFF);

    uint32_t pin = last3Bytes % 1000000;

    DBG_PRINT(F("[BLE_SEC] PIN gerado: "));
    DBG_PRINTF("%06lu\n", pin);

    return pin;
}

String generateBleName() {

    uint64_t macAddress = ESP.getEfuseMac();
    uint16_t last2Bytes = (uint16_t)(macAddress & 0xFFFF);

    String bleName = "CHOPP_";

    if ((last2Bytes >> 8) < 0x10) bleName += "0";
    bleName += String((last2Bytes >> 8), HEX);

    if ((last2Bytes & 0xFF) < 0x10) bleName += "0";
    bleName += String((last2Bytes & 0xFF), HEX);

    bleName.toUpperCase();

    return bleName;
}


// ============================================================
// Segurança BLE
// ============================================================

class MySecurity : public NimBLESecurityCallbacks {

    uint32_t onPassKeyRequest() override {

        uint32_t pin = generatePinFromMac();

        DBG_PRINT(F("[BLE_SEC] PassKey request: "));
        DBG_PRINTF("%06lu\n", pin);

        return pin;
    }

    void onPassKeyNotify(uint32_t pass_key) override {

        DBG_PRINT(F("[BLE_SEC] PassKey notify: "));
        DBG_PRINTF("%06lu\n", pass_key);
    }

    bool onSecurityRequest() override {

        DBG_PRINTLN(F("[BLE_SEC] Security request"));
        return true;
    }

    bool onConfirmPIN(uint32_t pin) override {

        DBG_PRINT(F("[BLE_SEC] Confirm PIN: "));
        DBG_PRINTF("%06lu\n", pin);

        return true;
    }

    void onAuthenticationComplete(ble_gap_conn_desc *desc) override {

        if (desc->sec_state.encrypted) {

            deviceAuthenticated = true;

            DBG_PRINTLN(F("[BLE_SEC] Autenticado"));

            enviaBLE("AUTH:OK");

        } else {

            deviceAuthenticated = false;

            DBG_PRINTLN(F("[BLE_SEC] Falha autenticacao"));
        }
    }
};


// ============================================================
// Conexão BLE
// ============================================================

class MyServerCallbacks : public NimBLEServerCallbacks {

    void onConnect(NimBLEServer *pServer) override {

        digitalWrite(PINO_STATUS, LED_STATUS_ON);

        deviceConnected = true;
        deviceAuthenticated = false;

        DBG_PRINTLN(F("[BLE] Conectado"));
    }

    void onDisconnect(NimBLEServer *pServer) override {

        digitalWrite(PINO_STATUS, !LED_STATUS_ON);

        deviceConnected = false;
        deviceAuthenticated = false;

        DBG_PRINTLN(F("[BLE] Desconectado"));

        delay(500);

        NimBLEDevice::startAdvertising();
    }
};


// ============================================================
// RX BLE
// ============================================================

class MyCallbacks : public NimBLECharacteristicCallbacks {

    void onWrite(NimBLECharacteristic *pCharacteristic) override {

        std::string rxValue = pCharacteristic->getValue();

        if (rxValue.length() > 0) {

            String cmd = "";

            for (int i = 0; i < rxValue.length(); i++) {

                cmd += (char)rxValue[i];
            }

            DBG_PRINT(F("[BLE] CMD: "));
            DBG_PRINTLN(cmd);

            if (!deviceAuthenticated) {

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

    DBG_PRINTLN(F("[BLE] Aguardando conexão"));
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
// Controle autenticação
// ============================================================

bool isDeviceAuthenticated() {

    return deviceAuthenticated;
}

void setDeviceAuthenticated(bool authenticated) {

    deviceAuthenticated = authenticated;
}

#endif