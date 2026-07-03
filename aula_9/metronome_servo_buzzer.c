#include <wiringPi.h>
#include <softPwm.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <time.h>

#define SERVO_PIN 18
#define BUZZER_PIN 23

#define SERVO_RANGE 200
#define SERVO_MIN 5
#define SERVO_MAX 25

#define ANGLE_LEFT 60
#define ANGLE_RIGHT 120

#define PERIOD_MS 1000
#define BEEP_MS 80

static void cleanup(int sig) {
    softPwmWrite(SERVO_PIN, 0);
    digitalWrite(BUZZER_PIN, LOW);

    pinMode(SERVO_PIN, INPUT);
    pinMode(BUZZER_PIN, INPUT);

    printf("\nMetrônomo encerrado com segurança.\n");
    exit(0);
}

int angle_to_pwm(int angle) {
    if (angle < 0) {
        angle = 0;
    }

    if (angle > 180) {
        angle = 180;
    }

    return SERVO_MIN + (angle * (SERVO_MAX - SERVO_MIN)) / 180;
}

void servo_write_angle(int angle) {
    softPwmWrite(SERVO_PIN, angle_to_pwm(angle));
}

void beep(int duration_ms) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(duration_ms);
    digitalWrite(BUZZER_PIN, LOW);
}

long elapsed_ms(struct timespec start, struct timespec end) {
    long seconds = end.tv_sec - start.tv_sec;
    long nanoseconds = end.tv_nsec - start.tv_nsec;
    return seconds * 1000 + nanoseconds / 1000000;
}

int main(void) {
    int beat_count = 0;
    int current_angle = ANGLE_LEFT;

    signal(SIGINT, cleanup);

    printf("Metrônomo com servo e buzzer - período de 1 segundo\n");

    if (wiringPiSetupGpio() == -1) {
        printf("Erro ao inicializar wiringPi.\n");
        return 1;
    }

    softPwmCreate(SERVO_PIN, 0, SERVO_RANGE);

    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);

    servo_write_angle(90);
    delay(1000);

    while (1) {
        struct timespec t_start;
        struct timespec t_end;

        clock_gettime(CLOCK_MONOTONIC, &t_start);

        if (current_angle == ANGLE_LEFT) {
            current_angle = ANGLE_RIGHT;
        } else {
            current_angle = ANGLE_LEFT;
        }

        servo_write_angle(current_angle);
        beep(BEEP_MS);

        beat_count++;
        printf("Batida %d | angulo: %d graus\n", beat_count, current_angle);

        clock_gettime(CLOCK_MONOTONIC, &t_end);

        long used = elapsed_ms(t_start, t_end);
        long remaining = PERIOD_MS - used;

        if (remaining > 0) {
            delay(remaining);
        } else {
            printf("Aviso: ciclo estourou o período em %ld ms\n", -remaining);
        }
    }

    return 0;
}
