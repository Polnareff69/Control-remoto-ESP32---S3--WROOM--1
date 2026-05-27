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
#define EN_CAMARA    4
#define STEP_CAMARA  2
#define DIR_CAMARA   1

// =======================
// MOTOR ALTURA
// =======================
#define EN_ALTURA    5
#define STEP_ALTURA  6
#define DIR_ALTURA   7

BLECharacteristic *txCharacteristic;

String comando = "";
String alturaSeleccionada = "";

int totalFotos = 0;
int numeroFotoActual = 0;

bool dispositivoConectado = false;
bool cicloActivo = false;
bool esperandoFoto = false;

// =======================
// CONFIGURACIÓN DE MOTORES
// =======================
const int pasosPorVuelta = 200;
const int microstepping = 16;
const float rangoCamaraGrados = 270.0;

// Prueba inicial de altura
const int pasosAlturaMaxima = 500;
const int pasosAlturaMinima = 500;

// =======================
// ENVIAR MENSAJE A LA APK
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
// CALLBACK CONEXIÓN BLE
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

// =======================
// CALLBACK RECEPCIÓN BLE
// =======================
class RXCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) {
    String valor = characteristic->getValue().c_str();

    for (int i = 0; i < valor.length(); i++) {
      char c = valor[i];

      if (c == '#') {
        comando.trim();

        if (comando.length() > 0) {
          procesarComando(comando);
        }

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

  // La mayoría de drivers A4988/DRV8825 tienen ENABLE activo en LOW
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

  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);

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
// PROCESAR COMANDOS
// =======================
void procesarComando(String cmd) {
  Serial.print("Comando recibido: ");
  Serial.println(cmd);

  if (cmd.startsWith("ALT:")) {
    alturaSeleccionada = cmd.substring(4);

    Serial.print("Altura seleccionada: ");
    Serial.println(alturaSeleccionada);

    enviarBLE("ALTURA_CONFIGURADA#");
  }

  else if (cmd.startsWith("FOTOS:")) {
    totalFotos = cmd.substring(6).toInt();

    Serial.print("Total fotos: ");
    Serial.println(totalFotos);

    if (totalFotos <= 0) {
      enviarBLE("ERROR:FOTOS_INVALIDAS#");
    } else {
      enviarBLE("FOTOS_CONFIGURADAS#");
    }
  }

  else if (cmd == "START") {
    iniciarCiclo();
  }

  else if (cmd == "PHOTO_OK") {
    if (esperandoFoto) {
      esperandoFoto = false;

      Serial.print("Foto confirmada: ");
      Serial.println(numeroFotoActual);
    }
  }

  else if (cmd == "STOP") {
    cicloActivo = false;
    esperandoFoto = false;

    enviarBLE("FIN#");
  }

  else {
    enviarBLE("ERROR:COMANDO_DESCONOCIDO#");
  }
}

// =======================
// INICIAR CICLO
// =======================
void iniciarCiclo() {
  if (alturaSeleccionada == "") {
    enviarBLE("ERROR:ALTURA_NO_CONFIGURADA#");
    return;
  }

  if (totalFotos <= 0) {
    enviarBLE("ERROR:FOTOS_NO_CONFIGURADAS#");
    return;
  }

  cicloActivo = true;
  esperandoFoto = false;
  numeroFotoActual = 0;

  enviarBLE("CICLO_INICIADO#");

  moverAltura();

  enviarBLE("ALTURA_OK#");

  int pasosPorFoto = calcularPasosCamara();

  Serial.print("Pasos por foto cámara: ");
  Serial.println(pasosPorFoto);

  for (int i = 1; i <= totalFotos; i++) {
    if (!cicloActivo) {
      enviarBLE("FIN#");
      return;
    }

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

// =======================
// MOVER ALTURA
// =======================
void moverAltura() {
  if (alturaSeleccionada == "MAXIMA") {
    Serial.println("Moviendo altura máxima 500 pasos");
    moverMotor(STEP_ALTURA, DIR_ALTURA, pasosAlturaMaxima, true);
  }

  else if (alturaSeleccionada == "MINIMA") {
    Serial.println("Moviendo altura mínima 500 pasos");
    moverMotor(STEP_ALTURA, DIR_ALTURA, pasosAlturaMinima, false);
  }

  else {
    enviarBLE("ERROR:ALTURA_INVALIDA#");
  }
}

// =======================
// CALCULAR PASOS CÁMARA
// =======================
int calcularPasosCamara() {
  int pasosTotales = pasosPorVuelta * microstepping;
  float pasos270 = pasosTotales * (rangoCamaraGrados / 360.0);

  return round(pasos270 / totalFotos);
}

// =======================
// MOVER MOTOR GENÉRICO
// =======================
void moverMotor(int stepPin, int dirPin, int pasos, bool sentidoHorario) {
  digitalWrite(dirPin, sentidoHorario ? HIGH : LOW);

  for (int i = 0; i < pasos; i++) {
    if (!cicloActivo) return;

    digitalWrite(stepPin, HIGH);
    delayMicroseconds(1200);

    digitalWrite(stepPin, LOW);
    delayMicroseconds(1200);
  }
}