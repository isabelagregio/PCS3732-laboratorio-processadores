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

void calculaMediaDesvio(double tempos[], double *media, double *desvio) {
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

void benchmarkOperacao(const char *nome, uint64_t (*op2)(uint64_t, uint64_t),
                       uint64_t a, uint64_t b) {
  double tempos[MEDIDAS];
  volatile uint64_t r = 0;

  for (int i = 0; i < MEDIDAS; i++) {
    uint64_t ini = micros();
    r = op2(a, b);
    uint64_t fim = micros();
    tempos[i] = fim - ini;
  }

  double media, desvio;
  calculaMediaDesvio(tempos, &media, &desvio);

  Serial.print(nome);
  Serial.print(": media = ");
  Serial.print(media, 4);
  Serial.print(" us | desvio = ");
  Serial.print(desvio, 4);
  Serial.print(" us | resultado = ");
  Serial.println((unsigned long long)r);
}

void benchmarkDivisao(uint64_t a, uint64_t b) {
  double tempos[MEDIDAS];
  volatile uint64_t r = 0;

  if (b == 0) {
    Serial.println("Divisao: erro, divisor zero");
    return;
  }

  for (int i = 0; i < MEDIDAS; i++) {
    uint64_t ini = micros();
    divisao(a, b, (uint64_t *)&r);
    uint64_t fim = micros();
    tempos[i] = fim - ini;
  }

  double media, desvio;
  calculaMediaDesvio(tempos, &media, &desvio);

  Serial.print("Divisao: media = ");
  Serial.print(media, 4);
  Serial.print(" us | desvio = ");
  Serial.print(desvio, 4);
  Serial.print(" us | resultado = ");
  Serial.println((unsigned long long)r);
}

void benchmarkFatorial(uint64_t n) {
  double tempos[MEDIDAS];
  volatile uint64_t r = 0;

  for (int i = 0; i < MEDIDAS; i++) {
    uint64_t ini = micros();
    r = fatorial(n);
    uint64_t fim = micros();
    tempos[i] = fim - ini;
  }

  double media, desvio;
  calculaMediaDesvio(tempos, &media, &desvio);

  Serial.print("Fatorial: media = ");
  Serial.print(media, 4);
  Serial.print(" us | desvio = ");
  Serial.print(desvio, 4);
  Serial.print(" us | resultado = ");
  Serial.println((unsigned long long)r);
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("=== Benchmark ESP32 RISC-V ===");

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

    Serial.println();
    Serial.print("--- ");
    Serial.print(n);
    Serial.println(" bits ---");

    benchmarkOperacao("Soma", soma, x, y);
    benchmarkOperacao("Subtracao", subtracao, x, y);
    benchmarkOperacao("Multiplicacao", multiplicacao, x, y);
    benchmarkDivisao(x, y);

    uint64_t fat_n = (n <= 8) ? 10 : 20;
    benchmarkFatorial(fat_n);
  }

  Serial.println();
  Serial.println("Teste de divisao por zero:");
  uint64_t r;
  if (divisao(10, 0, &r) == -1) {
    Serial.println("ERRO: divisao por zero. Operacao cancelada.");
  }
}

void loop() {
}
