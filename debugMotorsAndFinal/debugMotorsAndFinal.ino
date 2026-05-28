// ==========================
// ROTACION
// ==========================

#define EN_ROT 15
#define STEP_ROT 16
#define DIR_ROT 17

// ==========================
// ALTURA
// ==========================

#define EN_ALT 5
#define STEP_ALT 6
#define DIR_ALT 7

// ==========================
// FINALES DE CARRERA
// ==========================

#define FINAL_ALTURA 10
#define FINAL_ROTACION 11

// ==========================
// DIRECCIONES
// ==========================

// ALTURA (invertidas)
#define SUBIR LOW
#define BAJAR HIGH

// ROTACION
#define ROTAR_BUSQUEDA HIGH

// ==========================
// FUNCION MOTOR
// ==========================

void moverMotor(int stepPin, int velocidadMicro) {

  digitalWrite(stepPin, HIGH);
  delayMicroseconds(velocidadMicro);

  digitalWrite(stepPin, LOW);
  delayMicroseconds(velocidadMicro);
}

// ==========================
// SETUP
// ==========================

void setup() {

  Serial.begin(115200);

  // ==========================
  // CONFIGURAR PINES
  // ==========================

  pinMode(EN_ROT, OUTPUT);
  pinMode(STEP_ROT, OUTPUT);
  pinMode(DIR_ROT, OUTPUT);

  pinMode(EN_ALT, OUTPUT);
  pinMode(STEP_ALT, OUTPUT);
  pinMode(DIR_ALT, OUTPUT);

  pinMode(FINAL_ALTURA, INPUT);
  pinMode(FINAL_ROTACION, INPUT);

  // ==========================
  // HABILITAR DRIVERS
  // ==========================

  digitalWrite(EN_ROT, LOW);
  digitalWrite(EN_ALT, LOW);

  Serial.println("=================================");
  Serial.println("INICIO PRUEBA");
  Serial.println("=================================");

  // =====================================
  // ALTURA
  // =====================================

  Serial.println("Configurando altura");

  // -------------------------------------
  // SI EL FINAL ESTA PRESIONADO
  // -------------------------------------

  if(digitalRead(FINAL_ALTURA) == LOW) {

    Serial.println("Final altura presionado");

    // subir un poco
    digitalWrite(DIR_ALT, BAJAR);

    // mover MUY poco
    for(int i = 0; i < 400; i++) {

      moverMotor(STEP_ALT, 1500);
    }

    Serial.println("Altura subida un poco");

    // deshabilitar motor altura
    digitalWrite(EN_ALT, LOW);
  }

  // -------------------------------------
  // SI NO ESTA PRESIONADO
  // -------------------------------------

  else {

    Serial.println("Buscando final altura");

    digitalWrite(DIR_ALT, SUBIR);

    // bajar hasta detectar final
    while(digitalRead(FINAL_ALTURA) == HIGH) {

      moverMotor(STEP_ALT, 1500);
    }

    Serial.println("Final altura encontrado");

    // deshabilitar motor altura
    digitalWrite(EN_ALT, HIGH);
  }

  delay(1000);

  // =====================================
  // ROTACION
  // =====================================

  Serial.println("Buscando final rotacion");

  digitalWrite(DIR_ROT, ROTAR_BUSQUEDA);

  while(digitalRead(FINAL_ROTACION) == HIGH) {

    moverMotor(STEP_ROT, 4000);
  }

  Serial.println("Final rotacion encontrado");

  // deshabilitar motor rotacion
  digitalWrite(EN_ROT, HIGH);

  Serial.println("=================================");
  Serial.println("FIN PRUEBA");
  Serial.println("=================================");
}

void loop() {

}