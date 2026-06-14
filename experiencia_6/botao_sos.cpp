#include <WiFi.h>
#include <WebServer.h>

// Definição dos Pinos
const int PIN_LDR = 34;
const int PIN_BUTTON_SOS = 12;   // Pino do Botão SOS com resistor pull-up
const int PIN_LED_YELLOW = 2;    // LED de telemetria normal (Pisca em baixa luz)
const int PIN_LED_RED = 4;       // LED de Emergência SOS

// Variáveis para Controle de Interrupção e Debounce
volatile bool sosRequestActive = false;
volatile unsigned long lastInterruptTime = 0;
const unsigned long DEBOUNCE_WINDOW = 50; // Janela de 50ms para ignorar ruído focado na física do botão

// Máquina de Estados do LED
enum EstadoSistema { NORMAL, BAIXA_LUZ, EMERGENCIA_SOS };
EstadoSistema estadoAtual = NORMAL;

unsigned long sosStartTime = 0;
unsigned long previousMillisBlink = 0;
bool ledYellowState = LOW;

// Rotina de Serviço de Interrupção (ISR) - Armazenada na RAM para máxima velocidade
void IRAM_ATTR srvBotaoSOS() {
    unsigned long interruptTime = millis();
    // Verifica se a variação rápida ocorreu fora da janela de debounce
    if (interruptTime - lastInterruptTime > DEBOUNCE_WINDOW) {
        sosRequestActive = true;
        lastInterruptTime = interruptTime;
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_LDR, INPUT);
    pinMode(PIN_LED_YELLOW, OUTPUT);
    pinMode(PIN_LED_RED, OUTPUT);
    
    // Configura o pino com INPUT_PULLUP interno para evitar flutuação elétrica
    pinMode(PIN_BUTTON_SOS, INPUT_PULLUP);
    
    // Vincula a interrupção de hardware ao pino do botão SOS
    attachInterrupt(digitalPinToInterrupt(PIN_BUTTON_SOS), srvBotaoSOS, FALLING);
    
    Serial.println("Sistema Pronto. Aguardando interrupções do Botão SOS...");
}

void loop() {
    unsigned long currentMillis = millis();
    int ldrValue = analogRead(PIN_LDR);

    // VERIFICAÇÃO DE PRIORIDADE MÁXIMA: Interrupção solicitou SOS?
    if (sosRequestActive) {
        // Força a troca de contexto lógica imediata salvando o estado atual
        sosRequestActive = false; 
        estadoAtual = EMERGENCIA_SOS;
        sosStartTime = currentMillis;
        
        Serial.println("[ALERTA INTERRUPÇÃO] Botão SOS Pressionado! Prioridade Máxima Ativada.");
        
        // Comportamento imediato das saídas de hardware
        digitalWrite(PIN_LED_YELLOW, LOW); // Interrompe o amarelo piscante instantaneamente
        digitalWrite(PIN_LED_RED, HIGH);   // Acende o vermelho fixo
    }

    // Gerenciador da Máquina de Estados
    switch (estadoAtual) {
        
        case EMERGENCIA_SOS:
            // Mantém o estado vermelho fixo por exatamente 3 segundos (3000ms)
            if (currentMillis - sosStartTime >= 3000) {
                digitalWrite(PIN_LED_RED, LOW);
                estadoAtual = NORMAL; // Retorna para o fluxo padrão de execução
                Serial.println("[SISTEMA] Emergência finalizada. Retomando loop principal.");
            }
            break;

        case NORMAL:
        case BAIXA_LUZ:
            // Se não estiver em emergência, executa a rotina padrão do LDR de fundo
            if (ldrValue < 1500) {
                estadoAtual = BAIXA_LUZ;
                // Executa o piscar de 2 segundos de forma assíncrona
                if (currentMillis - previousMillisBlink >= 2000) {
                    previousMillisBlink = currentMillis;
                    ledYellowState = !ledYellowState;
                    digitalWrite(PIN_LED_YELLOW, ledYellowState);
                }
            } else {
                estadoAtual = NORMAL;
                digitalWrite(PIN_LED_YELLOW, LOW);
            }
            break;
    }
}
