#include <Arduino.h>

// Pinos usados
#define LDR_PIN 2
#define BUTTON_PIN 9
#define NEOPIXEL_PIN 8

// Ajuste conforme a leitura do seu LDR
// Neste exemplo, valores acima de 2500 indicam baixa luminosidade
const int LDR_THRESHOLD = 2500;

// Tempos do semáforo
const unsigned long GREEN_TIME = 5000;        // 5 segundos no verde
const unsigned long YELLOW_TIME = 2000;       // 2 segundos no amarelo
const unsigned long RED_TIME = 5000;          // 5 segundos no vermelho normal
const unsigned long PEDESTRIAN_TIME = 5000;   // 5 segundos para travessia

// Modo noturno
const unsigned long NIGHT_BLINK_TIME = 1000;  // pisca amarelo a cada 1 segundo

// Leitura do LDR
const unsigned long LDR_READ_INTERVAL = 500;

// Debounce do botão
const unsigned long DEBOUNCE_DELAY = 50;

// Estados do semáforo
enum TrafficState {
  GREEN,
  YELLOW,
  RED,
  PEDESTRIAN_RED
};

TrafficState currentState = GREEN;

// Variáveis de tempo
unsigned long stateStartTime = 0;
unsigned long lastBlinkTime = 0;
unsigned long lastLDRReadTime = 0;
unsigned long lastButtonTime = 0;

// Variáveis de controle
int ldrValue = 0;
bool nightMode = false;
bool yellowBlinkState = false;
bool pedestrianRequest = false;

// Interrupção do botão
volatile bool buttonPressed = false;

void IRAM_ATTR handleButtonInterrupt() {
  buttonPressed = true;
}

void setColor(int red, int green, int blue) {
  neopixelWrite(NEOPIXEL_PIN, red, green, blue);
}

void enterState(TrafficState newState) {
  currentState = newState;
  stateStartTime = millis();

  switch (currentState) {
    case GREEN:
      setColor(0, 255, 0);
      Serial.println("Semáforo: VERDE");
      break;

    case YELLOW:
      setColor(255, 255, 0);
      Serial.println("Semáforo: AMARELO");
      break;

    case RED:
      setColor(255, 0, 0);
      Serial.println("Semáforo: VERMELHO");
      break;

    case PEDESTRIAN_RED:
      setColor(255, 0, 0);
      Serial.println("Travessia de pedestres: VERMELHO para veículos");
      break;
  }
}

void readLDR() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastLDRReadTime >= LDR_READ_INTERVAL) {
    lastLDRReadTime = currentMillis;

    ldrValue = analogRead(LDR_PIN);

    bool newNightMode = (ldrValue > LDR_THRESHOLD);

    if (newNightMode != nightMode && currentState != PEDESTRIAN_RED) {
      nightMode = newNightMode;

      if (nightMode) {
        Serial.println("Modo noturno ativado");
        yellowBlinkState = true;
        lastBlinkTime = currentMillis;
        setColor(255, 255, 0);
      } else {
        Serial.println("Modo normal ativado");
        enterState(GREEN);
      }
    } else {
      nightMode = newNightMode;
    }

    Serial.print("LDR: ");
    Serial.println(ldrValue);
  }
}

void checkButton() {
  if (buttonPressed) {
    noInterrupts();
    buttonPressed = false;
    interrupts();

    unsigned long currentMillis = millis();

    if (currentMillis - lastButtonTime >= DEBOUNCE_DELAY) {
      lastButtonTime = currentMillis;

      if (currentState != PEDESTRIAN_RED) {
        pedestrianRequest = true;
        Serial.println("Botão de pedestre pressionado");
      }
    }
  }
}

void runNightMode() {
  unsigned long currentMillis = millis();

  if (pedestrianRequest) {
    pedestrianRequest = false;
    enterState(PEDESTRIAN_RED);
    return;
  }

  if (currentMillis - lastBlinkTime >= NIGHT_BLINK_TIME) {
    lastBlinkTime = currentMillis;

    yellowBlinkState = !yellowBlinkState;

    if (yellowBlinkState) {
      setColor(255, 255, 0);
    } else {
      setColor(0, 0, 0);
    }
  }
}

void runPedestrianCrossing() {
  unsigned long currentMillis = millis();

  if (currentMillis - stateStartTime >= PEDESTRIAN_TIME) {
    if (nightMode) {
      Serial.println("Retornando ao modo noturno");
      currentState = GREEN;
      yellowBlinkState = true;
      lastBlinkTime = currentMillis;
      setColor(255, 255, 0);
    } else {
      enterState(GREEN);
    }
  }
}

void runNormalTrafficLight() {
  unsigned long currentMillis = millis();

  switch (currentState) {
    case GREEN:
      if (pedestrianRequest) {
        enterState(YELLOW);
      } else if (currentMillis - stateStartTime >= GREEN_TIME) {
        enterState(YELLOW);
      }
      break;

    case YELLOW:
      if (currentMillis - stateStartTime >= YELLOW_TIME) {
        if (pedestrianRequest) {
          pedestrianRequest = false;
          enterState(PEDESTRIAN_RED);
        } else {
          enterState(RED);
        }
      }
      break;

    case RED:
      if (pedestrianRequest) {
        pedestrianRequest = false;
        enterState(PEDESTRIAN_RED);
      } else if (currentMillis - stateStartTime >= RED_TIME) {
        enterState(GREEN);
      }
      break;

    case PEDESTRIAN_RED:
      runPedestrianCrossing();
      break;
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(LDR_PIN, INPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), handleButtonInterrupt, FALLING);

  // Inicializa o semáforo no verde
  enterState(GREEN);

  Serial.println("Sistema de semáforo iniciado");
}

void loop() {
  readLDR();
  checkButton();

  if (currentState == PEDESTRIAN_RED) {
    runPedestrianCrossing();
    return;
  }

  if (nightMode) {
    runNightMode();
  } else {
    runNormalTrafficLight();
  }
}
