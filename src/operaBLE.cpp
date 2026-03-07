#include "operaBLE.h"

#ifdef USAR_ESP32_UART_BLE

    // Ponteiros globais NimBLE
    NimBLEServer          *pServer           = NULL;
    NimBLECharacteristic  *pTxCharacteristic = NULL;
    bool deviceConnected    = false;
    bool deviceAuthenticated = false;

    // -------------------------------------------------------------------------
    // Callback de segurança BLE (NimBLE)
    // Substitui BLESecurityCallbacks da biblioteca antiga.
    // NimBLE unifica todos os eventos de segurança em NimBLESecurityCallbacks.
    // -------------------------------------------------------------------------
    class MySecurity : public NimBLESecurityCallbacks {

        // Chamado quando o dispositivo remoto solicita o passkey exibido localmente
        uint32_t onPassKeyRequest() override {
            uint32_t pin = generatePinFromMac();
            DBG_PRINT(F("\n[BLE_SEC] onPassKeyRequest -> PIN: "));
            DBG_PRINTF("%06lu\n", pin);
            return pin;
        }

        // Chamado para confirmar o passkey (usado em modo de confirmação numérica)
        bool onConfirmPIN(uint32_t pin) override {
            DBG_PRINT(F("\n[BLE_SEC] onConfirmPIN: "));
            DBG_PRINTF("%06lu\n", pin);
            return true; // Aceita sempre — PIN validado pelo modo DISPLAY_ONLY
        }

        // Chamado ao concluir o processo de autenticação
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

    // -------------------------------------------------------------------------
    // Callbacks de conexão/desconexão do servidor BLE
    // Substitui BLEServerCallbacks
    // -------------------------------------------------------------------------
    class MyServerCallbacks : public NimBLEServerCallbacks {

        void onConnect(NimBLEServer *pServer) override {
            digitalWrite(PINO_STATUS, LED_STATUS_ON);
            DBG_PRINT(F("\n[BLE] Conectado"));
            deviceConnected    = true;
            deviceAuthenticated = false; // Autenticação será confirmada no callback de segurança
        }

        void onDisconnect(NimBLEServer *pServer) override {
            digitalWrite(PINO_STATUS, !LED_STATUS_ON);
            DBG_PRINT(F("\n[BLE] Desconectado"));
            deviceConnected    = false;
            deviceAuthenticated = false; // Resetar autenticação ao desconectar
            delay(500);
            // NimBLE: reinicia advertising automaticamente via startAdvertising()
            NimBLEDevice::startAdvertising();
        }
    };

    // -------------------------------------------------------------------------
    // Callback de escrita na characteristic RX
    // Substitui BLECharacteristicCallbacks
    // -------------------------------------------------------------------------
    class MyCallbacks : public NimBLECharacteristicCallbacks {

        void onWrite(NimBLECharacteristic *pCharacteristic) override {
            // NimBLE retorna std::string diretamente via getValue()
            std::string rxValue = pCharacteristic->getValue();
            DBG_PRINT(F("\n[BLE] Recebido: "));

            if (rxValue.length() > 0) {
                String cmd = "";
                for (size_t i = 0; i < rxValue.length(); i++) {
                    cmd += (char)rxValue[i];
                }
                DBG_PRINT(cmd);

                // Verificar autenticação antes de executar qualquer comando
                if (!deviceAuthenticated) {
                    DBG_PRINTLN(F("\n[BLE_SEC] Comando bloqueado - dispositivo nao autenticado"));
                    enviaBLE("ERROR:NOT_AUTHENTICATED");
                    return;
                }

                executaOperacao(cmd);
            }
        }
    };

    // -------------------------------------------------------------------------
    // setupBLE() — inicializa o stack NimBLE, segurança, serviço e características
    // -------------------------------------------------------------------------
    void setupBLE() {

        // Gerar nome BLE único e PIN a partir do MAC
        String   bleName = generateBleName();
        uint32_t pin     = generatePinFromMac();

        // Inicializar NimBLE com o nome gerado
        NimBLEDevice::init(bleName.c_str());

        // --- Configuração de segurança NimBLE ---
        // Substitui: BLESecurity + setAuthenticationMode + setCapability + setStaticPIN
        // true, true, true = bonding, MITM, Secure Connections
        NimBLEDevice::setSecurityAuth(true, true, true);

        // PIN estático derivado do MAC (6 dígitos)
        NimBLEDevice::setSecurityPasskey(pin);

        // Capacidade de I/O: apenas exibe o PIN (sem teclado no dispositivo)
        // Equivalente a ESP_IO_CAP_OUT da biblioteca antiga
        NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY);

        // Registrar callback de segurança
        NimBLEDevice::setSecurityCallbacks(new MySecurity());

        // --- Criar servidor BLE ---
        pServer = NimBLEDevice::createServer();
        pServer->setCallbacks(new MyServerCallbacks());

        // --- Criar serviço UART (NUS) ---
        NimBLEService *pService = pServer->createService(SERVICE_UUID);

        // Characteristic TX (notificação ESP32 → App)
        // NimBLE: BLE2902 (CCCD) é adicionado automaticamente em características com NOTIFY
        pTxCharacteristic = pService->createCharacteristic(
            CHARACTERISTIC_UUID_TX,
            NIMBLE_PROPERTY::NOTIFY
        );

        // Characteristic RX (escrita App → ESP32)
        NimBLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
            CHARACTERISTIC_UUID_RX,
            NIMBLE_PROPERTY::WRITE
        );
        pRxCharacteristic->setCallbacks(new MyCallbacks());

        // Iniciar serviço
        pService->start();

        // Iniciar advertising
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

    // -------------------------------------------------------------------------
    // enviaBLE() — envia string via characteristic TX com notificação
    // Lógica idêntica à versão anterior; apenas tipos atualizados para NimBLE
    // -------------------------------------------------------------------------
    void enviaBLE(String msg) {
        if (deviceConnected && pTxCharacteristic != NULL) {
            msg += '\n';
            pTxCharacteristic->setValue(msg.c_str());
            pTxCharacteristic->notify();
        }
    }

    // -------------------------------------------------------------------------
    // Funções de controle de autenticação — interface pública inalterada
    // -------------------------------------------------------------------------
    bool isDeviceAuthenticated() {
        return deviceAuthenticated;
    }

    void setDeviceAuthenticated(bool authenticated) {
        deviceAuthenticated = authenticated;
    }

#endif
