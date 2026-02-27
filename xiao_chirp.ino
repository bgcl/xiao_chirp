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
  
  // Set Advertising Interval (100ms for fast detection)
  pAdvertising->setMinInterval(0x64); // 100ms / 0.625ms = 160 (0xA0)
  pAdvertising->setMaxInterval(0xA0); 
  
  BLEDevice::startAdvertising();
  Serial.println("BLE Chirp Active with 128-bit UUID.");
}

void loop() {
  // We can just pulse the LED occasionally to show it's alive
  if (!deviceConnected) {
    digitalWrite(LED_PIN, LOW); // ON
    delay(50);
    digitalWrite(LED_PIN, HIGH); // OFF
    delay(1950);
  } else {
    // Solid light if connected (optional)
    digitalWrite(LED_PIN, LOW);
    delay(100);
  }
}

