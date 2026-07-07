/**********************************************************************
 * Projeto: Metrônomo com LED, Servomotor, Buzzer e Botões
 * Plataforma: Raspberry Pi 3B+
 *
 * Funcionamento:
 * - LED faz rampa luminosa usando PWM
 * - Servo alterna entre pulsos de 1 ms e 2 ms
 * - Buzzer emite beep curto a cada batida
 * - Botão em GPIO21 aumenta o BPM
 * - Botão em GPIO20 diminui o BPM
 *
 * Ligações:
 * - LED:
 *      GPIO17 -> resistor 220 ohms -> LED -> GND
 *
 * - Servo:
 *      Sinal  -> GPIO18
 *      VCC    -> 5V
 *      GND    -> GND
 *
 * - Buzzer ativo:
 *      Terminal + -> GPIO12
 *      Terminal - -> GND
 *
 * - Botões:
 *      Botão aumentar BPM:
 *          um terminal -> GPIO21
 *          outro terminal -> GND
 *
 *      Botão diminuir BPM:
 *          um terminal -> GPIO20
 *          outro terminal -> GND
 *
 * Compilar:
 * gcc metronome_botoes.c -o metronome_botoes -lwiringPi -lpthread
 *
 * Executar:
 * sudo ./metronome_botoes
 **********************************************************************/

#include <wiringPi.h>
#include <softPwm.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <pthread.h>

/* ==========================
   MAPEAMENTO DOS GPIOs - BCM
   ========================== */

#define LED_PIN       17   // GPIO17, pino físico 11
#define SERVO_PIN     18   // GPIO18, pino físico 12
#define BUZZER_PIN    12   // GPIO12, pino físico 16

#define BTN_UP_PIN    21   // aumenta BPM
#define BTN_DOWN_PIN  20   // diminui BPM

/* ==========================
   PARÂMETROS DO METRÔNOMO
   ========================== */

#define BPM_INITIAL  60
#define BPM_MIN      30
#define BPM_MAX     180
#define BPM_STEP      5

#define DEBOUNCE_MS 150

/* ==========================
   PARÂMETROS DO LED
   ========================== */

#define LED_PWM_RANGE       100
#define LED_RAMP_STEP         5
#define LED_RAMP_DELAY_MS     5

/* ==========================
   PARÂMETROS DO SERVO
   ========================== */

/*
 * Servo com PWM de 50 Hz:
 * período = 20 ms.
 *
 * softPwmCreate(pin, 0, 200)
 * range = 200
 * unidade aproximada = 0,1 ms
 *
 * 1,0 ms -> valor 10 -> 0°
 * 1,5 ms -> valor 15 -> 90°
 * 2,0 ms -> valor 20 -> 180°
 */
#define SERVO_PWM_RANGE    200
#define SERVO_LEFT_PULSE    10
#define SERVO_MID_PULSE     15
#define SERVO_RIGHT_PULSE   20

/* ==========================
   PARÂMETROS DO BUZZER
   ========================== */

#define BEEP_MS 80

/* ==========================
   VARIÁVEIS GLOBAIS
   ========================== */

volatile sig_atomic_t running = 1;

int current_bpm = BPM_INITIAL;

pthread_mutex_t bpm_mutex = PTHREAD_MUTEX_INITIALIZER;

volatile unsigned long last_up_press = 0;
volatile unsigned long last_down_press = 0;

/* ==========================
   FUNÇÕES DE BPM
   ========================== */

int get_bpm(void) {
    int bpm;

    pthread_mutex_lock(&bpm_mutex);
    bpm = current_bpm;
    pthread_mutex_unlock(&bpm_mutex);

    return bpm;
}

void set_bpm(int bpm) {
    pthread_mutex_lock(&bpm_mutex);

    if (bpm < BPM_MIN) {
        bpm = BPM_MIN;
    }

    if (bpm > BPM_MAX) {
        bpm = BPM_MAX;
    }

    current_bpm = bpm;

    pthread_mutex_unlock(&bpm_mutex);
}

int bpm_to_period_ms(int bpm) {
    return 60000 / bpm;
}

/* ==========================
   INTERRUPÇÕES DOS BOTÕES
   ========================== */

void increase_bpm(void) {
    unsigned long now = millis();

    if (now - last_up_press < DEBOUNCE_MS) {
        return;
    }

    last_up_press = now;

    int bpm = get_bpm();
    set_bpm(bpm + BPM_STEP);

    printf("[BOTAO +] BPM = %d | Frequencia = %.2f Hz\n",
           get_bpm(),
           get_bpm() / 60.0);
}

void decrease_bpm(void) {
    unsigned long now = millis();

    if (now - last_down_press < DEBOUNCE_MS) {
        return;
    }

    last_down_press = now;

    int bpm = get_bpm();
    set_bpm(bpm - BPM_STEP);

    printf("[BOTAO -] BPM = %d | Frequencia = %.2f Hz\n",
           get_bpm(),
           get_bpm() / 60.0);
}

/* ==========================
   FUNÇÕES DOS ATUADORES
   ========================== */

void servo_write_pulse(int pulse) {
    if (pulse < SERVO_LEFT_PULSE) {
        pulse = SERVO_LEFT_PULSE;
    }

    if (pulse > SERVO_RIGHT_PULSE) {
        pulse = SERVO_RIGHT_PULSE;
    }

    softPwmWrite(SERVO_PIN, pulse);
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

/* ==========================
   FUNÇÕES DE TEMPO
   ========================== */

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
    pinMode(BTN_UP_PIN, INPUT);
    pinMode(BTN_DOWN_PIN, INPUT);

    printf("\nMetrônomo encerrado com segurança.\n");
    exit(0);
}

