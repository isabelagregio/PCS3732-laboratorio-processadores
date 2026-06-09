#include <WiFi.h>
#include <WebServer.h>

// Configurações da Rede WiFi (Modo Access Point)
const char* ssid = "ESP32-CALC";
const char* senha = "12345678";

WebServer server(80);

// Configuração do Hardware (ESP32-C3)
const int ledPin = 8; 
const int pwmFreq = 5000;    // Frequência de 5kHz
const int pwmResolution = 8; // Resolução de 8 bits (0 a 255)

// Função para habilitar CORS (Permite requisições de outros computadores/sites)
void adicionarCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
}

// Responde à requisição de pré-verificação (Pre-flight) do navegador
void responderOptions() {
  adicionarCORS();
  server.send(204); // No Content
}

// Função que processa a alteração do LED
void ajustarLed() {
  adicionarCORS();

  if (server.hasArg("val")) {
    int duty = server.arg("val").toInt();
    
    // Proteção para garantir que o valor está no escopo do PWM (0-255)
    if (duty >= 0 && duty <= 255) {
      ledcWrite(ledPin, duty);
      server.send(200, "application/json", "{\"status\":\"OK\",\"brilho\":" + String(duty) + "}");
      return;
    }
  }
  
  server.send(400, "application/json", "{\"erro\":\"Parametro 'val' invalido ou ausente.\"}");
}

void setup() {
  Serial.begin(115200);

  // Configuração do LED usando a API moderna do ESP32 (Versão 3.0+)
  ledcAttach(ledPin, pwmFreq, pwmResolution);
  ledcWrite(ledPin, 0); // Inicia apagado

  // Configura o ESP32 como um Roteador (SoftAP)
  WiFi.softAP(ssid, senha);

  Serial.print("Dispositivo pronto! Conecte no WiFi 'ESP32-CALC' e envie comandos para o IP: ");
  Serial.println(WiFi.softAPIP());

  // Rotas da API com suporte a GET e OPTIONS (CORS)
  server.on("/setLED", HTTP_GET, ajustarLed);
  server.on("/setLED", HTTP_OPTIONS, responderOptions);

  server.begin();
}

void loop() {
  server.handleClient();
}
