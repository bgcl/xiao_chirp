#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <esp_bt.h>

// Built-in LED for XIAO ESP32-S3 Sense
const int LED_PIN = 21;

// UNIQUE 128-bit UUID for this application to avoid collisions
// Generated specifically for this checkout-counter app
#define SERVICE_UUID           "7b1d16a1-9f20-410c-974d-77f6b9073167"
#define CHARACTERISTIC_UUID    "7b1d16a2-9f20-410c-974d-77f6b9073167"

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
  
  Serial.println("--- XIAO ESP32-S3 Checkout Chirp Starting ---");

  // Initialize BLE
  BLEDevice::init("XIAO-CHIRP");
  Serial.print("BLE Device Initialized. Address: ");
  Serial.println(BLEDevice::getAddress().toString().c_str());
  
  // Set Transmit Power (P9 = +9 dBm, good for ~10-15m range at checkouts)
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);

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
  oAdvertisementData.setCompleteServices(BLEUUID(SERVICE_UUID));
  
  // Custom Service Data (0x01 = Chirp Active)
  String serviceData = "\x01";
  oAdvertisementData.setServiceData(BLEUUID(SERVICE_UUID), serviceData); 
  pAdvertising->setAdvertisementData(oAdvertisementData);
  
  BLEAdvertisementData oScanResponseData;
  oScanResponseData.setName("XIAO-CHIRP");
  pAdvertising->setScanResponseData(oScanResponseData);
  
  pAdvertising->setScanResponse(true);
  
  // Set Advertising Interval (2000ms for "slow" chirp)
  // 2000ms / 0.625ms = 3200 (0xC80)
  pAdvertising->setMinInterval(0xC80); 
  pAdvertising->setMaxInterval(0xC80); 
  
  BLEDevice::startAdvertising();
  Serial.println("BLE Chirp Active (2s interval) with 128-bit UUID.");
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
  
  // Update Service Data or Value if needed (optional)
  uint8_t chirpData[1] = {0x01};
  pCharacteristic->setValue(chirpData, 1);
  pCharacteristic->notify();

  delay(50);
  digitalWrite(LED_PIN, HIGH); // OFF

  // Complete the 2-second interval
  delay(1950);

  // Advance to next level
  currentLevelIndex = (currentLevelIndex + 1) % numLevels;
}