/* ==========================
   MAIN
   ========================== */

int main(void) {
    int beat_count = 0;
    int servo_pulse = SERVO_LEFT_PULSE;

    struct timespec next_beat_time;
    struct timespec previous_beat_time;
    int has_previous_beat = 0;

    printf("=================================================\n");
    printf(" Metrônomo com LED + Servo + Buzzer + Botões\n");
    printf(" BPM inicial: %d\n", BPM_INITIAL);
    printf(" Botao +: GPIO%d\n", BTN_UP_PIN);
    printf(" Botao -: GPIO%d\n", BTN_DOWN_PIN);
    printf(" Faixa de BPM: %d a %d\n", BPM_MIN, BPM_MAX);
    printf("=================================================\n");

    signal(SIGINT, cleanup);

    if (wiringPiSetupGpio() == -1) {
        printf("Erro ao inicializar wiringPi.\n");
        return 1;
    }

    if (softPwmCreate(LED_PIN, 0, LED_PWM_RANGE) != 0) {
        printf("Erro ao inicializar PWM do LED.\n");
        return 1;
    }

    if (softPwmCreate(SERVO_PIN, 0, SERVO_PWM_RANGE) != 0) {
        printf("Erro ao inicializar PWM do servo.\n");
        return 1;
    }

    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

    /*
     * Configuração dos botões com pull-up interno.
     * Botão solto: HIGH
     * Botão pressionado: LOW
     */
    pinMode(BTN_UP_PIN, INPUT);
    pinMode(BTN_DOWN_PIN, INPUT);

    pullUpDnControl(BTN_UP_PIN, PUD_UP);
    pullUpDnControl(BTN_DOWN_PIN, PUD_UP);

    /*
     * Interrupções por borda de descida.
     * A borda de descida ocorre quando o botão é pressionado.
     */
    if (wiringPiISR(BTN_UP_PIN, INT_EDGE_FALLING, &increase_bpm) < 0) {
        printf("Erro ao configurar interrupcao do botao de aumentar BPM.\n");
        return 1;
    }

    if (wiringPiISR(BTN_DOWN_PIN, INT_EDGE_FALLING, &decrease_bpm) < 0) {
        printf("Erro ao configurar interrupcao do botao de diminuir BPM.\n");
        return 1;
    }

    /*
     * Posição inicial segura do servo.
     */
    servo_write_pulse(SERVO_MID_PULSE);
    delay(1000);

    clock_gettime(CLOCK_MONOTONIC, &next_beat_time);

    while (running) {
        struct timespec actual_beat_time;
        struct timespec now;

        int bpm = get_bpm();
        int period_ms = bpm_to_period_ms(bpm);

        long interval_ms = 0;
        long jitter_ms = 0;

        /*
         * Espera até o instante absoluto da próxima batida.
         */
        sleep_until(next_beat_time);

        clock_gettime(CLOCK_MONOTONIC, &actual_beat_time);

        if (has_previous_beat) {
            interval_ms = diff_ms(previous_beat_time, actual_beat_time);
            jitter_ms = interval_ms - period_ms;
        }

        previous_beat_time = actual_beat_time;
        has_previous_beat = 1;

        /*
         * Atualiza BPM após acordar, caso algum botão tenha sido pressionado.
         */
        bpm = get_bpm();
        period_ms = bpm_to_period_ms(bpm);

        /*
         * Agenda próxima batida com base no BPM atual.
         */
        add_ms_to_timespec(&next_beat_time, period_ms);

        /*
         * Alterna o servo entre 1 ms e 2 ms.
         */
        if (servo_pulse == SERVO_LEFT_PULSE) {
            servo_pulse = SERVO_RIGHT_PULSE;
        } else {
            servo_pulse = SERVO_LEFT_PULSE;
        }

        servo_write_pulse(servo_pulse);

        /*
         * Buzzer e LED indicam a batida.
         */
        buzzer_beep_start();
        delay(BEEP_MS);
        buzzer_beep_stop();

        led_ramp();

        beat_count++;

        if (beat_count == 1) {
            printf("Batida %d | BPM: %d | Frequencia: %.2f Hz | Servo pulse: %d | primeira amostra\n",
                   beat_count,
                   bpm,
                   bpm / 60.0,
                   servo_pulse);
        } else {
            printf("Batida %d | BPM: %d | Frequencia: %.2f Hz | intervalo: %ld ms | jitter: %+ld ms | Servo pulse: %d\n",
                   beat_count,
                   bpm,
                   bpm / 60.0,
                   interval_ms,
                   jitter_ms,
                   servo_pulse);
        }

        /*
         * Se atrasar além do próximo período, reajusta a agenda.
         */
        clock_gettime(CLOCK_MONOTONIC, &now);

        if ((now.tv_sec > next_beat_time.tv_sec) ||
            (now.tv_sec == next_beat_time.tv_sec &&
             now.tv_nsec > next_beat_time.tv_nsec)) {

            printf("[AVISO] Ciclo atrasou. Reajustando agenda.\n");

            next_beat_time = now;
            add_ms_to_timespec(&next_beat_time, period_ms);
        }
    }

    cleanup(0);
    return 0;
}
