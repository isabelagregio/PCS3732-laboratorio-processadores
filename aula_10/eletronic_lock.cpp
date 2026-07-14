/**********************************************************************
 * Electronic Lock - Raspberry Pi 3
 *
 * Componentes:
 *   - Teclado matricial 4x4
 *   - LCD 16x2 com PCF8574 via I2C
 *   - Buzzer
 *   - Relé para acionamento da fechadura
 *   - Sensor ultrassônico para detectar porta aberta/fechada
 *
 * Teclas:
 *   0-9 : inserir senha
 *   *   : apagar último dígito
 *   #   : confirmar senha
 *   C   : limpar senha
 *   A   : travar imediatamente
 *
 * Compilação sugerida:
 * g++ electronic_lock.cpp Keypad.cpp \
 *     -o electronic_lock \
 *     -lwiringPi -lwiringPiDev -lcrypto -lpthread
 **********************************************************************/

#include "Keypad.hpp"

#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <pcf8574.h>
#include <lcd.h>

#include <openssl/sha.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/time.h>

// ================================================================
// CONFIGURAÇÃO DOS PINOS - NUMERAÇÃO BCM
// ================================================================

// Teclado 4x4
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

byte rowPins[ROWS] = {16, 20, 21, 26};
byte colPins[COLS] = {19, 13, 6, 5};

Keypad keypad = Keypad(
    makeKeymap(keys),
    rowPins,
    colPins,
    ROWS,
    COLS
);

// Saídas
#define BUZZER_PIN 12
#define RELAY_PIN  18

// Sensor ultrassônico
#define TRIG_PIN 14
#define ECHO_PIN 15

// ================================================================
// CONFIGURAÇÃO DO LCD I2C
// ================================================================

int pcf8574Address = 0x27;

#define BASE 64
#define RS   (BASE + 0)
#define RW   (BASE + 1)
#define EN   (BASE + 2)
#define LED  (BASE + 3)
#define D4   (BASE + 4)
#define D5   (BASE + 5)
#define D6   (BASE + 6)
#define D7   (BASE + 7)

int lcdHandle = -1;

// ================================================================
// CONFIGURAÇÕES DO SISTEMA
// ================================================================

const int MIN_PASSWORD_LENGTH = 4;
const int MAX_PASSWORD_LENGTH = 6;

const int MAX_FAILED_ATTEMPTS = 3;

const unsigned long UNLOCK_TIME_MS = 5000;
const unsigned long COOLDOWN_TIME_MS = 30000;
const unsigned long SENSOR_INTERVAL_MS = 200;
const unsigned long LCD_REFRESH_MS = 200;

// Ajustar experimentalmente conforme a montagem.
// Distância menor ou igual a este valor significa porta fechada.
const float CLOSED_DISTANCE_CM = 8.0f;

// Quantidade de leituras consecutivas necessárias para aceitar mudança.
const int SENSOR_CONFIRMATION_COUNT = 3;

// HC-SR04: distância máxima considerada.
#define MAX_DISTANCE_CM 220
#define SONAR_TIMEOUT_US (MAX_DISTANCE_CM * 60)

// Alguns módulos de relé são ativos em nível baixo.
// Troque para false caso o seu relé seja ativo em HIGH.
const bool RELAY_ACTIVE_LOW = true;

// Hash SHA-256 da senha padrão "1234".
const char STORED_PASSWORD_HASH[] =
    "03ac674216f3e15c761ee1a5e255f067"
    "953623c8b388b4459e13f978d7c846f4";

// ================================================================
// ESTADOS DO SISTEMA
// ================================================================

enum SystemState {
    STATE_LOCKED,
    STATE_UNLOCKED,
    STATE_COOLDOWN,
    STATE_FORCED_OPEN_ALERT
};

SystemState currentState = STATE_LOCKED;

std::string enteredPassword;

int failedAttempts = 0;

bool doorClosed = true;
bool previousDoorClosed = true;

int consecutiveOpenReadings = 0;
int consecutiveClosedReadings = 0;

