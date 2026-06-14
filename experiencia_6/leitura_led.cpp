#include <WiFi.h>
#include <WebServer.h>

// Configurações da Rede Wi-Fi
const char* ssid = "ESP32-CALC";
const char* password = "12345678";

WebServer server(80);

// Definição de Pinos
const int PIN_LDR = 34;          // Pino Analógico conectado ao LDR (ADC1_CH6)
const int PIN_LED_YELLOW = 2;    // Representando o LED nativo/externo para piscar Amarelo

// Limiar de baixa luminosidade (ajustável conforme seu ambiente)
const int LIMIAR_NOTURNO = 1500; 

// Controle de tempo para o LED (Blink sem delay)
unsigned long previousMillisBlink = 0;
const long intervalBlink = 2000; // Pisca a cada 2 segundos (2000ms)
bool ledState = LOW;

void handleLDR() {
    int ldrValue = analogRead(PIN_LDR); // Leitura de 12 bits (0-4095)
    
    // Criação da string JSON
    String json = "{\"value\":" + String(ldrValue) + "}";
    
    // CRÍTICO: Envio de cabeçalhos CORS para permitir que o HTML local acesse a API
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", json);
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_LED_YELLOW, OUTPUT);
    
    // Configuração explícita da resolução do ADC para 12 bits
    analogSetAttenuation(ADC_11db); 
    
    // Conexão Wi-Fi
    WiFi.begin(ssid, password);
    Serial.print("Conectando ao Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConectado com sucesso!");
    Serial.print("Endereço IP do ESP32: ");
    Serial.println(WiFi.localIP());

    // Rotas da API
    server.on("/api/ldr", HTTP_GET, handleLDR);
    server.begin();
}

void loop() {
    server.handleClient(); // Processa requisições HTTP do Webserver

    int ldrValue = analogRead(PIN_LDR);
    unsigned long currentMillis = millis();

    // Se estiver em baixa luminosidade, pisca o LED Amarelo a cada 2s
    if (ldrValue < LIMIAR_NOTURNO) {
        if (currentMillis - previousMillisBlink >= intervalBlink) {
            previousMillisBlink = currentMillis;
            ledState = !ledState;
            digitalWrite(PIN_LED_YELLOW, ledState);
        }
    } else {
        // Apaga o LED se a luminosidade voltar ao normal
        digitalWrite(PIN_LED_YELLOW, LOW);
    }
}
