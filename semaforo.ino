const int PINO_SEMAFORO = RGB_BUILTIN; 

void setup() {
  // Inicia garantindo que o LED comece apagado (valores zero para R, G e B)
  neopixelWrite(PINO_SEMAFORO, 0, 0, 0); 
}

void loop() {
  // --- FASE 1: VERDE (3 segundos) ---
  neopixelWrite(PINO_SEMAFORO, 0, 255, 0); 
  delay(3000);                      

  // --- FASE 2: AMARELO/ÂMBAR (1 segundo) ---
  // Misturamos R=255 e G=150 (mantendo o B zerado)
  neopixelWrite(PINO_SEMAFORO, 255, 150, 0); 
  delay(1000);                      

  // --- FASE 3: VERMELHO (4 segundos) ---
  neopixelWrite(PINO_SEMAFORO, 255, 0, 0); 
  delay(4000);                      
}
