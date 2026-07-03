/**********************************************************************
 * Projeto: Metrônomo com LED, Servomotor e Buzzer
 * Plataforma: Raspberry Pi 3B+
 *
 * Funções:
 * - LED com PWM indicando a batida
 * - Servomotor alternando posição a cada ciclo
 * - Buzzer emitindo beep a cada ciclo
 * - Botão para aumentar BPM
 * - Botão para diminuir BPM
 * - Debouncing por tempo
 * - Temporização com clock monotônico para reduzir drift
 *
 * Compilar:
 * gcc metronome_integrado.c -o metronome_integrado -lwiringPi -lpthread
 *
 * Executar:
 * sudo ./metronome_integrado
 **********************************************************************/

#include <wiringPi.h>
#include <softPwm.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>

/* ==========================
   MAPEAMENTO DOS GPIOs - BCM
   ========================== */

/*
 * LED:
 * No material da Freenove, o LED azul costuma usar GPIO17.
 * Como o GPIO18 será usado para o servo, deixamos o LED no GPIO17.
 */
#define LED_PIN 17

/*
 * Servo:
 * GPIO18 é o pino clássico para PWM no Raspberry Pi.
 * Pino físico 12.
 */
#define SERVO_PIN 18

/*
 * Buzzer ativo:
 * Pode ser ligado ao GPIO23.
 * Pino físico 16.
 */
#define BUZZER_PIN 23

/*
 * Botões:
 * BTN_UP aumenta BPM.
 * BTN_DOWN diminui BPM.
 *
 * Ajuste estes pinos caso a sua placa de laboratório use outros botões.
 * O exemplo de botões do material usa GPIO26 para botão.
 */
#define BTN_UP_PIN 26
#define BTN_DOWN_PIN 19

/* ==========================
   PARÂMETROS DO SISTEMA
   ========================== */

#define BPM_MIN 30
#define BPM_MAX 180
#define BPM_STEP 5
#define BPM_INICIAL 60

/*
 * Debounce:
 * Eventos de botão dentro de 100 ms são ignorados.
 */
#define DEBOUNCE_MS 100

/*
 * LED:
 * softPwm com range 100 permite duty cycle de 0 a 100.
 */
#define LED_PWM_RANGE 100

/*
 * Servo:
 * Para 50 Hz:
 * período = 20 ms.
 *
 * No softPwm, usando range 200:
 * cada unidade equivale aproximadamente a 0,1 ms.
 *
 * 0,5 ms -> valor 5
 * 1,5 ms -> valor 15
 * 2,5 ms -> valor 25
 */
#define SERVO_PWM_RANGE 200
#define SERVO_MIN 5
#define SERVO_MAX 25

/*
 * Ângulos usados no metrônomo.
 * Evitamos 0° e 180° para não forçar o fim de curso.
 */
#define SERVO_LEFT_ANGLE 60
#define SERVO_RIGHT_ANGLE 120

/*
 * Duração do beep e do pulso luminoso.
 */
#define BEEP_MS 80
#define LED_PULSE_MS 120

/* ==========================
   VARIÁVEIS GLOBAIS
   ========================== */

volatile int running = 1;
volatile int bpm = BPM_INICIAL;

pthread_mutex_t bpm_mutex = PTHREAD_MUTEX_INITIALIZER;

volatile unsigned long last_up_interrupt = 0;
volatile unsigned long last_down_interrupt = 0;

/* ==========================
   FUNÇÕES AUXILIARES
   ========================== */

long map_value(long value, long fromLow, long fromHigh, long toLow, long toHigh) {
    return (toHigh - toLow) * (value - fromLow) / (fromHigh - fromLow) + toLow;
}

int angle_to_servo_pwm(int angle) {
    if (angle < 0) {
        angle = 0;
    }

    if (angle > 180) {
        angle = 180;
    }

    return (int) map_value(angle, 0, 180, SERVO_MIN, SERVO_MAX);
}

void servo_write_angle(int angle) {
    int pwm_value = angle_to_servo_pwm(angle);
    softPwmWrite(SERVO_PIN, pwm_value);
}

void buzzer_beep(int duration_ms) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(duration_ms);
    digitalWrite(BUZZER_PIN, LOW);
}

void led_pulse(int duration_ms) {
    /*
     * Pulso simples de brilho:
     * sobe para 100%, mantém por duration_ms e apaga.
     */
    softPwmWrite(LED_PIN, 100);
    delay(duration_ms);
    softPwmWrite(LED_PIN, 0);
}

int get_bpm(void) {
    int current_bpm;

    pthread_mutex_lock(&bpm_mutex);
    current_bpm = bpm;
    pthread_mutex_unlock(&bpm_mutex);

    return current_bpm;
}

void set_bpm(int new_bpm) {
    pthread_mutex_lock(&bpm_mutex);

    if (new_bpm < BPM_MIN) {
        new_bpm = BPM_MIN;
    }

    if (new_bpm > BPM_MAX) {
        new_bpm = BPM_MAX;
    }

    bpm = new_bpm;

    pthread_mutex_unlock(&bpm_mutex);
}

int bpm_to_period_ms(int current_bpm) {
    /*
     * BPM = batidas por minuto.
     * Período em ms = 60000 / BPM.
     *
     * Para 60 BPM:
     * período = 60000 / 60 = 1000 ms.
     */
    return 60000 / current_bpm;
}

