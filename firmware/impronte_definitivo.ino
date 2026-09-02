#include <Adafruit_Fingerprint.h> // libreria per l'utilizzo delle impronte digitali
#include <HardwareSerial.h> // libreria per l'utilizzo delle orte seriali hardware

#define buzzer 5 // definisco il pin per il mio buzzer
#define SUONO_OK 33 // definisco il pin per cui se rilevo un 1 dall'esp32 allora parte una musichetta felice
#define OK_IMPRONTE 27 // definisco il pin per cui se rilevo un'impronta giusta lo riporto all'esp32
#define SUONO_noOK 25 // definisco il pin per cui se rilevo un 1 dall'esp32 allora parte un suono triste

Adafruit_Fingerprint finger = Adafruit_Fingerprint(&Serial2); // creo l'oggetto sesnsore di impronte digitali che punta alla seriale 1

unsigned long t = 0; // temporizzatore multitasking
void setup() {
Serial.begin(9600); // inizializzo i seriali
while (!Serial)
; 
delay(100);
Serial.println("\n\nAdafruit Fingerprint sensor enrollment");
finger.begin(57600);

pinMode(25, INPUT); // definisco input e output
pinMode(33, INPUT);
pinMode(27, OUTPUT);
millis();
}

bool impronta_ok;

void loop() {
if (digitalRead(SUONO_OK)) { // se l'esp32 mi comunica che il codice digitato sulla tastierà è giusto -> motivo felice
tone(buzzer, 1480, 80);
delay(240);
tone(buzzer, 1480, 40);
delay(120);
tone(buzzer, 1480, 40);
delay(120);
tone(buzzer, 1976, 1000);
}
if (digitalRead(SUONO_noOK)) { // se l'esp32 mi comunica che il codice digitato sulla tastierà è sbagliata -> suono triste
tone(buzzer, 1976, 200);
delay(100);
tone(buzzer, 1480, 200);
}
if (millis() - t >= 100) { // ogni 100ms cerco un'impronta sul sensore
t = millis();
impronta_ok = fingerprintMatches(); // "impronta ok" sarà true per una impronta trovata o false se l'immagine è scarsa o non è un'impronta registrata
}
if (impronta_ok) { // se trovo un'impronta -> motivo felice
digitalWrite(OK_IMPRONTE, 1); // comunixo all'esp32 che è stata trovata un'impronta
tone(buzzer, 1480, 80);
delay(240);
tone(buzzer, 1480, 40);
delay(120);
digitalWrite(OK_IMPRONTE, 0);
tone(buzzer, 1480, 40);
delay(120);
tone(buzzer, 1976, 1000);
}
}

bool fingerprintMatches() {
uint8_t result = finger.getImage();
if (result != FINGERPRINT_OK) {
return false;
}
result = finger.image2Tz();
if (result != FINGERPRINT_OK) {
return false;
}
result = finger.fingerSearch();
if (result != FINGERPRINT_OK) {
tone(buzzer, 1976, 200);
delay(100);
tone(buzzer, 1480, 200);
return false;
}
return true;
}
