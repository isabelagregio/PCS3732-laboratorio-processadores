// Definição dos Pinos do Semáforo
const int PIN_S_GREEN  = 18;
const int PIN_S_YELLOW = 19;
const int PIN_S_RED    = 21;

// Pinos de Entrada (Periféricos)
const int PIN_LDR = 34;
const int PIN_BUT_PEDESTRIAN = 12;

// Variáveis de Controle Baseadas em Tempo e Eventos
volatile bool pedestrianRequest = false;
volatile unsigned long lastPedestrianTime = 0;
const unsigned long DEBOUNCE_TIME = 50;

enum EstadosSemaforo { DIA_VERDE, DIA_AMARELO, DIA_VERDE_HOLD, DIA_RED_PEDESTRIAN, MODO_NOTURNO_ALERTA };
EstadosSemaforo estadoSemaforo = DIA_VERDE;

unsigned long tempoMudancaEstado = 0;
unsigned long previousMillisNoturno = 0;
bool estadoYellowNoturno = LOW;

// ISR para solicitação do pedestre
void IRAM_ATTR srvPedestre() {
    unsigned long currentTime = millis();
    if (currentTime - lastPedestrianTime > DEBOUNCE_TIME) {
        pedestrianRequest = true;
        lastPedestrianTime = currentTime;
    }
}

void setup() {
    Serial.begin(115200);
    
    pinMode(PIN_S_GREEN, OUTPUT);
    pinMode(PIN_S_YELLOW, OUTPUT);
    pinMode(PIN_S_RED, OUTPUT);
    
    pinMode(PIN_BUT_PEDESTRIAN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(PIN_BUT_PEDESTRIAN), srvPedestre, FALLING);
    
    tempoMudancaEstado = millis();
}

void loop() {
    unsigned long currentMillis = millis();
    int ldrValue = analogRead(PIN_LDR);

    // MONITORAMENTO COMPARTILHADO: Se a luz baixar, o modo noturno assume controle absoluto
    if (ldrValue < 1500) {
        if (estadoSemaforo != MODO_NOTURNO_ALERTA) {
            estadoSemaforo = MODO_NOTURNO_ALERTA;
            digitalWrite(PIN_S_GREEN, LOW);
            digitalWrite(PIN_S_RED, LOW);
            Serial.println("[MODO] Baixa Luminosidade Detectada: Ativando Alerta Noturno (1Hz).");
        }
    } else if (estadoSemaforo == MODO_NOTURNO_ALERTA) {
        // Se a luz voltar ao normal, restabelece o fluxo diurno a partir do Verde
        estadoSemaforo = DIA_VERDE;
        tempoMudancaEstado = currentMillis;
        pedestrianRequest = false; // Limpa requisições feitas à noite
        digitalWrite(PIN_S_YELLOW, LOW);
    }

    // Execução da Lógica da Máquina de Estados do Semáforo
    switch (estadoSemaforo) {

        case MODO_NOTURNO_ALERTA:
            // Piscar o LED Amarelo a cada segundo (1Hz = ciclo completo a cada 1000ms / 500ms ligado)
            if (currentMillis - previousMillisNoturno >= 500) {
                previousMillisNoturno = currentMillis;
                estadoYellowNoturno = !estadoYellowNoturno;
                digitalWrite(PIN_S_YELLOW, estadoYellowNoturno);
            }
            break;

        case DIA_VERDE:
            digitalWrite(PIN_S_GREEN, HIGH);
            digitalWrite(PIN_S_YELLOW, LOW);
            digitalWrite(PIN_S_RED, LOW);

            // Se o pedestre apertar o botão, inicia imediatamente a transição de segurança
            if (pedestrianRequest) {
                pedestrianRequest = false;
                estadoSemaforo = DIA_AMARELO;
                tempoMudancaEstado = currentMillis;
                Serial.println("[TRAVESSIA] Pedestre solicitou travessia. Transicionando para Amarelo.");
            } 
            // Caso contrário, simula o tempo regular do fluxo de carros (Ex: 8 segundos verde)
            else if (currentMillis - tempoMudancaEstado >= 8000) {
                estadoSemaforo = DIA_AMARELO;
                tempoMudancaEstado = currentMillis;
            }
            break;

        case DIA_AMARELO:
            digitalWrite(PIN_S_GREEN, LOW);
            digitalWrite(PIN_S_YELLOW, HIGH);
            digitalWrite(PIN_S_RED, LOW);

            // Tempo de retenção de segurança no Amarelo (3 segundos)
            if (currentMillis - tempoMudancaEstado >= 3000) {
                estadoSemaforo = DIA_RED_PEDESTRIAN;
                tempoMudancaEstado = currentMillis;
                Serial.println("[TRAVESSIA] Sinal VERMELHO para carros. Pedestre liberado.");
            }
            break;

        case DIA_RED_PEDESTRIAN:
            digitalWrite(PIN_S_GREEN, LOW);
            digitalWrite(PIN_S_YELLOW, LOW);
            digitalWrite(PIN_S_RED, HIGH);

            // Tempo seguro para travessia do pedestre (5 segundos)
            if (currentMillis - tempoMudancaEstado >= 5000) {
                estadoSemaforo = DIA_VERDE;
                tempoMudancaEstado = currentMillis;
                Serial.println("[SISTEMA] Travessia concluída. Retornando fluxo normal.");
            }
            break;
    }
}