unsigned long unlockStartTime = 0;
unsigned long cooldownStartTime = 0;
unsigned long lastSensorRead = 0;
unsigned long lastLcdRefresh = 0;

// Controle não bloqueante do buzzer
bool buzzerActive = false;
unsigned long buzzerEndTime = 0;

// Controle de sequência de bipes
int remainingBeeps = 0;
unsigned long nextBeepTime = 0;
unsigned long beepDuration = 0;
unsigned long beepInterval = 0;

// Evita reescrever o LCD sem necessidade
std::string lastLcdLine1;
std::string lastLcdLine2;

// ================================================================
// FUNÇÕES AUXILIARES
// ================================================================

void setRelayLocked(bool locked)
{
    int lockedLevel;
    int unlockedLevel;

    if (RELAY_ACTIVE_LOW) {
        lockedLevel = HIGH;
        unlockedLevel = LOW;
    } else {
        lockedLevel = LOW;
        unlockedLevel = HIGH;
    }

    digitalWrite(RELAY_PIN, locked ? lockedLevel : unlockedLevel);
}

void clearLcdLine(int line)
{
    lcdPosition(lcdHandle, 0, line);
    lcdPuts(lcdHandle, "                ");
}

std::string fitLcdText(const std::string &text)
{
    if (text.length() >= 16) {
        return text.substr(0, 16);
    }

    return text + std::string(16 - text.length(), ' ');
}

void showLcd(const std::string &line1, const std::string &line2)
{
    if (lcdHandle < 0) {
        return;
    }

    std::string formattedLine1 = fitLcdText(line1);
    std::string formattedLine2 = fitLcdText(line2);

    if (formattedLine1 != lastLcdLine1) {
        lcdPosition(lcdHandle, 0, 0);
        lcdPuts(lcdHandle, formattedLine1.c_str());
        lastLcdLine1 = formattedLine1;
    }

    if (formattedLine2 != lastLcdLine2) {
        lcdPosition(lcdHandle, 0, 1);
        lcdPuts(lcdHandle, formattedLine2.c_str());
        lastLcdLine2 = formattedLine2;
    }
}

bool detectI2C(int address)
{
    int fileDescriptor = wiringPiI2CSetup(address);

    if (fileDescriptor < 0) {
        return false;
    }

    return wiringPiI2CWrite(fileDescriptor, 0) >= 0;
}

bool initializeLcd()
{
    if (detectI2C(0x27)) {
        pcf8574Address = 0x27;
    } else if (detectI2C(0x3F)) {
        pcf8574Address = 0x3F;
    } else {
        printf(
            "LCD nao encontrado nos enderecos 0x27 ou 0x3F.\n"
            "Execute: i2cdetect -y 1\n"
        );
        return false;
    }

    pcf8574Setup(BASE, pcf8574Address);

    for (int i = 0; i < 8; i++) {
        pinMode(BASE + i, OUTPUT);
    }

    digitalWrite(LED, HIGH);
    digitalWrite(RW, LOW);

    lcdHandle = lcdInit(
        2,
        16,
        4,
        RS,
        EN,
        D4,
        D5,
        D6,
        D7,
        0,
        0,
        0,
        0
    );

    if (lcdHandle == -1) {
        printf("Falha ao inicializar o LCD.\n");
        return false;
    }

    lcdClear(lcdHandle);
    return true;
}

// ================================================================
// SHA-256 DA SENHA
// ================================================================

std::string sha256(const std::string &input)
{
    unsigned char digest[SHA256_DIGEST_LENGTH];

    SHA256(
        reinterpret_cast<const unsigned char *>(input.c_str()),
        input.length(),
        digest
    );

    char output[SHA256_DIGEST_LENGTH * 2 + 1];

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        std::sprintf(output + i * 2, "%02x", digest[i]);
    }

    output[SHA256_DIGEST_LENGTH * 2] = '\0';

    return std::string(output);
}

bool passwordIsCorrect(const std::string &password)
{
    return sha256(password) == STORED_PASSWORD_HASH;
}

