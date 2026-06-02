#include <WiFi.h>
#include <WebServer.h>
#include <math.h>
#include <stdint.h>

const char* ssid = "ESP32-CALC";
const char* senha = "12345678";

WebServer server(80);

// Ajuste estes pinos conforme a placa ESP32 usada no laboratório.
const int pinosLeds[] = {6, 7, 8, 9};
const int totalLeds = 4;

const int MAX_BITS_ENTRADA = 16;
const int TOTAL_MEDIDAS = 5;

// Alterado para int64_t para suportar valores negativos em complemento de dois
struct ResultadoOperacao {
  int64_t valor;
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

// Converte a string considerando o MSB como o bit de sinal (Extensão de Sinal)
int64_t binarioParaInteiro(const String& texto) {
  int64_t valor = 0;

  for (size_t i = 0; i < texto.length(); i++) {
    valor = (valor << 1) | (texto[i] == '1' ? 1 : 0);
  }

  // Se o primeiro bit (MSB) for '1', o número é negativo no complemento de dois.
  // Realiza a extensão de sinal para os bits superiores do int64_t.
  if (texto.length() > 0 && texto[0] == '1') {
    uint64_t mascara = ~((1ULL << texto.length()) - 1);
    valor |= mascara;
  }

  return valor;
}

// Converte o valor de volta para uma representação binária em complemento de dois
String inteiroParaBinario(int64_t valor) {
  uint64_t v = (uint64_t)valor;
  
  // Define o tamanho da palavra de exibição: 16 bits se couber, caso contrário 64 bits.
  int bits = (valor >= -32768 && valor <= 32767) ? 16 : 64;
  
  String binario = "";
  for (int i = bits - 1; i >= 0; i--) {
    binario += ((v >> i) & 1) ? "1" : "0";
  }

  return binario;
}

void mostrarResultado(int64_t valor) {
  // Os LEDs mostram os 4 bits menos significativos do resultado (independe de sinal).
  for (int i = 0; i < totalLeds; i++) {
    digitalWrite(pinosLeds[i], (valor >> i) & 1);
  }
}

// Multiplicação iterativa adaptada para números com sinal
ResultadoOperacao multiplicarIterativo(int64_t a, int64_t b) {
  ResultadoOperacao r;
  r.valor = 0;
  r.overflow = false;
  r.erro = "";

  bool resultadoNegativo = (a < 0) ^ (b < 0);
  int64_t absA = a < 0 ? -a : a;
  int64_t absB = b < 0 ? -b : b;

  if (a == INT64_MIN || b == INT64_MIN) {
    r.overflow = true;
    r.erro = "Overflow na multiplicacao (limite do tipo de dado).";
    return r;
  }

  for (int64_t i = 0; i < absB; i++) {
    if (INT64_MAX - r.valor < absA) {
      r.overflow = true;
      r.erro = "Overflow na multiplicacao.";
      return r;
    }
    r.valor += absA;
  }

  if (resultadoNegativo) {
    r.valor = -r.valor;
  }

  return r;
}

// Fatorial adaptado para validar números negativos
ResultadoOperacao fatorialIterativo(int64_t n) {
  ResultadoOperacao r;
  r.valor = 1;
  r.overflow = false;
  r.erro = "";

  if (n < 0) {
    r.erro = "Fatorial nao definido para numeros negativos.";
    return r;
  }

  if (n > 20) {
    r.overflow = true;
    r.erro = "Overflow: para int64_t, use fatorial de 0 ate 20.";
    return r;
  }

  for (int64_t i = n; i > 1; i--) {
    r.valor *= i;
  }

  return r;
}

ResultadoOperacao executarOperacao(const String& operacao, int64_t valorA, int64_t valorB) {
  ResultadoOperacao r;
  r.valor = 0;
  r.overflow = false;
  r.erro = "";

  if (operacao == "add") {
    int64_t soma = valorA + valorB;
    // Detecção de overflow com sinal: sinais iguais gerando resultado com sinal diferente
    if ((valorA > 0 && valorB > 0 && soma < 0) || (valorA < 0 && valorB < 0 && soma >= 0)) {
      r.overflow = true;
      r.erro = "Overflow na soma.";
      return r;
    }
    r.valor = soma;

  } else if (operacao == "sub") {
    int64_t sub = valorA - valorB;
    // Detecção de overflow na subtração com sinal
    if ((valorA > 0 && valorB < 0 && sub < 0) || (valorA < 0 && valorB > 0 && sub >= 0)) {
      r.overflow = true;
      r.erro = "Overflow na subtracao.";
      return r;
    }
    r.valor = sub; // Agora aceita resultados negativos nativamente!

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

  int64_t valorA = binarioParaInteiro(opA);
  int64_t valorB = (operacao == "fat") ? 0 : binarioParaInteiro(opB);

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
