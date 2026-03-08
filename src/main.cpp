#include "config.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "operacional.h"
#include "operaPagina.h"
#include "operaBLE.h"
#include "operaRFID.h"

config_t configuracao = {0};

xQueueHandle listaLiberarML;

TaskHandle_t taskRFIDHandle = NULL;


void setup() {

    // Desativa Brownout (evita reset por queda de tensão)
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    // ============================
    // Configuração dos pinos
    // ============================
    pinMode(PINO_RELE, OUTPUT);
    digitalWrite(PINO_RELE, !RELE_ON);

    pinMode(PINO_STATUS, OUTPUT);
    digitalWrite(PINO_STATUS, !LED_STATUS_ON);

    pinMode(PINO_SENSOR_FLUSO, INPUT);

    // ============================
    // Inicializa Serial
    // ============================
    Serial.begin(115200);
    delay(2000);

    Serial.println("BOOT OK");

    Serial.println();
    Serial.println("====================================");
    Serial.println("   MAQUINA CHOPP - INICIALIZANDO");
    Serial.println("====================================");

    // ============================
    // Mostra MAC da placa
    // ============================
    uint64_t mac = ESP.getEfuseMac();

    Serial.printf("MAC: %02X:%02X:%02X:%02X:%02X:%02X\n",
                  (uint8_t)(mac >> 40),
                  (uint8_t)(mac >> 32),
                  (uint8_t)(mac >> 24),
                  (uint8_t)(mac >> 16),
                  (uint8_t)(mac >> 8),
                  (uint8_t)(mac));

    Serial.println();

#ifdef debug_debug
    Serial.println("[SETUP] Debug ativo");
#endif

    // ============================
    // Leitura da configuração
    // ============================
    Serial.println("[EEPROM] Lendo configuração...");
    leConfiguracao();

    // ============================
    // Inicializa BLE
    // ============================
#ifdef USAR_ESP32_UART_BLE
    Serial.println("[BLE] Inicializando BLE...");
    setupBLE();
#endif

    // ============================
    // Cria fila para liberar ML
    // ============================
    listaLiberarML = xQueueCreate(1, sizeof(uint32_t));

    if (listaLiberarML == NULL) {
        Serial.println("[ERRO] Falha ao criar fila ML");
    }

    // ============================
    // Task RFID
    // ============================
#ifdef USAR_RFID

    Serial.println("[RFID] Iniciando Task RFID...");

    xTaskCreate(
        taskRFID,
        "taskRFID",
        4096,
        NULL,
        3,
        &taskRFIDHandle
    );

#endif

    // ============================
    // Task controle ML
    // ============================
    Serial.println("[FLOW] Iniciando controle de fluxo ML...");

    xTaskCreate(
        taskLiberaML,
        "taskLiberaML",
        4096,
        NULL,
        3,
        NULL
    );

    Serial.println();
    Serial.println("====================================");
    Serial.println("        SISTEMA PRONTO");
    Serial.println("====================================");
}


void loop() {
    // Loop não é utilizado (FreeRTOS controla)
    vTaskDelete(NULL);
}


/*
Arduino Water flow meter
YF- S201 Hall Effect Water Flow Sensor
Water Flow Sensor output processed to read in litres/hour

volatile int flow_frequency; // Measures flow sensor pulses
unsigned int l_hour; // Calculated litres/hour
unsigned char flowsensor = 2; // Sensor Input
unsigned long currentTime;
unsigned long cloopTime;

void flow () // Interrupt function
{
   flow_frequency++;
}

void setup()
{
   pinMode(flowsensor, INPUT);
   digitalWrite(flowsensor, HIGH); // Optional Internal Pull-Up
   Serial.begin(9600);
   attachInterrupt(0, flow, RISING); // Setup Interrupt
   sei(); // Enable interrupts
   currentTime = millis();
   cloopTime = currentTime;
}

void loop ()
{
   currentTime = millis();

   // Every second, calculate and print litres/hour
   if(currentTime >= (cloopTime + 1000))
   {
      cloopTime = currentTime;

      // Pulse frequency (Hz) = 7.5Q, Q is flow rate in L/min.
      l_hour = (flow_frequency * 60 / 7.5);

      flow_frequency = 0;

      Serial.print(l_hour, DEC);
      Serial.println(" L/hour");
   }
}
*/