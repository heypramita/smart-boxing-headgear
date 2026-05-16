#include <Wire.h>
#include <I2Cdev.h>
#include <MPU6050.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ================= BLE UUIDs =================
#define SERVICE_UUID        "12345678-1234-1234-1234-1234567890ab"
#define CHARACTERISTIC_UUID "abcd1234-5678-90ab-cdef-1234567890ab"

BLECharacteristic *pCharacteristic;

// ================= IMUs =================
MPU6050 imuFront(0x68);   // AD0 -> GND
MPU6050 imuSide(0x69);    // AD0 -> 3.3V

// ================= PINS =================
#define BUZZER_PIN 25
int piezoPins[2] = {32, 33};   // ONLY 2 PIEZOS

// ================= THRESHOLDS =================
#define PIEZO_THRESHOLD 120
#define ACC_THRESHOLD   1.3

// ================= IMPACT COUNTER =================
int impactCount = 0;
unsigned long lastImpactTime = 0;
const unsigned long COOLDOWN = 1000; // ms

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("Starting Smart Headgear BLE...");

  // I2C
  Wire.begin(21, 22);

  // Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // IMUs
  imuFront.initialize();
  imuSide.initialize();

  // BLE INIT
  BLEDevice::init("Smart_Headgear");
  BLEDevice::setMTU(185);

  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();

  BLEDevice::getAdvertising()->start();
  Serial.println("BLE advertising started");
}

// ================= FUNCTION =================
float readAcceleration(MPU6050 &imu) {
  int16_t ax, ay, az;
  imu.getAcceleration(&ax, &ay, &az);

  float x = ax / 16384.0;
  float y = ay / 16384.0;
  float z = az / 16384.0;

  return sqrt(x * x + y * y + z * z);
}

// ================= LOOP =================
void loop() {

  bool piezoImpact = false;
  bool imuImpact   = false;

  String data = "";

  // -------- PIEZO READINGS --------
  data += "Piezos: ";
  for (int i = 0; i < 2; i++) {
    int val = analogRead(piezoPins[i]);
    data += String(val) + " ";
    if (val > PIEZO_THRESHOLD) piezoImpact = true;
  }

  // -------- IMU READINGS --------
  float accFront = readAcceleration(imuFront);
  float accSide  = readAcceleration(imuSide);

  data += "| Front IMU: " + String(accFront, 2) + " g ";
  data += "| Side IMU: "  + String(accSide, 2)  + " g ";

  if (accFront > ACC_THRESHOLD || accSide > ACC_THRESHOLD)
    imuImpact = true;

  // -------- IMPACT LOGIC --------
  unsigned long now = millis();

  if ((piezoImpact || imuImpact) && (now - lastImpactTime > COOLDOWN)) {
    impactCount++;
    lastImpactTime = now;

    data += "| ⚠ IMPACT DETECTED ⚠ Count: " + String(impactCount);

    digitalWrite(BUZZER_PIN, HIGH);
    delay(200);
    digitalWrite(BUZZER_PIN, LOW);
  } else {
    data += "| Impact: Normal";
  }

  // -------- SEND BLE DATA --------
  pCharacteristic->setValue(data.c_str());
  pCharacteristic->notify();

  Serial.println(data);
  delay(500);
}
