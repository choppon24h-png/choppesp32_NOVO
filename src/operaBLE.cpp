#include "operaBLE.h"

#ifdef USAR_ESP32_UART_BLE

// Ponteiros globais NimBLE
NimBLEServer          *pServer           = NULL;
NimBLECharacteristic  *pTxCharacteristic = NULL;
bool deviceConnected    = false;
bool deviceAuthenticated = false;

// ============================================================
// Callback de segurança BLE (NimBLE)
// Substitui BLESecurityCallbacks da biblioteca ESP32-BLE-Arduino.
// NimBLE unifica todos os eventos de segurança em
// NimBLESecurityCallbacks com assinaturas diferentes da lib antiga.
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

    // Chamado ao concluir o processo de autenticacao
    // NimBLE usa ble_gap_conn_desc* no lugar de esp_ble_auth_cmpl_t
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
// Substitui BLEServerCallbacks
// ============================================================
class MyServerCallbacks : public NimBLEServerCallbacks {

    void onConnect(NimBLEServer *pServer) override {
        digitalWrite(PINO_STATUS, LED_STATUS_ON);
        DBG_PRINT(F("\n[BLE] Conectado"));
        deviceConnected    = true;
        deviceAuthenticated = false; // Autenticacao sera confirmada no callback de seguranca
    }

    void onDisconnect(NimBLEServer *pServer) override {
        digitalWrite(PINO_STATUS, !LED_STATUS_ON);
        DBG_PRINT(F("\n[BLE] Desconectado"));
        deviceConnected    = false;
        deviceAuthenticated = false; // Resetar autenticacao ao desconectar
        delay(500);
        // NimBLE: reinicia advertising via NimBLEDevice (nao via pServer)
        NimBLEDevice::startAdvertising();
    }
};

// ============================================================
// Callback de escrita na characteristic RX
// Substitui BLECharacteristicCallbacks
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

            // Verificar autenticacao antes de executar qualquer comando
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
// setupBLE() — inicializa NimBLE, seguranca, servico e caracteristicas
// ============================================================
void setupBLE() {

    // Gerar nome BLE unico e PIN a partir do MAC
    String   bleName = generateBleName();
    uint32_t pin     = generatePinFromMac();

    // Inicializar NimBLE com o nome gerado por MAC
    NimBLEDevice::init(bleName.c_str());

    // --- Configuracao de seguranca NimBLE ---
    // Substitui: BLESecurity + setAuthenticationMode + setCapability +
    //            setInitEncryptionKey + setStaticPIN da biblioteca antiga.
    // true, true, true = bonding habilitado, MITM, Secure Connections
    NimBLEDevice::setSecurityAuth(true, true, true);

    // PIN estatico de 6 digitos derivado do MAC do dispositivo
    NimBLEDevice::setSecurityPasskey(pin);

    // Capacidade de I/O: apenas exibe o PIN (sem teclado no dispositivo)
    // Equivalente a ESP_IO_CAP_OUT da biblioteca antiga
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);

    // Registrar callback de seguranca (autenticacao, passkey, etc.)
    NimBLEDevice::setSecurityCallbacks(new MySecurity());

    // --- Criar servidor BLE ---
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    // --- Criar servico UART (Nordic UART Service — NUS) ---
    NimBLEService *pService = pServer->createService(SERVICE_UUID);

    // Characteristic TX: notificacao ESP32 -> App
    // NimBLE adiciona o CCCD (BLE2902) automaticamente em NOTIFY — nao precisa addDescriptor
    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        NIMBLE_PROPERTY::NOTIFY
    );

    // Characteristic RX: escrita App -> ESP32
    NimBLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        NIMBLE_PROPERTY::WRITE
    );
    pRxCharacteristic->setCallbacks(new MyCallbacks());

    // Iniciar servico
    pService->start();

    // Iniciar advertising com UUID do servico
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
// enviaBLE() — envia string via characteristic TX com notificacao
// Interface publica identica a versao anterior
// ============================================================
void enviaBLE(String msg) {
    if (deviceConnected && pTxCharacteristic != NULL) {

        msg += '\n';

        pTxCharacteristic->setValue(msg.c_str());

        pTxCharacteristic->notify();

    }

}

// ============================================================
// Controle de autenticacao — interface publica inalterada
// ============================================================
bool isDeviceAuthenticated() {

    return deviceAuthenticated;

}



void setDeviceAuthenticated(bool authenticated) {

    deviceAuthenticated = authenticated;

}

#endif  