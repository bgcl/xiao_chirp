#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// Built-in LED for XIAO ESP32-S3 Sense
const int LED_PIN = 21;

// BLE Advertisement Settings
#define CHIRP_DURATION_MS 500   // How long to stay visible
#define SILENCE_DURATION_MS 5000 // Time between chirps

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // LED is active LOW on many XIAO boards, but GEMINI.md says GPIO 21. Let's assume standard HIGH = ON for now, or check typical Seeed behavior. 
  // Actually, on Seeed XIAO ESP32S3, the LED is often active LOW. Let's use digitalWrite(LED_PIN, LOW) to turn it ON if that's the case. 
  // Project context says: "The built-in LED on the XIAO ESP32-S3 Sense is connected to GPIO 21."
  
  Serial.println("Initializing BLE...");
  BLEDevice::init("XIAO-CHIRP");
}

void loop() {
  Serial.println("Chirping...");
  
  // Turn LED ON (assuming active LOW for Seeed boards)
  digitalWrite(LED_PIN, LOW); 
  
  // Start Advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(BLEUUID((uint16_t)0x180D)); // Heart Rate service as a dummy minimal UUID
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  delay(CHIRP_DURATION_MS);

  // Stop Advertising
  BLEDevice::stopAdvertising();
  
  // Turn LED OFF
  digitalWrite(LED_PIN, HIGH);

  Serial.println("Sleeping...");
  delay(SILENCE_DURATION_MS);
}
