#include <bluefruit.h>
#include "NRF52TimerInterrupt.h"
#include "SparkFun_BMI270_Arduino_Library.h"
#include <Wire.h>

// Create a new sensor object
BMI270 imu; // IMU

// I2C address selection
uint8_t i2cAddress = BMI2_I2C_PRIM_ADDR; // 0x68

// Pin di selezione del multiplexer (fino a 8 canali)
const int S0 = A4; 
const int S1 = A5;
const int S2 = A6;

// Ingresso analogico del multiplexer
const int muxAnalogPin = A7;

// Altri due FSR diretti (senza MUX)
const int fsr9Pin  = A2; // 9° FSR
const int fsr10Pin = A3; // 10° FSR

// Variabili per i segnali FSR
volatile uint16_t fsrValues[10];  // 10 FSR in totale
volatile int currentChannel = 0;  
volatile bool dataReady = false;  

// Dichiarazioni per BLE
BLEService fsrService = BLEService(0x180A);  
BLECharacteristic fsrCharacteristic = BLECharacteristic(0x2A58);

// bufferFSR BLE (10 FSR * 2 byte = 20 byte)
uint8_t bufferFSR[20];

// Timer configurazione
NRF52Timer ITimer0(NRF_TIMER_1);

// Intervallo timer (es. 1 ms = 1000 µs)
#define TIMER_INTERVAL_US 1000

// Funzione richiamata dal timer
void timerISR() 
{
  if (currentChannel < 8) 
  {
    // Selezioniamo il canale sul multiplexer (0-7)
    selectChannel(currentChannel);
    fsrValues[currentChannel] = analogRead(muxAnalogPin);

    // Aggiunta dell'header (bit 14 e 15 = 1) solo sul primo canale
    if (currentChannel == 0) {
      fsrValues[0] |= 0xC000;
    }
  }
  else if (currentChannel == 8) 
  {
    // Leggi direttamente da A2
    fsrValues[8] = analogRead(fsr9Pin);
  }
  else if (currentChannel == 9) 
  {
    // Leggi direttamente da A3
    fsrValues[9] = analogRead(fsr10Pin);
  }

  currentChannel++;
  if (currentChannel >= 10) 
  {
    currentChannel = 0;
    dataReady = true;
  }
}

void setup() 
{
  // Configurazione pin multiplexer
  pinMode(S0, OUTPUT);
  pinMode(S1, OUTPUT);
  pinMode(S2, OUTPUT);
  pinMode(muxAnalogPin, INPUT);

  // Configurazione pin analogici aggiuntivi
  pinMode(fsr9Pin, INPUT);
  pinMode(fsr10Pin, INPUT);

  // Serial
  Serial.begin(9600);
  Serial.println("Inizializzazione del sistema...");

  // Inizializzazione BLE
  Bluefruit.begin();
  Bluefruit.setTxPower(4);  // potenza massima
  Bluefruit.setName("FSR Sensor");

  // Configurazione del servizio e caratteristica BLE
  fsrService.begin();
  fsrCharacteristic.setProperties(CHR_PROPS_NOTIFY);
  fsrCharacteristic.setPermission(SECMODE_OPEN, SECMODE_NO_ACCESS);
  fsrCharacteristic.setFixedLen(sizeof(bufferFSR)); // 20 byte
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

  // Inizializzazione IMU
  Serial.println("Inizializzazione IMU...");
  Wire.begin();
  while (imu.beginI2C(i2cAddress) != BMI2_OK) {
    Serial.println("Errore: BMI270 non connesso. Controllare il cablaggio e l'indirizzo I2C!");
    delay(1000);
  }
  Serial.println("BMI270 connesso!");
}

void loop() 
{
  if (dataReady) 
  {
    dataReady = false; // reset flag

    // Prepara il buffer BLE
    for (int i = 0; i < 10; i++) {
      bufferFSR[i * 2]     = fsrValues[i] & 0xFF;
      bufferFSR[i * 2 + 1] = (fsrValues[i] >> 8);
    }

    // Debug seriale
    Serial.print("Bytes inviati: ");
    for (int i = 0; i < 10; i++) {
      uint16_t value = bufferFSR[i * 2] | (bufferFSR[i * 2 + 1] << 8);
      Serial.print(value, DEC);
      Serial.print(" ");
    }
    Serial.println();

    // Leggi i dati IMU
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

// Funzione per selezionare il canale del multiplexer (0–7)
void selectChannel(int channel) 
{
  digitalWrite(S0, bitRead(channel, 0));
  digitalWrite(S1, bitRead(channel, 1));
  digitalWrite(S2, bitRead(channel, 2));
}
