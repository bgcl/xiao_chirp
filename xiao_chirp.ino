#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Configuration
#define SERVICE_UUID        "19ed3841-6934-43cb-8d79-000000000000" // Base UUID
#define AUTH_CHAR_UUID      "19ed3842-6934-43cb-8d79-f1cc9c343434"
#define TIME_CHAR_UUID      "19ed3843-6934-43cb-8d79-f1cc9c343434"

#define SHOUT_PWR           ESP_PWR_LVL_P18
#define WHISPER_PWR         ESP_PWR_LVL_N12
#define DISCOVERY_MS        1000
#define INTERACTION_MS      9000
#define PULSE_MS            200

// State
uint8_t current_token[11]; // 88-bit token to fit exactly in 31 bytes
uint8_t current_sequence = 1;
unsigned long session_start = 0;
unsigned long connection_timestamp = 0;
bool deviceConnected = false;
bool rotateTokenNextLoop = false; 
BLECharacteristic *pTimeChar;
BLECharacteristic *pAuthChar;
BLEAdvertising *pAdvertising;
BLEServer *pServerGlobal;

void generate_new_token() {
    for (int i = 0; i < 11; i++) {
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
        
        bool match = (len == 11);
        if (match) {
            for (int i = 0; i < 11; i++) {
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
            rotateTokenNextLoop = true; 
        } else {
            Serial.println("AUTH FAILED: Rotating.");
            generate_new_token();
        }
    }
};

void update_adv_payload(bool isWhisper) {
    BLEAdvertisementData oData;
    oData.setFlags(0x06); // 3 bytes total

    uint8_t seq = current_sequence;
    if (isWhisper) {
        current_sequence++;
        if (current_sequence == 0) current_sequence = 1;
    }

    // Combine both records into a single 28-byte array to avoid fragmentation
    uint8_t fullRecord[28];
    
    // AD Record 1: 128-bit Service UUID (18 bytes)
    fullRecord[0] = 17; // Length
    fullRecord[1] = 0x07; // Type: Complete 128-bit Service UUID
    if (isWhisper) {
        for(int i=0; i<6; i++) fullRecord[2+i] = current_token[5-i]; 
    } else {
        for(int i=0; i<6; i++) fullRecord[2+i] = 0;
    }
    uint8_t base_uuid[10] = {0x79, 0x8d, 0xcb, 0x43, 0x34, 0x69, 0x41, 0x38, 0xed, 0x19};
    memcpy(&fullRecord[8], base_uuid, 10);

    // AD Record 2: Manufacturer Data (10 bytes)
    fullRecord[18] = 9; // Length
    fullRecord[19] = 0xFF; // Type: Manufacturer Data
    fullRecord[20] = 0xFF; // Company ID 0xFFFF
    fullRecord[21] = 0xFF;
    if (isWhisper) {
        fullRecord[22] = seq;
        memcpy(&fullRecord[23], &current_token[6], 5); // 5 bytes left of 11-byte token
    } else {
        memset(&fullRecord[22], 0, 6);
    }
    
    oData.addData((char*)fullRecord, 28);

    // Total AD Size: 3 (flags) + 28 = 31 bytes exactly
    pAdvertising->setAdvertisementData(oData);
}

void setup() {
    Serial.begin(115200);
    pinMode(21, OUTPUT);
    digitalWrite(21, HIGH);

    BLEDevice::init("XIAO_PAY_GATEWAY");
    pServerGlobal = BLEDevice::createServer();
    pServerGlobal->setCallbacks(new MyServerCallbacks());

    // Service matches the base UUID
    BLEService *pService = pServerGlobal->createService(SERVICE_UUID);
    pAuthChar = pService->createCharacteristic(AUTH_CHAR_UUID, BLECharacteristic::PROPERTY_WRITE);
    pAuthChar->setCallbacks(new AuthCallbacks());
    
    pTimeChar = pService->createCharacteristic(TIME_CHAR_UUID, BLECharacteristic::PROPERTY_READ);
    pService->start();

    pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->setMinInterval(0x20);
    pAdvertising->setMaxInterval(0x20);
    pAdvertising->setScanResponse(false);

    generate_new_token();
    pAdvertising->start();
}

void loop() {
    if (millis() - session_start > 10000 || rotateTokenNextLoop) generate_new_token();

    // 1. DISCOVERY HEADER (1s)
    digitalWrite(21, LOW); 
    BLEDevice::setPower(SHOUT_PWR);
    update_adv_payload(false); 
    
    unsigned long discovery_start = millis();
    while (millis() - discovery_start < DISCOVERY_MS) {
        if (rotateTokenNextLoop) break;
        delay(50);
    }
    digitalWrite(21, HIGH);

    // 2. INTERACTION WINDOW (9s)
    unsigned long interact_start = millis();
    while (millis() - interact_start < INTERACTION_MS) {
        if (rotateTokenNextLoop) break;

        if (deviceConnected) { 
            if (millis() - connection_timestamp > 2000) {
                Serial.println("WATCHDOG: Forcing Disconnect.");
                pServerGlobal->disconnect(0); 
                connection_timestamp = millis();
            }
            delay(100); 
            continue; 
        }
        
        BLEDevice::setPower(WHISPER_PWR);
        unsigned long whisper_start = millis();
        while (millis() - whisper_start < PULSE_MS) {
            if (rotateTokenNextLoop || deviceConnected) break;
            update_adv_payload(true); 
            delay(20);
        }

        if (rotateTokenNextLoop || deviceConnected) continue;
        BLEDevice::setPower(SHOUT_PWR);
        unsigned long shout_start = millis();
        while (millis() - shout_start < PULSE_MS) {
            if (rotateTokenNextLoop || deviceConnected) break;
            update_adv_payload(false); 
            delay(20);
        }
    }
}
