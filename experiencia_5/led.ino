#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "ESP32-CALC";
const char* senha = "12345678";

WebServer server(80);

const int ledPin = 8; 
int pwmFreq = 5000;    // Variável global para armazenar a frequência atual
const int pwmResolution = 8;

void adicionarCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
}

void responderOptions() {
  adicionarCORS();
  server.send(204);
}

void ajustarLed() {
  adicionarCORS();

  // Verifica se ambos os parâmetros existem
  if (server.hasArg("val") && server.hasArg("freq")) {
    int duty = server.arg("val").toInt();
    int novaFreq = server.arg("freq").toInt();
    
    // Atualiza a frequência se ela tiver mudado
    if (novaFreq != pwmFreq && novaFreq > 0) {
      pwmFreq = novaFreq;
      ledcChangeFrequency(ledPin, pwmFreq, pwmResolution);
    }

    if (duty >= 0 && duty <= 255) {
      ledcWrite(ledPin, duty);
      server.send(200, "application/json", "{\"status\":\"OK\",\"brilho\":" + String(duty) + ",\"freq\":" + String(pwmFreq) + "}");
      return;
    }
  }
  
  server.send(400, "application/json", "{\"erro\":\"Parametros invalidos\"}");
}

void setup() {
  Serial.begin(115200);

  ledcAttach(ledPin, pwmFreq, pwmResolution);
  ledcWrite(ledPin, 0);

  WiFi.softAP(ssid, senha);
  server.on("/setLED", HTTP_GET, ajustarLed);
  server.on("/setLED", HTTP_OPTIONS, responderOptions);
  server.begin();
}

void loop() {
  server.handleClient();
}
