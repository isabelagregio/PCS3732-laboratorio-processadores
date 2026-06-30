#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <math.h>

#define MEDIDAS 1000

uint64_t soma(uint64_t a, uint64_t b) {
    return a + b;
}

uint64_t subtracao(uint64_t a, uint64_t b) {
    return a - b;
}

uint64_t multiplicacao(uint64_t a, uint64_t b) {
    uint64_t res = 0;
    while (b > 0) {
        if (b & 1) res += a;
        a <<= 1;
        b >>= 1;
    }
    return res;
}

int divisao(uint64_t a, uint64_t b, uint64_t *resultado) {
    if (b == 0) return -1;
    *resultado = a / b;
    return 0;
}

uint64_t fatorial(uint64_t n) {
    uint64_t res = 1;
    for (uint64_t i = 2; i <= n; i++) {
        res *= i;
    }
    return res;
}

double tempo_ns() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e9 + (double)t.tv_nsec;
}

void calcula_media_desvio(double tempos[], double *media, double *desvio) {
    *media = 0;

    for (int i = 0; i < MEDIDAS; i++) {
        *media += tempos[i];
    }

    *media /= MEDIDAS;

    double var = 0;
    for (int i = 0; i < MEDIDAS; i++) {
        double d = tempos[i] - *media;
        var += d * d;
    }

    *desvio = sqrt(var / (MEDIDAS - 1));
}

void benchmark_operacao(const char *nome, uint64_t (*op2)(uint64_t, uint64_t),
                        uint64_t a, uint64_t b) {
    double tempos[MEDIDAS];
    volatile uint64_t r = 0;

    for (int i = 0; i < MEDIDAS; i++) {
        double ini = tempo_ns();
        r = op2(a, b);
        double fim = tempo_ns();
        tempos[i] = fim - ini;
    }

    double media, desvio;
    calcula_media_desvio(tempos, &media, &desvio);

    printf("%s: media = %.2f ns | desvio = %.2f ns | resultado = %llu\n",
           nome, media, desvio, (unsigned long long)r);
}

void benchmark_divisao(uint64_t a, uint64_t b) {
    double tempos[MEDIDAS];
    volatile uint64_t r = 0;

    if (b == 0) {
        printf("Divisao: erro, divisor zero\n");
        return;
    }

    for (int i = 0; i < MEDIDAS; i++) {
        double ini = tempo_ns();
        divisao(a, b, (uint64_t *)&r);
        double fim = tempo_ns();
        tempos[i] = fim - ini;
    }

    double media, desvio;
    calcula_media_desvio(tempos, &media, &desvio);

    printf("Divisao: media = %.2f ns | desvio = %.2f ns | resultado = %llu\n",
           media, desvio, (unsigned long long)r);
}

void benchmark_fatorial(uint64_t n) {
    double tempos[MEDIDAS];
    volatile uint64_t r = 0;

    for (int i = 0; i < MEDIDAS; i++) {
        double ini = tempo_ns();
        r = fatorial(n);
        double fim = tempo_ns();
        tempos[i] = fim - ini;
    }

    double media, desvio;
    calcula_media_desvio(tempos, &media, &desvio);

    printf("Fatorial: media = %.2f ns | desvio = %.2f ns | resultado = %llu\n",
           media, desvio, (unsigned long long)r);
}

int main() {
    int a, b;
    char op;
    uint64_t resultado = 0;

    printf("=== Calculadora 4 bits - Raspberry Pi 3 ARM ===\n");
    printf("Operacoes: +, -, *, /, !\n");
    printf("Formato: A operador B\n");
    printf("Exemplos: 7 + 3 | 9 - 2 | 4 * 3 | 10 / 2 | 5 ! 0\n");
    printf("Valores de A e B devem estar entre 0 e 15.\n\n");

    printf("Entrada: ");
    scanf("%d %c %d", &a, &op, &b);

    if (a < 0 || a > 15 || b < 0 || b > 15) {
        printf("Erro: entrada fora de 4 bits.\n");
        return 1;
    }

    switch (op) {
        case '+':
            resultado = soma(a, b);
            break;

        case '-':
            resultado = subtracao(a, b);
            break;

        case '*':
            resultado = multiplicacao(a, b);
            break;

        case '/':
            if (divisao(a, b, &resultado) == -1) {
                printf("ERRO: divisao por zero.\n");
                printf("Operacao cancelada. Aguardando nova entrada.\n");
                return 1;
            }
            break;

        case '!':
            resultado = fatorial(a);
            break;

        default:
            printf("Erro: operador invalido.\n");
            return 1;
    }

    printf("Resultado decimal: %llu\n", (unsigned long long)resultado);
    printf("Resultado hexadecimal: 0x%llX\n", (unsigned long long)resultado);

    printf("\n=== Benchmark ===\n");

    int bits[] = {4, 8, 16, 32, 64};

    for (int i = 0; i < 5; i++) {
        int n = bits[i];

        uint64_t max;
        if (n == 64) {
            max = UINT64_MAX;
        } else {
            max = ((uint64_t)1 << n) - 1;
        }

        uint64_t x = max / 3;
        uint64_t y = max / 5;
        if (y == 0) y = 1;

        printf("\n--- %d bits ---\n", n);
        benchmark_operacao("Soma", soma, x, y);
        benchmark_operacao("Subtracao", subtracao, x, y);
        benchmark_operacao("Multiplicacao", multiplicacao, x, y);
        benchmark_divisao(x, y);

        uint64_t fat_n = (n <= 8) ? 10 : 20;
        benchmark_fatorial(fat_n);
    }

    return 0;
}
