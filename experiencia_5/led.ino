#include <WiFi.h>
#include <WebServer.h>

// Configurações da Rede WiFi
const char* ssid = "SEU_SSID";
const char* password = "SUA_SENHA";

// Configuração do Hardware (ESP32-C3)

const int ledPin = 8; 
const int pwmChannel = 0;
const int pwmFreq = 5000; // Frequência fixa de 5kHz
const int pwmResolution = 8; // Resolução de 8 bits (0 a 255)

WebServer server(80);

// Insira o HTML aqui
const char index_html[] PROGMEM = R"rawliteral(
// COLE O CÓDIGO HTML AQUI
)rawliteral";

void setup() {
  Serial.begin(115200);

  // Configuração do PWM
  ledcSetup(pwmChannel, pwmFreq, pwmResolution);
  ledcAttachPin(ledPin, pwmChannel);
  ledcWrite(pwmChannel, 0);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nIP do ESP32: " + WiFi.localIP().toString());

  // Rotas do Servidor
  server.on("/", []() {
    server.send(200, "text/html", index_html);
  });

  server.on("/setLED", []() {
    if (server.hasArg("val")) {
      int duty = server.arg("val").toInt();
      ledcWrite(pwmChannel, duty);
      server.send(200, "text/plain", "OK");
    }
  });

  server.begin();
}

void loop() {
  server.handleClient(); // Escuta requisições HTTP (Não-bloqueante)
}
