const int pinosLeds[] = {6, 7, 8, 9};
const int totalLeds = 4;

String operandoA;
String operandoB;
char operacao;

// ===== Mostra valor nos LEDs =====
void mostrarNosLeds(byte valor) {
  for (int i = 0; i < totalLeds; i++) {
    digitalWrite(pinosLeds[i], (valor >> i) & 1);
  }
}

// ===== Binário string -> inteiro =====
byte binarioParaByte(String binario) {
  return strtol(binario.c_str(), NULL, 2);
}

// ===== Inteiro -> string binária =====
String byteParaBinario(byte valor) {
  String resultado = "";
  for (int i = 3; i >= 0; i--) {
    resultado += String((valor >> i) & 1);
  }
  return resultado;
}

// ===== Complemento de 1 =====
byte complementoDeUm(byte valor) {
  return (~valor) & 0x0F;
}

// ===== Soma em complemento de 1 =====
byte somaComplemento1(byte a, byte b) {
  byte resultado = a + b;

  // carry circular
  if (resultado > 0x0F) {
    resultado = (resultado & 0x0F) + 1;
  }

  return resultado & 0x0F;
}

// ===== Subtração =====
byte subtracaoComplemento1(byte a, byte b) {
  byte comp1 = complementoDeUm(b);
  return somaComplemento1(a, comp1);
}

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < totalLeds; i++) {
    pinMode(pinosLeds[i], OUTPUT);
    digitalWrite(pinosLeds[i], LOW);
  }

  Serial.println("=== CALCULADORA 4 BITS ===");
  Serial.println("Complemento de 1");
}

void loop() {
  Serial.println("\nDigite operando A (4 bits):");
  while (!Serial.available()) {}
  operandoA = Serial.readStringUntil('\n');
  operandoA.trim();

  Serial.println("Digite operando B (4 bits):");
  while (!Serial.available()) {}
  operandoB = Serial.readStringUntil('\n');
  operandoB.trim();

  Serial.println("Digite operacao (+ ou -):");
  while (!Serial.available()) {}
  
  // CORREÇÃO 1: Lendo como String para consumir o '\n' do buffer por completo
  String opStr = Serial.readStringUntil('\n');
  opStr.trim();
  if (opStr.length() > 0) {
    operacao = opStr[0];
  } else {
    operacao = ' ';
  }

  // CORREÇÃO 2: Aplicando a máscara & 0x0F para garantir isolamento de 4 bits
  byte valorA = binarioParaByte(operandoA) & 0x0F;
  byte valorB = binarioParaByte(operandoB) & 0x0F;

  byte resultado;

  // ===== SOMA =====
  if (operacao == '+') {
    resultado = somaComplemento1(valorA, valorB);
    Serial.println("\nOperacao: SOMA");
  }
  // ===== SUBTRAÇÃO =====
  else if (operacao == '-') {
    resultado = subtracaoComplemento1(valorA, valorB);
    Serial.println("\nOperacao: SUBTRACAO");
  }
  else {
    Serial.println("Operacao invalida.");
    return;
  }

  mostrarNosLeds(resultado);

  Serial.print("Resultado binario: ");
  Serial.println(byteParaBinario(resultado));

  delay(1000);
}
