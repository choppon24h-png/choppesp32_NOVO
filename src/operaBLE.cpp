#include "operaBLE.h"

#ifdef USAR_ESP32_UART_BLE

NimBLEServer *pServer = NULL;
NimBLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool deviceAuthenticated = false;

// ================================
// Callbacks do servidor BLE
// ================================
class MyServerCallbacks : public NimBLEServerCallbacks {

    void onConnect(NimBLEServer *pServer) {

        digitalWrite(PINO_STATUS, LED_STATUS_ON);

        DBG_PRINT(F("\n[BLE] Conectado"));

        deviceConnected = true;

    }

    void onDisconnect(NimBLEServer *pServer) {

        digitalWrite(PINO_STATUS, !LED_STATUS_ON);

        DBG_PRINT(F("\n[BLE] Desconectado"));

        deviceConnected = false;

        deviceAuthenticated = false;

        delay(500);

        pServer->startAdvertising();

    }

};

// ================================
// Callbacks de escrita BLE
// ================================
class MyCallbacks : public NimBLECharacteristicCallbacks {

    void onWrite(NimBLECharacteristic *pCharacteristic) {

        String cmd = "";

        std::string rxValue = pCharacteristic->getValue();

        DBG_PRINT(F("\n[BLE] Recebido: "));

        if (rxValue.length() > 0) {

            for (int i = 0; i < rxValue.length(); i++) {

                cmd += (char)rxValue[i];

            }

            DBG_PRINT(cmd);

            if (!deviceAuthenticated) {

                DBG_PRINTLN(F("\n[BLE_SEC] Comando bloqueado - dispositivo não autenticado"));

                enviaBLE("ERROR:NOT_AUTHENTICATED");

                return;

            }

            executaOperacao(cmd);

        }

    }

};



// ================================
// Inicialização BLE
// ================================
void setupBLE() {

    String bleName = generateBleName();

    uint32_t pin = generatePinFromMac();

    NimBLEDevice::init(bleName.c_str());

    NimBLEDevice::setSecurityAuth(true, true, true);

    NimBLEDevice::setSecurityPasskey(pin);

    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);



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

    pAdvertising->start();



    DBG_PRINT(F("\n[BLE] Aguardando conexão"));

    DBG_PRINT(F("\n[BLE] Nome: "));

    DBG_PRINTLN(bleName);

    DBG_PRINT(F("[BLE_SEC] PIN: "));

    DBG_PRINTF("%06lu\n", pin);

}



// ================================
// Envio de mensagem BLE
// ================================
void enviaBLE(String msg) {

    if (deviceConnected && pTxCharacteristic != NULL) {

        msg += '\n';

        pTxCharacteristic->setValue(msg.c_str());

        pTxCharacteristic->notify();

    }

}



// ================================
// Controle de autenticação
// ================================
bool isDeviceAuthenticated() {

    return deviceAuthenticated;

}



void setDeviceAuthenticated(bool authenticated) {

    deviceAuthenticated = authenticated;

}

#endif  