// ================================================================
// CONTROLE NÃO BLOQUEANTE DO BUZZER
// ================================================================

void stopBuzzer()
{
    digitalWrite(BUZZER_PIN, LOW);
    buzzerActive = false;
}

void startBeep(unsigned long durationMs)
{
    digitalWrite(BUZZER_PIN, HIGH);
    buzzerActive = true;
    buzzerEndTime = millis() + durationMs;
}

void scheduleBeeps(
    int count,
    unsigned long durationMs,
    unsigned long intervalMs
)
{
    remainingBeeps = count;
    beepDuration = durationMs;
    beepInterval = intervalMs;
    nextBeepTime = millis();
}

void updateBuzzer()
{
    unsigned long now = millis();

    if (buzzerActive && static_cast<long>(now - buzzerEndTime) >= 0) {
        stopBuzzer();

        if (remainingBeeps > 0) {
            nextBeepTime = now + beepInterval;
        }
    }

    if (!buzzerActive &&
        remainingBeeps > 0 &&
        static_cast<long>(now - nextBeepTime) >= 0) {

        remainingBeeps--;
        startBeep(beepDuration);
    }
}

// ================================================================
// SENSOR ULTRASSÔNICO
// ================================================================

int pulseIn(int pin, int level, int timeoutUs)
{
    struct timeval startTime;
    struct timeval currentTime;
    struct timeval pulseStart;

    gettimeofday(&startTime, nullptr);

    while (digitalRead(pin) != level) {
        gettimeofday(&currentTime, nullptr);

        long elapsed =
            (currentTime.tv_sec - startTime.tv_sec) * 1000000L +
            (currentTime.tv_usec - startTime.tv_usec);

        if (elapsed > timeoutUs) {
            return 0;
        }
    }

    gettimeofday(&pulseStart, nullptr);

    while (digitalRead(pin) == level) {
        gettimeofday(&currentTime, nullptr);

        long elapsed =
            (currentTime.tv_sec - pulseStart.tv_sec) * 1000000L +
            (currentTime.tv_usec - pulseStart.tv_usec);

        if (elapsed > timeoutUs) {
            return 0;
        }
    }

    return static_cast<int>(
        (currentTime.tv_sec - pulseStart.tv_sec) * 1000000L +
        (currentTime.tv_usec - pulseStart.tv_usec)
    );
}

float getDistanceCm()
{
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);

    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    int pulseTime = pulseIn(ECHO_PIN, HIGH, SONAR_TIMEOUT_US);

    if (pulseTime == 0) {
        return -1.0f;
    }

    return static_cast<float>(pulseTime) * 340.0f / 2.0f / 10000.0f;
}

void updateDoorSensor()
{
    unsigned long now = millis();

    if (now - lastSensorRead < SENSOR_INTERVAL_MS) {
        return;
    }

    lastSensorRead = now;

    float distance = getDistanceCm();

    if (distance < 0.0f) {
        printf("[SENSOR] Timeout ou leitura invalida.\n");
        return;
    }

    bool currentReadingClosed = distance <= CLOSED_DISTANCE_CM;

    if (currentReadingClosed) {
        consecutiveClosedReadings++;
        consecutiveOpenReadings = 0;

        if (consecutiveClosedReadings >= SENSOR_CONFIRMATION_COUNT) {
            doorClosed = true;
        }
    } else {
        consecutiveOpenReadings++;
        consecutiveClosedReadings = 0;

        if (consecutiveOpenReadings >= SENSOR_CONFIRMATION_COUNT) {
            doorClosed = false;
        }
    }

    printf(
        "[SENSOR] Distancia: %.2f cm | Porta: %s\n",
        distance,
        doorClosed ? "FECHADA" : "ABERTA"
    );
}

// ================================================================
// TRANSIÇÕES DE ESTADO
// ================================================================

