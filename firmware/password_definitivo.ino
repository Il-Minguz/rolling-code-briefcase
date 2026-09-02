//TX
#include <Arduino.h>
#include "EBYTE.h"              // libreria per l'utilizzo del modulo LoRa
#include <Wire.h>               // libreria per l'utilizzo del display i2ce garantirne la compatibilità
#include <LiquidCrystal_I2C.h>  // libreria per l'utilizzo del display i2c

#define PIN_RX 16  // pin moduli LoRa
#define PIN_TX 17
#define PIN_M0 4
#define PIN_M1 18
#define PIN_AX 19

struct DATA {         // struttura dati da inviare o ricevere (deve essere contenuta in tutti e due i moduli)
  bool aperto;        // con "aperto" capisco se la valigia è stata aperta 0 = chiusa / 1 = aperta
  String passwordTX;  // "passwordTX" è la password trasmessa. Una password ogni volta che viene aperta la valigia
  bool connesso;      // "connesso" mi permette di capire se sono ancora connesso all'altro modulo
};

int Chan;                         // variabile canale, per cambiare frequenza. Il modulo è stato impostato sul canale 17 => 433MHz (Frequenza base) + 17MHz = 450MHZ
DATA MyData;                      // rinomino la struttura dati
unsigned long Last, timer_conto;  // questi sono tre timer multitasking utili per controlli

EBYTE Transceiver(&Serial2, PIN_M0, PIN_M1, PIN_AX);  // creo l'oggetto trasmettitore
LiquidCrystal_I2C lcd(0x27, 16, 2);                   // creo l'oggetto display

void setup() {
  Serial.begin(9600);                 //imizializzo seriali, display e trasmettitore
  Serial2.begin(9600);
  Serial.println("Starting password generator");
  lcd.init();
  lcd.backlight();
  Serial.println(Transceiver.init());
  millis();
  Transceiver.PrintParameters();
}

void loop() {
  if (Serial2.available()) {                        //estrapolo le informazioni dal modulo LoRa
    Transceiver.GetStruct(&MyData, sizeof(MyData));
    Serial.print("aperto: ");
    Serial.println(MyData.aperto);
  }
  if (MyData.aperto) {           // se la valigia è stata aperta 
    passwordGen(5);              // creo una password di una lunghezza scelta (nel mio caso 5 numeri)
  }
  if (millis() - timer_conto >= 5000) {
    timer_conto = millis();
    MyData.connesso = true;
    Transceiver.SendStruct(&MyData, sizeof(MyData));
  }
}

void passwordGen(int length) {
  String password = "";
  for (int i = 0; i < length; i++) {
    password += String(random(10));  // Genera un numero casuale tra 0 e 9 e lo aggiunge alla password
  }
  Serial.println(password);         // una volta creata la password la salvo e la invio
  MyData.passwordTX = password;
  MyData.aperto = false;
  Transceiver.SendStruct(&MyData, sizeof(MyData));
  lcd.setCursor(0, 0);
  lcd.print("Password:");
  lcd.setCursor(0, 1);
  lcd.print(password);
}
