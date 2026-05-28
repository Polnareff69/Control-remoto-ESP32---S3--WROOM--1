#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// =======================
// UUID BLE
// =======================
#define SERVICE_UUID "12345678-1234-1234-1234-1234567890ab"
#define RX_UUID      "abcd1234-5678-90ab-cdef-1234567890ab"
#define TX_UUID      "dcba4321-8765-09ba-fedc-0987654321ba"

// =======================
// MOTOR CÁMARA / ROTACIÓN
// =======================
#define EN_CAMARA    15
#define STEP_CAMARA  16
#define DIR_CAMARA   17

// =======================
// MOTOR ALTURA
// =======================
#define EN_ALTURA    5
#define STEP_ALTURA  6
#define DIR_ALTURA   7

// =======================
// FINAL DE CARRERA
// =======================
#define FINAL_ALTURA 10

// =======================
// DIRECCIONES (INVERTIDAS EN VIDA REAL)
// =======================
#define SUBIR LOW   // físicamente BAJA
#define BAJAR HIGH  // físicamente SUBE

// =======================
// VARIABLES BLE
// =======================
BLECharacteristic *txCharacteristic;

String comando = "";
String alturaSeleccionada = "";

int totalFotos = 0;
int numeroFotoActual = 0;

bool dispositivoConectado = false;
bool cicloActivo = false;
bool esperandoFoto = false;

// =======================
// CONFIGURACIÓN
// =======================
const int pasosPorVuelta = 200;
const int microstepping = 16;
const float rangoCamaraGrados = 270.0;

const int pasosAlturaMaxima = 500;
const int pasosAlturaMinima = 500;

const int pasosAjusteSeguro = 200;

// =======================
// BLE SEND
// =======================
void enviarBLE(String mensaje) {
  if (dispositivoConectado && txCharacteristic != nullptr) {
    txCharacteristic->setValue(mensaje.c_str());
    txCharacteristic->notify();

    Serial.print("Enviado BLE: ");
    Serial.println(mensaje);
  }
}

// =======================
// CALLBACKS BLE
// =======================
class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    dispositivoConectado = true;
    Serial.println("Celular conectado por BLE");
  }

  void onDisconnect(BLEServer *pServer) {
    dispositivoConectado = false;
    cicloActivo = false;
    esperandoFoto = false;

    Serial.println("Celular desconectado");
    BLEDevice::startAdvertising();
  }
};

class RXCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) {
    String valor = characteristic->getValue().c_str();

    for (int i = 0; i < valor.length(); i++) {
      char c = valor[i];

      if (c == '#') {
        comando.trim();
        if (comando.length() > 0) procesarComando(comando);
        comando = "";
      } else {
        comando += c;
      }
    }
  }
};

// =======================
// SETUP
// =======================
void setup() {
  Serial.begin(115200);

  pinMode(EN_CAMARA, OUTPUT);
  pinMode(STEP_CAMARA, OUTPUT);
  pinMode(DIR_CAMARA, OUTPUT);

  pinMode(EN_ALTURA, OUTPUT);
  pinMode(STEP_ALTURA, OUTPUT);
  pinMode(DIR_ALTURA, OUTPUT);

  pinMode(FINAL_ALTURA, INPUT_PULLUP);

  digitalWrite(EN_CAMARA, LOW);
  digitalWrite(EN_ALTURA, LOW);

  BLEDevice::init("ESP32-S3-DIST");

  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  BLEService *service = server->createService(SERVICE_UUID);

  txCharacteristic = service->createCharacteristic(
    TX_UUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );

  txCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *rxCharacteristic = service->createCharacteristic(
    RX_UUID,
    BLECharacteristic::PROPERTY_WRITE
  );

  rxCharacteristic->setCallbacks(new RXCallbacks());

  service->start();
  BLEDevice::getAdvertising()->addServiceUUID(SERVICE_UUID);
  BLEDevice::startAdvertising();

  Serial.println("BLE iniciado. Esperando conexión...");
}

// =======================
// LOOP
// =======================
void loop() {
  delay(20);
}

// =======================
// MOTOR GENÉRICO
// =======================
void moverMotor(int stepPin, int dirPin, int pasos, bool sentido) {
  digitalWrite(dirPin, sentido ? HIGH : LOW);

  for (int i = 0; i < pasos; i++) {
    if (!cicloActivo) return;

    digitalWrite(stepPin, HIGH);
    delayMicroseconds(1200);
    digitalWrite(stepPin, LOW);
    delayMicroseconds(1200);
  }
}

// =======================
// ALTURA (CORREGIDO)
// =======================
void moverAltura() {

  Serial.println("Verificando altura...");

  // SI YA ESTÁ EN MINIMA
  if (digitalRead(FINAL_ALTURA) == LOW) {
    Serial.println("MINIMA detectada. No se mueve, continuando flujo.");
    return; // SOLO omite movimiento, NO detiene el sistema
  }

  Serial.println("No está en MINIMA. Ajuste seguro...");

  // dirección invertida real
  digitalWrite(DIR_ALTURA, BAJAR);

  for (int i = 0; i < pasosAjusteSeguro; i++) {
    if (!cicloActivo) return;

    digitalWrite(STEP_ALTURA, HIGH);
    delayMicroseconds(1200);
    digitalWrite(STEP_ALTURA, LOW);
    delayMicroseconds(1200);
  }

  Serial.println("Ajuste de altura completado.");
}

// =======================
// PROCESAR COMANDO
// =======================
void procesarComando(String cmd) {

  if (cmd.startsWith("ALT:")) {
    alturaSeleccionada = cmd.substring(4);
  }

  else if (cmd.startsWith("FOTOS:")) {
    totalFotos = cmd.substring(6).toInt();
  }

  else if (cmd == "START") {
    iniciarCiclo();
  }

  else if (cmd == "PHOTO_OK") {
    if (esperandoFoto) esperandoFoto = false;
  }

  else if (cmd == "STOP") {
    cicloActivo = false;
    esperandoFoto = false;
  }
}

// =======================
// CICLO PRINCIPAL
// =======================
void iniciarCiclo() {

  if (totalFotos <= 0) return;

  cicloActivo = true;
  numeroFotoActual = 0;

  enviarBLE("CICLO_INICIADO#");

  moverAltura();

  enviarBLE("ALTURA_OK#");

  int pasosPorFoto =
    (pasosPorVuelta * microstepping) *
    (rangoCamaraGrados / 360.0) / totalFotos;

  for (int i = 1; i <= totalFotos; i++) {

    if (!cicloActivo) return;

    numeroFotoActual = i;

    moverMotor(STEP_CAMARA, DIR_CAMARA, pasosPorFoto, true);

    enviarBLE("PHOTO_NOW#");

    esperandoFoto = true;

    while (esperandoFoto && cicloActivo) {
      delay(10);
    }
  }

  cicloActivo = false;
  enviarBLE("FIN#");
}