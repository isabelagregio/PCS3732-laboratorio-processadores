#include <WiFi.h>
#include <WebServer.h>
#include <math.h>
#include <stdint.h>

const char* ssid = "ESP32-CALC";
const char* senha = "12345678";

WebServer server(80);

// Ajuste estes pinos conforme a placa ESP32 usada no laboratório.
// Evite pinos reservados para boot/flash na sua placa específica.
const int pinosLeds[] = {6, 7, 8, 9};
const int totalLeds = 4;

const int MAX_BITS_ENTRADA = 16;
const int TOTAL_MEDIDAS = 5;

struct ResultadoOperacao {
  uint64_t valor;
  bool overflow;
  String erro;
};

void adicionarCORS() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "*");
}

bool ehBinarioValido(const String& texto) {
  if (texto.length() == 0 || texto.length() > MAX_BITS_ENTRADA) {
    return false;
  }

  for (size_t i = 0; i < texto.length(); i++) {
    if (texto[i] != '0' && texto[i] != '1') {
      return false;
    }
  }

  return true;
}

uint64_t binarioParaInteiro(const String& texto) {
  uint64_t valor = 0;

  for (size_t i = 0; i < texto.length(); i++) {
    valor = (valor << 1) | (texto[i] == '1' ? 1 : 0);
  }

  return valor;
}

String inteiroParaBinario(uint64_t valor) {
  if (valor == 0) {
    return "0";
  }

  String binario = "";

  while (valor > 0) {
    binario = String((valor & 1) ? "1" : "0") + binario;
    valor >>= 1;
  }

  return binario;
}

void mostrarResultado(uint64_t valor) {
  // Os LEDs mostram apenas os 4 bits menos significativos do resultado.
  for (int i = 0; i < totalLeds; i++) {
    digitalWrite(pinosLeds[i], (valor >> i) & 1);
  }
}

// Implementação no ESP32 em C/C++.
// Multiplicação por somas sucessivas para evidenciar as múltiplas iterações.
// Critério de parada: contador i atinge o valor do operando b.
ResultadoOperacao multiplicarIterativo(uint64_t a, uint64_t b) {
  ResultadoOperacao r;
  r.valor = 0;
  r.overflow = false;
  r.erro = "";

  for (uint64_t i = 0; i < b; i++) {
    if (UINT64_MAX - r.valor < a) {
      r.overflow = true;
      r.erro = "Overflow na multiplicacao.";
      return r;
    }

    r.valor += a;
  }

  return r;
}

// Implementação no ESP32 em C/C++.
// Fatorial por multiplicações sucessivas.
// Critério de parada: contador i chega a 1.
ResultadoOperacao fatorialIterativo(uint64_t n) {
  ResultadoOperacao r;
  r.valor = 1;
  r.overflow = false;
  r.erro = "";

  // 20! é o maior fatorial que cabe em uint64_t.
  if (n > 20) {
    r.overflow = true;
    r.erro = "Overflow: para uint64_t, use fatorial de 0 ate 20.";
    return r;
  }

  for (uint64_t i = n; i > 1; i--) {
    r.valor *= i;
  }

  return r;
}

ResultadoOperacao executarOperacao(const String& operacao, uint64_t valorA, uint64_t valorB) {
  ResultadoOperacao r;
  r.valor = 0;
  r.overflow = false;
  r.erro = "";

  if (operacao == "add") {
    if (UINT64_MAX - valorA < valorB) {
      r.overflow = true;
      r.erro = "Overflow na soma.";
      return r;
    }

    r.valor = valorA + valorB;

  } else if (operacao == "sub") {
    if (valorB > valorA) {
      r.overflow = true;
      r.erro = "Resultado negativo nao representado nesta versao binaria sem sinal.";
      return r;
    }

    r.valor = valorA - valorB;

  } else if (operacao == "mul") {
    r = multiplicarIterativo(valorA, valorB);

  } else if (operacao == "fat") {
    r = fatorialIterativo(valorA);

  } else {
    r.erro = "Operacao invalida.";
  }

  return r;
}

double calcularMedia(unsigned long tempos[], int n) {
  double soma = 0.0;

  for (int i = 0; i < n; i++) {
    soma += tempos[i];
  }

  return soma / n;
}

double calcularDesvioPadrao(unsigned long tempos[], int n, double media) {
  double somaQuadrados = 0.0;

  for (int i = 0; i < n; i++) {
    double diferenca = tempos[i] - media;
    somaQuadrados += diferenca * diferenca;
  }

  return sqrt(somaQuadrados / n);
}

String montarArrayTemposJSON(unsigned long tempos[], int n) {
  String json = "[";

  for (int i = 0; i < n; i++) {
    json += String(tempos[i]);

    if (i < n - 1) {
      json += ",";
    }
  }

  json += "]";
  return json;
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

  if (!ehBinarioValido(opA)) {
    server.send(400, "application/json", "{\"erro\":\"Operando A invalido. Use de 1 a 16 bits binarios.\"}");
    return;
  }

  if (operacao != "fat" && !ehBinarioValido(opB)) {
    server.send(400, "application/json", "{\"erro\":\"Operando B invalido. Use de 1 a 16 bits binarios.\"}");
    return;
  }

  uint64_t valorA = binarioParaInteiro(opA);
  uint64_t valorB = (operacao == "fat") ? 0 : binarioParaInteiro(opB);

  ResultadoOperacao resultadoFinal;
  unsigned long tempos[TOTAL_MEDIDAS];

  for (int i = 0; i < TOTAL_MEDIDAS; i++) {
    unsigned long inicio = micros();
    resultadoFinal = executarOperacao(operacao, valorA, valorB);
    unsigned long fim = micros();

    tempos[i] = fim - inicio;
  }

  if (resultadoFinal.erro.length() > 0) {
    String respostaErro = "{";
    respostaErro += "\"operacao\":\"" + operacao + "\",";
    respostaErro += "\"erro\":\"" + resultadoFinal.erro + "\",";
    respostaErro += "\"overflow\":";
    respostaErro += (resultadoFinal.overflow ? "true" : "false");
    respostaErro += "}";

    server.send(200, "application/json", respostaErro);
    return;
  }

  double media = calcularMedia(tempos, TOTAL_MEDIDAS);
  double desvioPadrao = calcularDesvioPadrao(tempos, TOTAL_MEDIDAS, media);

  mostrarResultado(resultadoFinal.valor);

  String resposta = "{";
  resposta += "\"operacao\":\"" + operacao + "\",";
  resposta += "\"resultado_dec\":\"" + String(resultadoFinal.valor) + "\",";
  resposta += "\"resultado_bin\":\"" + inteiroParaBinario(resultadoFinal.valor) + "\",";
  resposta += "\"tempos_us\":" + montarArrayTemposJSON(tempos, TOTAL_MEDIDAS) + ",";
  resposta += "\"media_us\":\"" + String(media, 2) + "\",";
  resposta += "\"desvio_padrao_us\":\"" + String(desvioPadrao, 2) + "\",";
  resposta += "\"overflow\":";
  resposta += (resultadoFinal.overflow ? "true" : "false");
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

  Serial.print("IP do ESP32: ");
  Serial.println(WiFi.softAPIP());

  server.on("/calc", HTTP_GET, calcular);
  server.on("/calc", HTTP_OPTIONS, responderOptions);

  server.begin();
}

void loop() {
  server.handleClient();
}
