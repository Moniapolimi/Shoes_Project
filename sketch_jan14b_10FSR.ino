#include <bluefruit.h>
#include "NRF52TimerInterrupt.h"
#include "SparkFun_BMI270_Arduino_Library.h"
#include <Wire.h>

// Create a new sensor object
BMI270 imu; // <-- Aggiunta (già presente nel tuo codice originale)

// I2C address selection
uint8_t i2cAddress = BMI2_I2C_PRIM_ADDR; // 0x68

// Pin di selezione del multiplexer
const int S0 = A4; // Pin per il bit 0
const int S1 = A5; // Pin per il bit 1
const int S2 = A6; // Pin per il bit 2
const int S3 = A8; // Pin per il bit 3 (nuovo per arrivare a 10 canali)

// Ingresso analogico del multiplexer
const int analogPin = A7;

// Variabili per i segnali FSR
volatile uint16_t fsrValues[10]; // Ora 10 elementi
volatile int currentChannel = 0; // Canale corrente del multiplexer
volatile bool dataReady = false; // Flag per indicare che i dati sono pronti

// Dichiarazioni per BLE
BLEService fsrService = BLEService(0x180A);  // ID del servizio BLE
BLECharacteristic fsrCharacteristic = BLECharacteristic(0x2A58); // ID caratteristica BLE

// bufferFSR BLE: 10 canali * 2 byte = 20 byte
uint8_t bufferFSR[20];

// Timer configurazione
NRF52Timer ITimer0(NRF_TIMER_1);

// Intervallo timer in microsecondi (esempio: 1 ms = 1000 µs)
#define TIMER_INTERVAL_US 1000

// Funzione gestita dal timer
void timerISR() {
  selectChannel(currentChannel);
  fsrValues[currentChannel] = analogRead(analogPin);

  // Esempio: Aggiunta di un "header" solo sul primo canale
  if (currentChannel == 0){
    fsrValues[currentChannel] = fsrValues[currentChannel] | 0xC000;
  }

  currentChannel++;
  // Ora ci fermiamo a 10 canali
  if (currentChannel >= 10) {
    currentChannel = 0;
    dataReady = true;
  }
}

void setup() {
  // Configurazione dei pin digitali per la selezione
  pinMode(analogPin, INPUT);
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(S3, OUTPUT); // Nuovo pin di selezione

  // Configurazione seriale
  Serial.begin(9600);
  Serial.println("Inizializzazione del sistema...");

  // Inizializzazione BLE
  Bluefruit.begin();
  Bluefruit.setTxPower(4);  // Potenza massima
  Bluefruit.setName("FSR Sensor");

  // Configurazione del servizio e caratteristica BLE
  fsrService.begin();
  fsrCharacteristic.setProperties(CHR_PROPS_NOTIFY);
  fsrCharacteristic.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);

  // Adattiamo la lunghezza fissa della caratteristica da 16 a 20 byte
  fsrCharacteristic.setFixedLen(sizeof(bufferFSR));

  fsrCharacteristic.begin();

  // Avvia advertising BLE
  Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
  Bluefruit.Advertising.addTxPower();
  Bluefruit.Advertising.addService(fsrService);
  Bluefruit.Advertising.start(0);

  // Configura il timer
  if (ITimer0.attachInterruptInterval(TIMER_INTERVAL_US, timerISR)) {
    Serial.println(F("Timer configurato correttamente."));
  } else {
    Serial.println(F("Errore nella configurazione del timer."));
  }

  // <-- Aggiunta: Inizializzazione IMU
  Serial.println("Inizializzazione IMU...");
  Wire.begin();

  // Loop finché l'IMU non è connessa
  while (imu.beginI2C(i2cAddress) != BMI2_OK) {
    Serial.println("Errore: BMI270 non connesso, controllare il cablaggio e l'indirizzo I2C!");
    delay(1000);
  }

  Serial.println("BMI270 connesso!");
}

void loop() {
  if (dataReady) {
    dataReady = false; // Resetta il flag

    // Prepara il bufferFSR BLE per i valori FSR
    for (int i = 0; i < 10; i++) {
      bufferFSR[i * 2]     = fsrValues[i] & 0xFF;
      bufferFSR[i * 2 + 1] = (fsrValues[i] >> 8);
    }

    // Debug seriale: stampa i valori (in decimale)
    Serial.print("Bytes inviati: ");
    for (int i = 0; i < 10; i++) {
      uint16_t value = bufferFSR[i * 2] | (bufferFSR[i * 2 + 1] << 8);
      Serial.print(value, DEC);
      Serial.print(" ");
    }
    Serial.println();

    // Acquisizione dati IMU
    imu.getSensorData();
    Serial.print("Acceleration in g's\tX: ");
    Serial.print(imu.data.accelX, 3);
    Serial.print("\tY: ");
    Serial.print(imu.data.accelY, 3);
    Serial.print("\tZ: ");
    Serial.print(imu.data.accelZ, 3);

    Serial.print("\tRotation in deg/sec\tX: ");
    Serial.print(imu.data.gyroX, 3);
    Serial.print("\tY: ");
    Serial.print(imu.data.gyroY, 3);
    Serial.print("\tZ: ");
    Serial.println(imu.data.gyroZ, 3);

    // Invia il bufferFSR via BLE
    fsrCharacteristic.notify(bufferFSR, sizeof(bufferFSR));
  }
}

// Funzione per selezionare il canale del multiplexer
void selectChannel(int channel) {
  digitalWrite(S0, bitRead(channel, 0));
  digitalWrite(S1, bitRead(channel, 1));
  digitalWrite(S2, bitRead(channel, 2));
  digitalWrite(S3, bitRead(channel, 3)); // Necessario per arrivare fino a 10
}
