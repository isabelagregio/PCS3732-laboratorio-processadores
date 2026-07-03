/**********************************************************************
 * Projeto: Metrônomo com LED, Servomotor e Buzzer
 * Plataforma: Raspberry Pi 3B+
 *
 * Versão sem controle de BPM por botões.
 *
 * Requisitos atendidos:
 * - Ciclo nominal de 1000 ms, equivalente a 60 BPM
 * - Correção de drift com clock monotônico e temporização absoluta
 * - Servo com PWM de 50 Hz e pulso entre 1 ms e 2 ms
 * - LED com rampa luminosa usando PWM
 * - Buzzer com beep curto a cada batida
 *
 * Ligações sugeridas:
 * - LED:
 *      GPIO17, pino físico 11 -> resistor 220 ohms -> LED -> GND
 *
 * - Servo:
 *      Sinal, fio laranja  -> GPIO18, pino físico 12
 *      VCC, fio vermelho  -> 5V, pino físico 2 ou 4
 *      GND, fio marrom    -> GND, pino físico 6
 *
 * - Buzzer ativo:
 *      Terminal + -> GPIO23, pino físico 16
 *      Terminal - -> GND, pino físico 6 ou 14
 *
 * Compilar:
 * gcc metronome_integrado_final.c -o metronome_integrado_final -lwiringPi -lpthread
 *
 * Executar:
 * sudo ./metronome_integrado_final
 **********************************************************************/

#include <wiringPi.h>
#include <softPwm.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

/* ==========================
   MAPEAMENTO DOS GPIOs - BCM
   ========================== */

#define LED_PIN     17   // GPIO17, pino físico 11
#define SERVO_PIN   18   // GPIO18, pino físico 12
#define BUZZER_PIN  23   // GPIO23, pino físico 16

/* ==========================
   PARÂMETROS DO METRÔNOMO
   ========================== */

#define BPM 60
#define PERIOD_MS 1000   // 60 BPM -> 1 batida por segundo

/* ==========================
   PARÂMETROS DO LED
   ========================== */

#define LED_PWM_RANGE 100

/*
 * Rampa luminosa:
 * 0 -> 100 -> 0.
 *
 * Com passo 5 e delay 5 ms:
 * subida: 21 passos * 5 ms = ~105 ms
 * descida: 21 passos * 5 ms = ~105 ms
 * total: ~210 ms
 *
 * Esse tempo fica dentro do ciclo de 1000 ms.
 */
#define LED_RAMP_STEP       5
#define LED_RAMP_DELAY_MS   5

/* ==========================
   PARÂMETROS DO SERVO
   ========================== */

/*
 * Servo com PWM de 50 Hz:
 * período = 20 ms.
 *
 * No softPwm:
 * range = 200
 * unidade aproximada = 0,1 ms
 *
 * Resultado esperado do guia:
 * movimento mecânico suave com pulso entre 1 ms e 2 ms.
 *
 * 1 ms -> valor 10
 * 2 ms -> valor 20
 */
#define SERVO_PWM_RANGE 200
#define SERVO_MIN       10   // 1,0 ms
#define SERVO_MAX       20   // 2,0 ms

/*
 * Ângulos escolhidos para o metrônomo.
 * Evitam extremos mecânicos e usam a região central do servo.
 */
#define SERVO_LEFT_ANGLE   45
#define SERVO_RIGHT_ANGLE  135

/* ==========================
   PARÂMETROS DO BUZZER
   ========================== */

#define BEEP_MS 80

/* ==========================
   VARIÁVEIS GLOBAIS
   ========================== */

volatile sig_atomic_t running = 1;

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

void led_ramp(void) {
    int duty;

    for (duty = 0; duty <= 100; duty += LED_RAMP_STEP) {
        softPwmWrite(LED_PIN, duty);
        delay(LED_RAMP_DELAY_MS);
    }

    for (duty = 100; duty >= 0; duty -= LED_RAMP_STEP) {
        softPwmWrite(LED_PIN, duty);
        delay(LED_RAMP_DELAY_MS);
    }

    softPwmWrite(LED_PIN, 0);
}

void buzzer_beep_start(void) {
    digitalWrite(BUZZER_PIN, HIGH);
}

void buzzer_beep_stop(void) {
    digitalWrite(BUZZER_PIN, LOW);
}

long diff_ms(struct timespec start, struct timespec end) {
    long seconds = end.tv_sec - start.tv_sec;
    long nanoseconds = end.tv_nsec - start.tv_nsec;

    return seconds * 1000 + nanoseconds / 1000000;
}

