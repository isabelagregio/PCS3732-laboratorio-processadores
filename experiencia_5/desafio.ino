#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

const char* ssid = "SEU_SSID";
const char* password = "SUA_SENHA";

// Pinos e Configurações
const int ledPin = 8; 
const int pwmChannel = 0;
const int pwmFreq = 5000;
const int pwmResolution = 8;

const int servoPin = 9; 
Servo meuServo;

WebServer server(80);

// Insira o HTML aqui
const char index_html[] PROGMEM = R"rawliteral(
// COLE O CÓDIGO HTML AQUI
)rawliteral";

void setup() {
  Serial.begin(115200);

  // Setup do LED
  ledcSetup(pwmChannel, pwmFreq, pwmResolution);
  ledcAttachPin(ledPin, pwmChannel);
  ledcWrite(pwmChannel, 0);

  // Setup do Servo
  meuServo.setPeriodHertz(50);
  meuServo.attach(servoPin, 500, 2400);
  meuServo.write(90);

  // Setup da Rede
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nIP do ESP32: " + WiFi.localIP().toString());

  // Definição das Rotas
  server.on("/", []() {
    server.send(200, "text/html", index_html);
  });

  server.on("/setLED", []() {
    if (server.hasArg("val")) {
      ledcWrite(pwmChannel, server.arg("val").toInt());
      server.send(200, "text/plain", "OK");
    }
  });

  server.on("/setServo", []() {
    if (server.hasArg("val")) {
      meuServo.write(server.arg("val").toInt());
      server.send(200, "text/plain", "OK");
    }
  });

  server.begin();
}

void loop() {
  // A execução principal fica livre para processar os pacotes de rede
  server.handleClient();
}