long diff_ms(struct timespec start, struct timespec end) {
    long sec = end.tv_sec - start.tv_sec;
    long nsec = end.tv_nsec - start.tv_nsec;

    return sec * 1000 + nsec / 1000000;
}

/* ==========================
   INTERRUPÇÕES DOS BOTÕES
   ========================== */

void increase_bpm_isr(void) {
    unsigned long now = millis();

    if (now - last_up_interrupt < DEBOUNCE_MS) {
        return;
    }

    last_up_interrupt = now;

    int current_bpm = get_bpm();
    set_bpm(current_bpm + BPM_STEP);

    printf("[BOTAO +] BPM alterado para %d\n", get_bpm());
}

void decrease_bpm_isr(void) {
    unsigned long now = millis();

    if (now - last_down_interrupt < DEBOUNCE_MS) {
        return;
    }

    last_down_interrupt = now;

    int current_bpm = get_bpm();
    set_bpm(current_bpm - BPM_STEP);

    printf("[BOTAO -] BPM alterado para %d\n", get_bpm());
}

/* ==========================
   FINALIZAÇÃO SEGURA
   ========================== */

void cleanup(int sig) {
    running = 0;

    softPwmWrite(LED_PIN, 0);
    softPwmWrite(SERVO_PIN, 0);
    digitalWrite(BUZZER_PIN, LOW);

    pinMode(LED_PIN, INPUT);
    pinMode(SERVO_PIN, INPUT);
    pinMode(BUZZER_PIN, INPUT);

    printf("\nMetrônomo encerrado com segurança.\n");
    exit(0);
}

/* ==========================
   MAIN
   ========================== */

int main(void) {
    int beat_count = 0;
    int servo_angle = SERVO_LEFT_ANGLE;

    printf("=========================================\n");
    printf(" Metrônomo integrado: LED + Servo + Buzzer\n");
    printf(" BPM inicial: %d\n", BPM_INICIAL);
    printf(" BTN_UP   GPIO%d: aumenta BPM\n", BTN_UP_PIN);
    printf(" BTN_DOWN GPIO%d: diminui BPM\n", BTN_DOWN_PIN);
    printf("=========================================\n");

    signal(SIGINT, cleanup);

    if (wiringPiSetupGpio() == -1) {
        printf("Erro ao inicializar wiringPi.\n");
        return 1;
    }

    /* Configuração dos atuadores */
    softPwmCreate(LED_PIN, 0, LED_PWM_RANGE);
    softPwmCreate(SERVO_PIN, 0, SERVO_PWM_RANGE);

    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

    /* Configuração dos botões */
    pinMode(BTN_UP_PIN, INPUT);
    pinMode(BTN_DOWN_PIN, INPUT);

    /*
     * Pull-up interno:
     * botão solto = HIGH
     * botão pressionado = LOW
     *
     * Por isso usamos INT_EDGE_FALLING.
     */
    pullUpDnControl(BTN_UP_PIN, PUD_UP);
    pullUpDnControl(BTN_DOWN_PIN, PUD_UP);

    if (wiringPiISR(BTN_UP_PIN, INT_EDGE_FALLING, &increase_bpm_isr) < 0) {
        printf("Erro ao configurar interrupção do botão de aumentar BPM.\n");
        return 1;
    }

    if (wiringPiISR(BTN_DOWN_PIN, INT_EDGE_FALLING, &decrease_bpm_isr) < 0) {
        printf("Erro ao configurar interrupção do botão de diminuir BPM.\n");
        return 1;
    }

    /*
     * Posição inicial segura do servo.
     */
    servo_write_angle(90);
    delay(1000);

    while (running) {
        struct timespec cycle_start;
        struct timespec cycle_end;

        clock_gettime(CLOCK_MONOTONIC, &cycle_start);

        int current_bpm = get_bpm();
        int period_ms = bpm_to_period_ms(current_bpm);

        /*
         * Alterna o servo a cada batida.
         */
        if (servo_angle == SERVO_LEFT_ANGLE) {
            servo_angle = SERVO_RIGHT_ANGLE;
        } else {
            servo_angle = SERVO_LEFT_ANGLE;
        }

        servo_write_angle(servo_angle);

        /*
         * Aciona LED e buzzer na batida.
         * O LED indica visualmente a batida.
         * O buzzer indica sonoramente a batida.
         */
        softPwmWrite(LED_PIN, 100);
        digitalWrite(BUZZER_PIN, HIGH);

        delay(BEEP_MS);

        digitalWrite(BUZZER_PIN, LOW);

        /*
         * Mantém o LED aceso por um pouco mais, se LED_PULSE_MS > BEEP_MS.
         */
        if (LED_PULSE_MS > BEEP_MS) {
            delay(LED_PULSE_MS - BEEP_MS);
        }

        softPwmWrite(LED_PIN, 0);

        beat_count++;

        printf("Batida %d | BPM: %d | Periodo: %d ms | Servo: %d graus\n",
               beat_count,
               current_bpm,
               period_ms,
               servo_angle);

        /*
         * Correção de drift:
         * mede quanto tempo o ciclo já gastou e dorme apenas o restante.
         */
        clock_gettime(CLOCK_MONOTONIC, &cycle_end);

        long elapsed = diff_ms(cycle_start, cycle_end);
        long remaining = period_ms - elapsed;

        if (remaining > 0) {
            delay(remaining);
        } else {
            printf("[AVISO] Ciclo excedeu o período em %ld ms.\n", -remaining);
        }
    }

    cleanup(0);
    return 0;
}