void lockDoor()
{
    setRelayLocked(true);

    currentState = STATE_LOCKED;
    enteredPassword.clear();

    showLcd("PORTA TRANCADA", "Digite a senha");

    printf("[ESTADO] Fechadura trancada.\n");
}

void unlockDoor()
{
    setRelayLocked(false);

    currentState = STATE_UNLOCKED;
    unlockStartTime = millis();
    enteredPassword.clear();
    failedAttempts = 0;

    showLcd("ACESSO LIBERADO", "Porta aberta");

    scheduleBeeps(2, 100, 100);

    printf("[ESTADO] Acesso autorizado. Rele acionado.\n");
}

void startCooldown()
{
    currentState = STATE_COOLDOWN;
    cooldownStartTime = millis();
    enteredPassword.clear();

    setRelayLocked(true);

    showLcd("SISTEMA BLOQUEADO", "Aguarde 30 s");

    scheduleBeeps(3, 300, 150);

    printf("[SEGURANCA] Cooldown iniciado.\n");
}

void activateForcedOpenAlert()
{
    if (currentState == STATE_FORCED_OPEN_ALERT) {
        return;
    }

    currentState = STATE_FORCED_OPEN_ALERT;
    enteredPassword.clear();

    setRelayLocked(true);

    showLcd("ALERTA: PORTA", "ABERTA A FORCA");

    scheduleBeeps(6, 250, 150);

    printf("[ALERTA] Porta aberta enquanto deveria estar trancada.\n");
}

void processPassword()
{
    if (enteredPassword.length() < MIN_PASSWORD_LENGTH ||
        enteredPassword.length() > MAX_PASSWORD_LENGTH) {

        showLcd("SENHA INVALIDA", "Use 4 a 6 dig.");

        scheduleBeeps(1, 500, 0);

        enteredPassword.clear();
        return;
    }

    if (passwordIsCorrect(enteredPassword)) {
        unlockDoor();
        return;
    }

    failedAttempts++;
    enteredPassword.clear();

    printf(
        "[SEGURANCA] Senha incorreta. Tentativa %d de %d.\n",
        failedAttempts,
        MAX_FAILED_ATTEMPTS
    );

    if (failedAttempts >= MAX_FAILED_ATTEMPTS) {
        startCooldown();
        return;
    }

    char attemptsMessage[17];
    std::snprintf(
        attemptsMessage,
        sizeof(attemptsMessage),
        "Falhas: %d/%d",
        failedAttempts,
        MAX_FAILED_ATTEMPTS
    );

    showLcd("ACESSO NEGADO", attemptsMessage);
    scheduleBeeps(1, 700, 0);
}

// ================================================================
// TECLADO
// ================================================================

void updatePasswordDisplay()
{
    std::string maskedPassword(enteredPassword.length(), '*');

    showLcd("DIGITE A SENHA:", maskedPassword);
}

void handleKey(char key)
{
    printf("[TECLADO] Tecla pressionada: %c\n", key);

    if (currentState == STATE_COOLDOWN ||
        currentState == STATE_FORCED_OPEN_ALERT) {
        return;
    }

    if (key >= '0' && key <= '9') {
        if (enteredPassword.length() < MAX_PASSWORD_LENGTH) {
            enteredPassword.push_back(key);
            updatePasswordDisplay();
            scheduleBeeps(1, 40, 0);
        } else {
            showLcd("LIMITE ATINGIDO", "Max. 6 digitos");
            scheduleBeeps(1, 200, 0);
        }

        return;
    }

    switch (key) {
        case '*':
            if (!enteredPassword.empty()) {
                enteredPassword.pop_back();
            }

            updatePasswordDisplay();
            scheduleBeeps(1, 40, 0);
            break;

        case '#':
            processPassword();
            break;

        case 'C':
            enteredPassword.clear();
            showLcd("ENTRADA LIMPA", "Digite a senha");
            scheduleBeeps(1, 80, 0);
            break;

        case 'A':
            lockDoor();
            scheduleBeeps(1, 100, 0);
            break;

        default:
            break;
    }
}

