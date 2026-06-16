#include <WiFi.h>
#include <WebServer.h>

// Definição dos pinos conforme o segundo código
#define LDR_PIN 2
#define BUTTON_PIN 9
#define NEOPIXEL_PIN 8

// Variáveis de controle
int ldrValue = 0;

enum EstadoSistema { NORMAL, BAIXA_LUMINOSIDADE, EMERGENCIA_SOS };
EstadoSistema estadoAtual = NORMAL;

// Controle da leitura do LDR
unsigned long lastLDRReadTime = 0;
const unsigned long ldrInterval = 1000;

// Controle do piscar amarelo
unsigned long lastBlinkTime = 0;
bool yellowLedState = false;

// Controle da interrupção SOS e debounce
volatile bool sosRequested = false;
volatile unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// Servidor Web
WebServer server(80);

// Rotina de interrupção do botão SOS
void IRAM_ATTR handleSOSInterrupt() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    sosRequested = true;
    lastDebounceTime = millis();
  }
}

// Rota /data para enviar os dados em JSON
void handleData() {
  server.sendHeader("Access-Control-Allow-Origin", "*");

  String stateStr = (estadoAtual == NORMAL) ? "Luminosidade Normal" :
                    (estadoAtual == BAIXA_LUMINOSIDADE) ? "Baixa Luminosidade" :
                    "EMERGÊNCIA (SOS)";

  String json = "{\"ldr\":" + String(ldrValue) + ",\"state\":\"" + stateStr + "\"}";

  server.send(200, "application/json", json);
}

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LDR_PIN, INPUT);

  // Inicializa o NeoPixel em verde por 3 segundos
  neopixelWrite(NEOPIXEL_PIN, 0, 128, 0);
  delay(3000);
  neopixelWrite(NEOPIXEL_PIN, 0, 0, 0);

  // Cria a rede Wi-Fi do ESP32
  WiFi.softAP("ta_funcionando");

  // Configura rota do servidor
  server.on("/data", handleData);
  server.begin();

  // Configura interrupção do botão SOS
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleSOSInterrupt, FALLING);

  Serial.println("Sistema pronto. Acesse /data para ver os dados.");
}

void loop() {
  server.handleClient();

  unsigned long currentMillis = millis();

  // Verifica se houve solicitação de SOS por interrupção
  if (sosRequested) {
    sosRequested = false;

    estadoAtual = EMERGENCIA_SOS;

    // Cor vermelha no NeoPixel
    neopixelWrite(NEOPIXEL_PIN, 255, 0, 0);

    delay(3000);

    // Apaga o NeoPixel
    neopixelWrite(NEOPIXEL_PIN, 0, 0, 0);

    estadoAtual = NORMAL;
  }

  // Faz leitura do LDR a cada 1 segundo
  if (currentMillis - lastLDRReadTime >= ldrInterval) {
    lastLDRReadTime = currentMillis;

    ldrValue = analogRead(LDR_PIN);

    // Mesma lógica do segundo código:
    // valor acima de 2500 indica baixa luminosidade
    estadoAtual = (ldrValue > 2500) ? BAIXA_LUMINOSIDADE : NORMAL;
  }

  // Se estiver em baixa luminosidade, pisca amarelo a cada 2 segundos
  if (estadoAtual == BAIXA_LUMINOSIDADE) {
    if (currentMillis - lastBlinkTime >= 2000) {
      lastBlinkTime = currentMillis;

      yellowLedState = !yellowLedState;

      if (yellowLedState) {
        // Amarelo: vermelho + verde
        neopixelWrite(NEOPIXEL_PIN, 255, 255, 0);
      } else {
        neopixelWrite(NEOPIXEL_PIN, 0, 0, 0);
      }
    }
  } 
  else if (estadoAtual == NORMAL) {
    // Garante que o NeoPixel fique apagado no estado normal
    neopixelWrite(NEOPIXEL_PIN, 0, 0, 0);
  }
}
