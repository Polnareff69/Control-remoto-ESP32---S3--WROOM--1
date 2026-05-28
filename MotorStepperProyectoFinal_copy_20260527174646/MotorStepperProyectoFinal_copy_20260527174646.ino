#define STEP_PIN 15
#define DIR_PIN 18
#define EN_PIN 16

void setup() {

  Serial.begin(115200);

  pinMode(STEP_PIN, OUTPUT);
  pinMode(DIR_PIN, OUTPUT);
  pinMode(EN_PIN, OUTPUT);

  // direccion
  digitalWrite(DIR_PIN, HIGH);

  // prueba ENABLE LOW
  digitalWrite(EN_PIN, LOW);

  Serial.println("Inicio motor");
}

void loop() {
  Serial.println("Inicio 2motor");
  digitalWrite(STEP_PIN, HIGH);
  delay(20000);

  digitalWrite(STEP_PIN, LOW);
  delay(20000);
}