void updateKeypad()
{
    char key = keypad.getKey();

    if (key != 0) {
        handleKey(key);
    }
}

// ================================================================
// ATUALIZAÇÃO DOS ESTADOS
// ================================================================

void updateLockedState()
{
    if (!doorClosed) {
        activateForcedOpenAlert();
    }
}

void updateUnlockedState()
{
    unsigned long now = millis();

    if (now - unlockStartTime >= UNLOCK_TIME_MS) {
        lockDoor();
    }
}

void updateCooldownState()
{
    unsigned long now = millis();

    unsigned long elapsed = now - cooldownStartTime;

    if (elapsed >= COOLDOWN_TIME_MS) {
        failedAttempts = 0;
        lockDoor();
        return;
    }

    if (now - lastLcdRefresh >= LCD_REFRESH_MS) {
        lastLcdRefresh = now;

        unsigned long remainingSeconds =
            (COOLDOWN_TIME_MS - elapsed + 999) / 1000;

        char line2[17];

        std::snprintf(
            line2,
            sizeof(line2),
            "Aguarde %lu s",
            remainingSeconds
        );

        showLcd("SISTEMA BLOQUEADO", line2);
    }
}

void updateForcedOpenAlertState()
{
    /*
     * O alerta somente é encerrado quando o sensor confirma que
     * a porta voltou ao estado fechado.
     */
    if (doorClosed) {
        scheduleBeeps(2, 100, 100);
        lockDoor();

        printf("[ALERTA] Porta fechada novamente. Alerta encerrado.\n");
        return;
    }

    /*
     * Repete o alerta sonoro periodicamente sem bloquear o programa.
     */
    static unsigned long lastAlertSequence = 0;
    unsigned long now = millis();

    if (now - lastAlertSequence >= 5000) {
        lastAlertSequence = now;
        scheduleBeeps(3, 250, 150);
    }
}

void updateSystemState()
{
    switch (currentState) {
        case STATE_LOCKED:
            updateLockedState();
            break;

        case STATE_UNLOCKED:
            updateUnlockedState();
            break;

        case STATE_COOLDOWN:
            updateCooldownState();
            break;

        case STATE_FORCED_OPEN_ALERT:
            updateForcedOpenAlertState();
            break;
    }
}

// ================================================================
// INICIALIZAÇÃO
// ================================================================

bool initializeHardware()
{
    if (wiringPiSetupGpio() == -1) {
        printf("Falha ao inicializar wiringPi.\n");
        return false;
    }

    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(RELAY_PIN, OUTPUT);

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(TRIG_PIN, LOW);

    setRelayLocked(true);

    keypad.setDebounceTime(50);

    if (!initializeLcd()) {
        return false;
    }

    return true;
}

// ================================================================
// PROGRAMA PRINCIPAL
// ================================================================

int main()
{
    printf("========================================\n");
    printf(" Fechadura Eletronica - Raspberry Pi 3\n");
    printf("========================================\n");

    if (!initializeHardware()) {
        return EXIT_FAILURE;
    }

    showLcd("FECHADURA", "Inicializando...");
    scheduleBeeps(1, 100, 0);

    delay(500);

    /*
     * Obtém algumas leituras iniciais antes de assumir o estado
     * físico da porta.
     */
    for (int i = 0; i < SENSOR_CONFIRMATION_COUNT; i++) {
        lastSensorRead = 0;
        updateDoorSensor();
        delay(100);
    }

    if (doorClosed) {
        lockDoor();
    } else {
        activateForcedOpenAlert();
    }

    while (true) {
        /*
         * Nenhuma função abaixo usa sleep longo.
         * Cada subsistema é atualizado em pequenos passos.
         */
        updateKeypad();
        updateDoorSensor();
        updateSystemState();
        updateBuzzer();

        previousDoorClosed = doorClosed;

        /*
         * Pequeno atraso apenas para reduzir uso excessivo da CPU.
         * Não interfere de forma relevante no tempo de resposta.
         */
        delay(5);
    }

    return EXIT_SUCCESS;
}
