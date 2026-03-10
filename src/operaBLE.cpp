#include "operaBLE.h"

#ifdef USAR_ESP32_UART_BLE

// ============================================================
// Variáveis globais NimBLE
// ============================================================

NimBLEServer *pServer = NULL;
NimBLECharacteristic *pTxCharacteristic = NULL;

bool deviceConnected = false;
bool deviceAuthenticated = false;

static const uint32_t BLE_FIXED_PIN = 259087;

// ============================================================
// Identidade do dispositivo
// ============================================================

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

        DBG_PRINT(F("[BLE_SEC] PassKey request: "));
        DBG_PRINTF("%06lu\n", BLE_FIXED_PIN);

        return BLE_FIXED_PIN;
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

            DBG_PRINTLN(F("[BLE] autenticado"));

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
        (void)pServer;
        // Usa a versao com desc para evitar duplicar logica
    }

    void onConnect(NimBLEServer *pServer, ble_gap_conn_desc *desc) override {

        digitalWrite(PINO_STATUS, LED_STATUS_ON);

        deviceConnected = true;
        deviceAuthenticated = false;

        DBG_PRINTLN(F("[BLE] conectado"));

        // Garante que o pareamento/criptografia seja iniciado
        if (desc != nullptr) {
            NimBLEDevice::startSecurity(desc->conn_handle);
        }
    }

    void onDisconnect(NimBLEServer *pServer) override {
        (void)pServer;
        // Usa a versao com desc para evitar duplicar logica
    }

    void onDisconnect(NimBLEServer *pServer, ble_gap_conn_desc *desc) override {
        (void)desc;

        digitalWrite(PINO_STATUS, !LED_STATUS_ON);

        deviceConnected = false;
        deviceAuthenticated = false;

        DBG_PRINTLN(F("[BLE] desconectado"));
        DBG_PRINTLN(F("[BLE] advertising iniciado"));
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

            DBG_PRINT(F("[BLE] comando recebido: "));
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
    NimBLEDevice::init(bleName.c_str());

    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityPasskey(BLE_FIXED_PIN);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);
    NimBLEDevice::setSecurityCallbacks(new MySecurity());

    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    NimBLEService *pService = pServer->createService(SERVICE_UUID);

    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        NIMBLE_PROPERTY::NOTIFY
    );
    pTxCharacteristic->addDescriptor(new NimBLE2902());

    NimBLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        NIMBLE_PROPERTY::WRITE
    );

    pRxCharacteristic->setCallbacks(new MyCallbacks());

    pService->start();

    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();

    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);
    pAdvertising->start();

    DBG_PRINTLN(F("[BLE] advertising iniciado"));
    DBG_PRINTLN(F("[BLE] aguardando conexao"));
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
