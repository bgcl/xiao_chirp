#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <esp_bt.h>

// Built-in LED for XIAO ESP32-S3 Sense
const int LED_PIN = 21;

// Heart Rate Service and Characteristic UUIDs (Standard)
#define SERVICE_UUID           "180D"
#define CHARACTERISTIC_UUID    "2A37"

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;

// ESP32-S3 BLE Power Levels (-24 to +20 dBm)
esp_power_level_t powerLevels[] = {
  ESP_PWR_LVL_N24, ESP_PWR_LVL_N21, ESP_PWR_LVL_N18,
  ESP_PWR_LVL_N15, ESP_PWR_LVL_N12, ESP_PWR_LVL_N9,  ESP_PWR_LVL_N6,
  ESP_PWR_LVL_N3,  ESP_PWR_LVL_N0,  ESP_PWR_LVL_P3,  ESP_PWR_LVL_P6,
  ESP_PWR_LVL_P9,  ESP_PWR_LVL_P12, ESP_PWR_LVL_P15, ESP_PWR_LVL_P18,
  ESP_PWR_LVL_P20
};

int powerLevelStrings[] = {
  -24, -21, -18, -15, -12, -9, -6, -3, 0, 3, 6, 9, 12, 15, 18, 20
};

int currentLevelIndex = 0;
const int numLevels = 16;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      Serial.println("Device connected");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("Device disconnected. Restarting advertising...");
      pServer->getAdvertising()->start();
    }
};

void setup() {
  Serial.begin(115200);
  // Wait up to 3 seconds for Serial Monitor to connect
  unsigned long startWait = millis();
  while (!Serial && millis() - startWait < 3000) {
    delay(10);
  }

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // LED OFF (Active LOW)
  
  Serial.println("--- XIAO ESP32-S3 Attenuation Test Starting ---");

  // Initialize BLE
  BLEDevice::init("XIAO");
  Serial.print("BLE Device Initialized. Address: ");
  Serial.println(BLEDevice::getAddress().toString().c_str());
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create Custom Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create Custom Characteristic
  pCharacteristic = pService->createCharacteristic(
                                CHARACTERISTIC_UUID,
                                BLECharacteristic::PROPERTY_NOTIFY
                              );
  
  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();

  // Start Advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  
  BLEAdvertisementData oAdvertisementData;
  oAdvertisementData.setFlags(0x06); // General Discoverable Mode & BR/EDR Not Supported
  oAdvertisementData.setCompleteServices(BLEUUID((uint16_t)0x180D));
  oAdvertisementData.setServiceData(BLEUUID((uint16_t)0x180D), String("\x01")); 
  pAdvertising->setAdvertisementData(oAdvertisementData);
  
  BLEAdvertisementData oScanResponseData;
  oScanResponseData.setName("XIAO");
  pAdvertising->setScanResponseData(oScanResponseData);
  
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  
  BLEDevice::startAdvertising();
  Serial.println("BLE Attenuation Test Initialized.");
  Serial.println("Cycling through 16 power levels (1 level per second).");
}

void loop() {
  // Select the current power level
  esp_power_level_t power = powerLevels[currentLevelIndex];
  int dbm = powerLevelStrings[currentLevelIndex];

  // Apply Transmit Power for both Advertising and active Connections
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, power);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, power);
  
  // Output status to Serial
  Serial.print("[");
  Serial.print(currentLevelIndex + 1);
  Serial.print("/");
  Serial.print(numLevels);
  Serial.print("] TX Power: ");
  Serial.print(dbm);
  Serial.println(" dBm");

  // Indicate level change with LED pulse
  digitalWrite(LED_PIN, LOW); // ON
  
  // Update Heart Rate Measurement value
  // Reporting: BPM = dBm + 50 (e.g. -24 dBm -> 26 BPM, +20 dBm -> 70 BPM)
  uint8_t hrData[2];
  hrData[0] = 0x00; 
  hrData[1] = (uint8_t)(dbm + 50); 
  
  pCharacteristic->setValue(hrData, 2);
  pCharacteristic->notify();

  delay(100);
  digitalWrite(LED_PIN, HIGH); // OFF

  // Complete the 5-second interval
  delay(4900);

  // Advance to next level
  currentLevelIndex = (currentLevelIndex + 1) % numLevels;
}
