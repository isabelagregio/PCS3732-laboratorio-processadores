#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

const char* ssid = "SEU_SSID";
const char* password = "SUA_SENHA";

const int servoPin = 9; 
Servo meuServo;

WebServer server(80);

// Insira o HTML aqui
const char index_html[] PROGMEM = R"rawliteral(
// COLE O CÓDIGO HTML AQUI
)rawliteral";

void setup() {
  Serial.begin(115200);
  
  meuServo.setPeriodHertz(50); // Frequência padrão para servos
  meuServo.attach(servoPin, 500, 2400); // Pinos e larguras de pulso min/max
  meuServo.write(90); // Posição inicial

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nIP do ESP32: " + WiFi.localIP().toString());

  server.on("/", []() {
    server.send(200, "text/html", index_html);
  });

  server.on("/setServo", []() {
    if (server.hasArg("val")) {
      int angle = server.arg("val").toInt();
      meuServo.write(angle);
      server.send(200, "text/plain", "OK");
    }
  });

  server.begin();
}

void loop() {
  server.handleClient();
}
