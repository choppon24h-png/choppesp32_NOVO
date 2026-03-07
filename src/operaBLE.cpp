#include "operaBLE.h"

#ifdef USAR_ESP32_UART_BLE

    BLEServer *pServer = NULL;
    BLECharacteristic *pTxCharacteristic;
    bool deviceConnected = false;
    bool deviceAuthenticated = false;

    // Callback para eventos de segurança BLE
    class MySecurity : public BLESecurityCallbacks {
        
        uint32_t onPassKeyRequest() {
            DBG_PRINTLN(F("\n[BLE_SEC] onPassKeyRequest"));
            uint32_t pin = generatePinFromMac();
            return pin;
        }

        void onPassKeyNotify(uint32_t passkey) {
            DBG_PRINT(F("\n[BLE_SEC] onPassKeyNotify: "));
            DBG_PRINTF("%06lu\n", passkey);
        }

        bool onSecurityRequest() {
            DBG_PRINTLN(F("\n[BLE_SEC] onSecurityRequest"));
            return true;
        }

        void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) {
            DBG_PRINT(F("\n[BLE_SEC] onAuthenticationComplete - success: "));
            DBG_PRINTLN(cmpl.success);
            
            if (cmpl.success) {
                deviceAuthenticated = true;
                DBG_PRINTLN(F("[BLE_SEC] Dispositivo autenticado com sucesso!"));
                // Enviar confirmação de autenticação
                #ifdef USAR_ESP32_UART_BLE
                    enviaBLE("AUTH:OK");
                #endif
            } else {
                deviceAuthenticated = false;
                DBG_PRINTLN(F("[BLE_SEC] Falha na autenticação"));
            }
        }

        bool onConfirmPIN(uint32_t pin) {
            DBG_PRINT(F("\n[BLE_SEC] onConfirmPIN: "));
            DBG_PRINTF("%06lu\n", pin);
            return true;
        }
    };

    class MyServerCallbacks : public BLEServerCallbacks {
        void onConnect(BLEServer *pServer) {
            digitalWrite( PINO_STATUS, LED_STATUS_ON);
            DBG_PRINT(F("\n[BLE] Conectado"));
            deviceConnected = true;
            // Nota: deviceAuthenticated será definido no callback de autenticação
        };

        void onDisconnect(BLEServer *pServer) {
            digitalWrite( PINO_STATUS, !LED_STATUS_ON);
            DBG_PRINT(F("\n[BLE] Desconectado"));            
            deviceConnected = false;
            deviceAuthenticated = false;  // Resetar autenticação ao desconectar
            delay(500);
            pServer->startAdvertising();
            
        }
    };

    class MyCallbacks : public BLECharacteristicCallbacks {
        void onWrite(BLECharacteristic *pCharacteristic) {        
            String cmd = "";
            std::string rxValue = pCharacteristic->getValue();
            DBG_PRINT(F("\n[BLE] Recebido: "));
            
            if (rxValue.length() > 0) {
                for (int i = 0; i < rxValue.length(); i++) {                    
                    cmd += (char)rxValue[i];
                }
                DBG_PRINT(cmd);
                
                // Verificar autenticação antes de executar comando
                if (!deviceAuthenticated) {
                    DBG_PRINTLN(F("\n[BLE_SEC] Comando bloqueado - dispositivo não autenticado"));
                    enviaBLE("ERROR:NOT_AUTHENTICATED");
                    return;
                }
                
                executaOperacao(cmd);
            }
        }
    };

void setupBLE() {  

    // Gerar nome BLE único baseado no MAC
    String bleName = generateBleName();
    
    // Gerar PIN a partir do MAC
    uint32_t pin = generatePinFromMac();

    // Create the BLE Device
    BLEDevice::init(bleName.c_str());

    // Configurar segurança BLE
    BLESecurity *pSecurity = new BLESecurity();
    
    // Definir callbacks de segurança
    pSecurity->setSecurityCallbacks(new MySecurity());
    
    // Configurar modo de autenticação com PIN obrigatório
    // ESP_LE_AUTH_REQ_SC_MITM_BOND: Secure Connections com MITM protection e bonding
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
    
    // Capacidade de I/O: ESP_IO_CAP_OUT (apenas saída - mostra PIN)
    pSecurity->setCapability(ESP_IO_CAP_OUT);
    
    // Inicializar chaves de criptografia
    pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    
    // Definir PIN estático gerado a partir do MAC
    pSecurity->setStaticPIN(pin);

    // Create the BLE Server
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // Create the BLE Service
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Create a BLE Characteristic
    pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
    pTxCharacteristic->addDescriptor(new BLE2902());
    BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
    pRxCharacteristic->setCallbacks(new MyCallbacks());

    // Start the service
    pService->start();

    // Start advertising
    pServer->getAdvertising()->start();
    DBG_PRINT(F("\n[BLE] Aguardando conexão"));
    DBG_PRINT(F("\n[BLE] Nome: "));
    DBG_PRINTLN(bleName);
    DBG_PRINT(F("[BLE_SEC] PIN: "));
    DBG_PRINTF("%06lu\n", pin);
}

void enviaBLE( String msg ) {
    if (deviceConnected && pTxCharacteristic != NULL) {
        msg += '\n';
        pTxCharacteristic->setValue(msg.c_str());
        pTxCharacteristic->notify();
    }
}

bool isDeviceAuthenticated() {
    return deviceAuthenticated;
}

void setDeviceAuthenticated(bool authenticated) {
    deviceAuthenticated = authenticated;
}

#endif
