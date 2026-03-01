#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Configuration
#define SERVICE_UUID        "19ed3841-6934-43cb-8d79-f1cc9c343434"
#define AUTH_CHAR_UUID      "19ed3842-6934-43cb-8d79-f1cc9c343434"
#define TIME_CHAR_UUID      "19ed3843-6934-43cb-8d79-f1cc9c343434"

#define SHOUT_PWR           ESP_PWR_LVL_P18
#define WHISPER_PWR         ESP_PWR_LVL_N21
#define DISCOVERY_MS        1000
#define INTERACTION_MS      9000
#define PULSE_MS            200

// State
uint8_t current_token[23];
uint8_t current_sequence = 1;
unsigned long session_start = 0;
unsigned long connection_timestamp = 0; // Watchdog timer
bool deviceConnected = false;
bool rotateTokenNextLoop = false; 
BLECharacteristic *pTimeChar;
BLECharacteristic *pAuthChar;
BLEAdvertising *pAdvertising;
BLEServer *pServerGlobal;

void generate_new_token() {
    for (int i = 0; i < 23; i++) {
        current_token[i] = (uint8_t)random(1, 256); 
    }
    current_sequence = 1;
    session_start = millis();
    rotateTokenNextLoop = false; 
    Serial.print("NEW TOKEN: ");
    for(int i=0; i<4; i++) Serial.printf("%02X", current_token[i]);
    Serial.println("...");
}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) { 
        deviceConnected = true; 
        connection_timestamp = millis(); 
    }
    void onDisconnect(BLEServer* pServer) { 
        deviceConnected = false;
        pServer->getAdvertising()->start();
    }
};

class AuthCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        uint8_t* pData = pCharacteristic->getData();
        size_t len = pCharacteristic->getLength();
        
        bool match = (len == 23);
        if (match) {
            for (int i = 0; i < 23; i++) {
                if (pData[i] != current_token[i]) {
                    match = false;
                    break;
                }
            }
        }

        if (match) {
            Serial.println("AUTH SUCCESS");
            uint32_t uptime = millis();
            pTimeChar->setValue((uint8_t*)&uptime, 4);
            rotateTokenNextLoop = true; // Mark for immediate rotation
        } else {
            Serial.println("AUTH FAILED: Rotating.");
            generate_new_token();
        }
        // Small delay to let read happen before we disconnect manually in next logic
    }
};

void update_adv_payload(bool isWhisper) {
    uint8_t payload[28]; 
    payload[0] = 0x1B; 
    payload[1] = 0xFF; 
    payload[2] = 0xFF; 
    payload[3] = 0xFF; 
    
    if (!isWhisper) {
        payload[4] = 0x00; 
        memset(&payload[5], 0, 23); 
    } else {
        payload[4] = current_sequence++;
        if (current_sequence == 0) current_sequence = 1; 
        memcpy(&payload[5], current_token, 23);
    }

    BLEAdvertisementData oData;
    oData.setFlags(0x06);
    oData.addData((char*)payload, 28);
    pAdvertising->setAdvertisementData(oData);
}

void setup() {
    Serial.begin(115200);
    pinMode(21, OUTPUT);
    digitalWrite(21, HIGH);

    BLEDevice::init("XIAO_PAY_GATEWAY");
    pServerGlobal = BLEDevice::createServer();
    pServerGlobal->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServerGlobal->createService(SERVICE_UUID);
    pAuthChar = pService->createCharacteristic(AUTH_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE);
    pAuthChar->setCallbacks(new AuthCallbacks());
    
    pTimeChar = pService->createCharacteristic(TIME_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    pService->start();

    pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setMinInterval(0x20);
    pAdvertising->setMaxInterval(0x20);

    generate_new_token();
    pAdvertising->start();
}

void loop() {
    if (millis() - session_start > 10000 || rotateTokenNextLoop) generate_new_token();

    // 1. DISCOVERY HEADER (1s)
    digitalWrite(21, LOW); 
    BLEDevice::setPower(SHOUT_PWR);
    update_adv_payload(false); 
    delay(DISCOVERY_MS);
    digitalWrite(21, HIGH);

    // 2. INTERACTION WINDOW (9s)
    unsigned long interact_start = millis();
    while (millis() - interact_start < INTERACTION_MS) {
        if (deviceConnected) { 
            // Server-side Watchdog: Forcibly disconnect after 2 seconds
            if (millis() - connection_timestamp > 2000) {
                Serial.println("WATCHDOG: Forcing Disconnect.");
                pServerGlobal->disconnect(0); // Disconnect client 0
            }
            delay(100); 
            continue; 
        }
        
        if (millis() - session_start > 10000 || rotateTokenNextLoop) generate_new_token();

        // Whisper Pulse - 20ms update rate
        BLEDevice::setPower(WHISPER_PWR);
        unsigned long whisper_start = millis();
        while (millis() - whisper_start < PULSE_MS) {
            update_adv_payload(true); 
            delay(20);
        }

        // Shout Pulse - 20ms update rate
        BLEDevice::setPower(SHOUT_PWR);
        unsigned long shout_start = millis();
        while (millis() - shout_start < PULSE_MS) {
            update_adv_payload(false); 
            delay(20);
        }
    }
}