void add_ms_to_timespec(struct timespec *t, long ms) {
    t->tv_sec += ms / 1000;
    t->tv_nsec += (ms % 1000) * 1000000L;

    if (t->tv_nsec >= 1000000000L) {
        t->tv_sec += 1;
        t->tv_nsec -= 1000000000L;
    }
}

void sleep_until(struct timespec target_time) {
    int ret;

    do {
        ret = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &target_time, NULL);
    } while (ret == EINTR && running);
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

    struct timespec next_beat_time;
    struct timespec previous_beat_time;
    int has_previous_beat = 0;

    printf("=================================================\n");
    printf(" Metrônomo integrado: LED + Servo + Buzzer\n");
    printf(" BPM fixo: %d\n", BPM);
    printf(" Periodo nominal: %d ms\n", PERIOD_MS);
    printf(" Servo: pulso entre 1 ms e 2 ms\n");
    printf(" LED: rampa luminosa PWM\n");
    printf(" Temporizacao: CLOCK_MONOTONIC + TIMER_ABSTIME\n");
    printf("=================================================\n");

    signal(SIGINT, cleanup);

    if (wiringPiSetupGpio() == -1) {
        printf("Erro ao inicializar wiringPi.\n");
        return 1;
    }

    /*
     * Inicialização do LED com PWM por software.
     */
    if (softPwmCreate(LED_PIN, 0, LED_PWM_RANGE) != 0) {
        printf("Erro ao inicializar PWM do LED.\n");
        return 1;
    }

    /*
     * Inicialização do servo com PWM por software.
     * Range 200 -> período aproximado de 20 ms -> 50 Hz.
     */
    if (softPwmCreate(SERVO_PIN, 0, SERVO_PWM_RANGE) != 0) {
        printf("Erro ao inicializar PWM do servo.\n");
        return 1;
    }

    /*
     * Inicialização do buzzer ativo.
     */
    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

    /*
     * Posição inicial segura do servo.
     */
    servo_write_angle(90);
    delay(1000);

    /*
     * Define o primeiro instante absoluto de batida.
     */
    clock_gettime(CLOCK_MONOTONIC, &next_beat_time);

    while (running) {
        struct timespec actual_beat_time;
        long interval_ms = 0;
        long jitter_ms = 0;

        /*
         * Espera até o instante absoluto programado.
         * Isso evita que pequenos atrasos acumulem drift.
         */
        sleep_until(next_beat_time);

        /*
         * Marca o instante real em que a batida começou.
         */
        clock_gettime(CLOCK_MONOTONIC, &actual_beat_time);

        if (has_previous_beat) {
            interval_ms = diff_ms(previous_beat_time, actual_beat_time);
            jitter_ms = interval_ms - PERIOD_MS;
        }

        previous_beat_time = actual_beat_time;
        has_previous_beat = 1;

        /*
         * Agenda a próxima batida antes de executar os atuadores.
         * Isso mantém o ciclo preso ao relógio absoluto.
         */
        add_ms_to_timespec(&next_beat_time, PERIOD_MS);

        /*
         * Alterna o lado do servo em cada batida.
         */
        if (servo_angle == SERVO_LEFT_ANGLE) {
            servo_angle = SERVO_RIGHT_ANGLE;
        } else {
            servo_angle = SERVO_LEFT_ANGLE;
        }

        /*
         * Ação da batida:
         * - move servo
         * - inicia beep
         * - faz rampa luminosa do LED
         * - desliga beep após BEEP_MS
         *
         * O beep é desligado antes da rampa terminar.
         */
        servo_write_angle(servo_angle);

        buzzer_beep_start();
        delay(BEEP_MS);
        buzzer_beep_stop();

        led_ramp();

        beat_count++;

        if (beat_count == 1) {
            printf("Batida %d | BPM: %d | Servo: %d graus | primeira amostra\n",
                   beat_count,
                   BPM,
                   servo_angle);
        } else {
            printf("Batida %d | intervalo: %ld ms | jitter: %+ld ms | Servo: %d graus\n",
                   beat_count,
                   interval_ms,
                   jitter_ms,
                   servo_angle);
        }

        /*
         * Se por algum motivo o ciclo atrasar demais, evita acumular erro indefinidamente.
         * Em uso normal, essa condição não deve ocorrer.
         */
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        if ((now.tv_sec > next_beat_time.tv_sec) ||
            (now.tv_sec == next_beat_time.tv_sec && now.tv_nsec > next_beat_time.tv_nsec)) {
            printf("[AVISO] O ciclo atrasou além do próximo período. Reajustando agenda.\n");
            next_beat_time = now;
            add_ms_to_timespec(&next_beat_time, PERIOD_MS);
        }
    }

    cleanup(0);
    return 0;
}
