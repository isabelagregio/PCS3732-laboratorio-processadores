#include <WiFi.h>
#include <WebServer.h>

// Configurações da Rede WiFi (Modo Access Point)
const char* ssid = "ESP32-CALC";
const char* senha = "12345678";

WebServer server(80);

// Configuração do Hardware (ESP32-C3)
const int pwmResolutionLed = 8; // Resolução de 8 bits (0 a 255)

const int pwmFreqServo = 50;     // Frequência de 50Hz para Servo Motor
const int pwmResolutionServo = 12; // Resolução de 12 bits (0 a 4095) para maior precisão

// Função para habilitar CORS
void adicionarCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
}

void responderOptions() {
  adicionarCORS();
  server.send(204);
}

// Função que processa a alteração do Servo (0 a 180 graus)
void ajustarServo() {
  adicionarCORS();
  if (server.hasArg("graus")) {
    int graus = server.arg("graus").toInt();
    if (graus >= 0 && graus <= 180) {
      
      // Mapeamento de graus (0-180) para o Duty Cycle de 12 bits (0-4095) em 50Hz
      // Geralmente: 0° = 0.5ms (duty ~102) e 180° = 2.5ms (duty ~512)
      int dutyServo = map(graus, 0, 180, 102, 512);
      
      ledcWrite(servoPin, dutyServo);
      server.send(200, "application/json", "{\"status\":\"OK\",\"angulo\":" + String(graus) + "}");
      return;
    }
  }
  server.send(400, "application/json", "{\"erro\":\"Parametro 'graus' invalido ou ausente.\"}");
}

void setup() {
  Serial.begin(115200);


  // Configuração do Servo (API Moderna ESP32)
  ledcAttach(servoPin, pwmFreqServo, pwmResolutionServo);
  // Inicia o servo no meio do caminho (90 graus -> duty ~307)
  ledcWrite(servoPin, map(90, 0, 180, 102, 512)); 

  // Configura o ESP32 como SoftAP
  WiFi.softAP(ssid, senha);

  Serial.print("Dispositivo pronto! IP: ");
  Serial.println(WiFi.softAPIP());

  
  server.on("/setServo", HTTP_GET, ajustarServo);
  server.on("/setServo", HTTP_OPTIONS, responderOptions);

  server.begin();
}

void loop() {
  server.handleClient();
}
