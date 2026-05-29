// Definição dos pinos do ESP32 conectados aos 4 LEDs
const int pinosLeds[] = {6, 7, 8, 9}; 
const int totalLeds = 4;

void setup() {
  // Inicializa todos os pinos dos LEDs como saída (OUTPUT)
  for (int i = 0; i < totalLeds; i++) {
    pinMode(pinosLeds[i], OUTPUT);
    digitalWrite(pinosLeds[i], LOW); // Garante que todos iniciam apagados
  }
}

void loop() {
  // Percorre sequencialmente cada LED do array
  for (int i = 0; i < totalLeds; i++) {
    digitalWrite(pinosLeds[i], HIGH); // Acende o LED atual
    delay(500);                       // Mantém aceso por 500 milissegundos
    
    digitalWrite(pinosLeds[i], LOW);  // Apaga o LED atual antes de passar para o próximo
  }
}
