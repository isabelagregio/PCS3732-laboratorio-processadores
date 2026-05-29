#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "ESP32-CALC";
const char* senha = "12345678";

WebServer server(80);

const int pinosLeds[] = {6, 7, 8, 9};
const int totalLeds = 4;

void adicionarCORS() {

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
}

void mostrarResultado(int valor) {

  for (int i = 0; i < totalLeds; i++) {

    digitalWrite(pinosLeds[i], (valor >> i) & 1);
  }
}

void responderOptions() {

  adicionarCORS();

  server.send(204);
}

void calcular() {

  adicionarCORS();

  String opA = server.arg("a");
  String opB = server.arg("b");
  String operacao = server.arg("op");

  int valorA = strtol(opA.c_str(), NULL, 2);
  int valorB = strtol(opB.c_str(), NULL, 2);

  int resultado = 0;
  bool overflow = false;

  if (operacao == "add") {

    resultado = valorA + valorB;

    if (resultado > 8) {

      overflow = true;
      resultado &= 0x0F;
    }

  } else if (operacao == "sub") {

    resultado = valorA - valorB;

    if (resultado < -7) {

      overflow = true;
      resultado = 0;
    }
  }

  mostrarResultado(resultado);

  char buffer[5];

  sprintf(buffer, "%d%d%d%d",
          (resultado >> 3) & 1,
          (resultado >> 2) & 1,
          (resultado >> 1) & 1,
          resultado & 1);

  String resposta = "{";
  resposta += "\"operacao\":\"" + operacao + "\",";
  resposta += "\"resultado_bin\":\"" + String(buffer) + "\",";
  resposta += "\"overflow\":";
  resposta += (overflow ? "true" : "false");
  resposta += "}";

  server.send(200, "application/json", resposta);
}

void setup() {

  Serial.begin(115200);

  for (int i = 0; i < totalLeds; i++) {

    pinMode(pinosLeds[i], OUTPUT);
    digitalWrite(pinosLeds[i], LOW);
  }

  WiFi.softAP(ssid, senha);

  Serial.println(WiFi.softAPIP());

  server.on("/calc", HTTP_GET, calcular);

  server.on("/calc", HTTP_OPTIONS, responderOptions);

  server.begin();
}

void loop() {

  server.handleClient();
}
