/**********************************************************************
* Filename    : Sweep.c
* Description : Servo sweep using 1ms to 2ms pulse width
* Author      : Adaptado a partir de www.freenove.com
**********************************************************************/

#include <wiringPi.h>
#include <softPwm.h>
#include <stdio.h>

#define servoPin 18

/*
 * softPwmCreate(pin, 0, 200)
 * Range 200 => período aproximado de 20 ms => 50 Hz
 * Cada unidade equivale a aproximadamente 0,1 ms.
 *
 * Especificação:
 * 1,0 ms -> 0 graus   -> valor 10
 * 1,5 ms -> 90 graus  -> valor 15
 * 2,0 ms -> 180 graus -> valor 20
 */
#define SERVO_MIN_PULSE 10
#define SERVO_MID_PULSE 15
#define SERVO_MAX_PULSE 20
#define SERVO_PWM_RANGE 200

long map_value(long value, long fromLow, long fromHigh, long toLow, long toHigh) {
    return (toHigh - toLow) * (value - fromLow) / (fromHigh - fromLow) + toLow;
}

void servoInit(int pin) {
    softPwmCreate(pin, 0, SERVO_PWM_RANGE);
}

void servoWrite(int pin, int angle) {
    int pulse;

    if (angle > 180) {
        angle = 180;
    }

    if (angle < 0) {
        angle = 0;
    }

    pulse = map_value(angle, 0, 180, SERVO_MIN_PULSE, SERVO_MAX_PULSE);

    softPwmWrite(pin, pulse);
}

void servoWritePulse(int pin, int pulse) {
    if (pulse > SERVO_MAX_PULSE) {
        pulse = SERVO_MAX_PULSE;
    }

    if (pulse < SERVO_MIN_PULSE) {
        pulse = SERVO_MIN_PULSE;
    }

    softPwmWrite(pin, pulse);
}

int main(void) {
    int angle;

    printf("Program is starting ...\n");
    printf("Servo PWM: 50 Hz, periodo de 20 ms\n");
    printf("1.0 ms -> 0 graus | 1.5 ms -> 90 graus | 2.0 ms -> 180 graus\n");

    wiringPiSetupGpio();
    servoInit(servoPin);

    while (1) {
        /*
         * Varredura suave de 0 a 180 graus.
         */
        for (angle = 0; angle <= 180; angle += 1) {
            servoWrite(servoPin, angle);
            delay(15);
        }

        delay(500);

        /*
         * Varredura suave de 180 a 0 graus.
         */
        for (angle = 180; angle >= 0; angle -= 1) {
            servoWrite(servoPin, angle);
            delay(15);
        }

        delay(500);
    }

    return 0;
}
