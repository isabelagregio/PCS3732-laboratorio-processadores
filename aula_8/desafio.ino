#include <Wire.h>
#include <Keypad.h>
#include <LiquidCrystal_I2C.h>

#define LCD_ADDR 0x27
#define LCD_COLS 16
#define LCD_ROWS 2

LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);

const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  {'1', '2', '3', '+'},
  {'4', '5', '6', '-'},
  {'7', '8', '9', '*'},
  {'C', '0', '!', '/'}
};

byte rowPins[ROWS] = {13, 12, 14, 27};
byte colPins[COLS] = {26, 25, 33, 32};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

String inputA = "";
String inputB = "";
char operacao = '\0';
bool esperandoSegundoNumero = false;

uint64_t soma(uint64_t a, uint64_t b) {
  return a + b;
}

uint64_t subtracao(uint64_t a, uint64_t b) {
  return a - b;
}

uint64_t multiplicacao(uint64_t a, uint64_t b) {
  uint64_t res = 0;

  while (b > 0) {
    if (b & 1) {
      res += a;
    }

    a <<= 1;
    b >>= 1;
  }

  return res;
}

int divisao(uint64_t a, uint64_t b, uint64_t *resultado) {
  if (b == 0) {
    return -1;
  }

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

void limparCalculadora() {
  inputA = "";
  inputB = "";
  operacao = '\0';
  esperandoSegundoNumero = false;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Calc 4 bits");
  lcd.setCursor(0, 1);
  lcd.print("Digite A");
}

void mostrarExpressao() {
  lcd.clear();
  lcd.setCursor(0, 0);

  if (inputA.length() > 0) {
    lcd.print(inputA);
  }

  if (operacao != '\0') {
    lcd.print(" ");
    lcd.print(operacao);
    lcd.print(" ");
  }

  if (inputB.length() > 0) {
    lcd.print(inputB);
  }
}

void mostrarErro(const char *msg) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("ERRO:");
  lcd.setCursor(0, 1);
  lcd.print(msg);
  delay(2000);
  limparCalculadora();
}

void calcularResultado() {
  if (inputA.length() == 0 || operacao == '\0') {
    mostrarErro("Entrada invalida");
    return;
  }

  uint64_t a = inputA.toInt();
  uint64_t b = inputB.toInt();
  uint64_t resultado = 0;

  if (a > 15 || b > 15) {
    mostrarErro("Fora de 4 bits");
    return;
  }

  switch (operacao) {
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
        mostrarErro("Div por zero");
        return;
      }
      break;

    case '!':
      resultado = fatorial(a);
      break;

    default:
      mostrarErro("Op invalida");
      return;
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Resultado:");
  lcd.setCursor(0, 1);
  lcd.print((unsigned long long)resultado);

  delay(4000);
  limparCalculadora();
}

void setup() {
  Wire.begin(21, 22);

  lcd.init();
  lcd.backlight();

  limparCalculadora();
}

void loop() {
  char tecla = keypad.getKey();

  if (!tecla) {
    return;
  }

  if (tecla == 'C') {
    limparCalculadora();
    return;
  }

  if (tecla >= '0' && tecla <= '9') {
    if (!esperandoSegundoNumero) {
      inputA += tecla;
    } else {
      inputB += tecla;
    }

    mostrarExpressao();
    return;
  }

  if (tecla == '+' || tecla == '-' || tecla == '*' || tecla == '/') {
    if (inputA.length() == 0) {
      mostrarErro("Digite A");
      return;
    }

    operacao = tecla;
    esperandoSegundoNumero = true;
    mostrarExpressao();
    return;
  }

  if (tecla == '!') {
    if (inputA.length() == 0) {
      mostrarErro("Digite A");
      return;
    }

    operacao = '!';
    calcularResultado();
    return;
  }
}
