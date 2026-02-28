#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

// Configuration
#define SHOUT_PWR ESP_PWR_LVL_P18    
#define WHISPER_PWR ESP_PWR_LVL_N21  
#define SESSION_INTERVAL_MS 10000    
#define DISCOVERY_WINDOW_MS 1000     
#define INTERACTION_WINDOW_MS 9000   

// BLE State
BLEAdvertising *pAdvertising;

// Application State
uint16_t current_session_id = 0;
uint32_t secret_key = 0;
uint32_t encrypted_time = 0;
unsigned long last_session_refresh = 0;

void update_session_data() {
    current_session_id = (uint16_t)random(1, 65535);
    secret_key = (uint32_t)random(1, 0xFFFFFF); 
    uint32_t uptime_sec = millis() / 1000;
    uint32_t current_time_rounded = (uptime_sec / 10) * 10;
    encrypted_time = current_time_rounded ^ secret_key;
    last_session_refresh = millis();
    Serial.printf("New Session: %04X | Time: %u | Key: %06X | Enc: %06X\n", 
                  current_session_id, current_time_rounded, secret_key, encrypted_time);
}

void update_adv_payload(uint8_t type, uint16_t session, uint32_t data_val) {
    uint8_t payload[10];
    payload[0] = 0x09; 
    payload[1] = 0xFF; 
    payload[2] = 0xFF; 
    payload[3] = 0xFF; 
    
    payload[4] = type; 
    payload[5] = (session >> 8) & 0xFF;
    payload[6] = session & 0xFF;
    payload[7] = (data_val >> 16) & 0xFF;
    payload[8] = (data_val >> 8) & 0xFF;
    payload[9] = data_val & 0xFF;

    BLEAdvertisementData oData;
    oData.setFlags(0x06);
    oData.addData((char*)payload, 10);
    pAdvertising->setAdvertisementData(oData);
}

void setup() {
    Serial.begin(115200);
    pinMode(21, OUTPUT);
    digitalWrite(21, HIGH);

    BLEDevice::init("XIAO_CHIRP_POC");
    pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->setMinInterval(0x20); 
    pAdvertising->setMaxInterval(0x20);
    
    update_session_data();
    pAdvertising->start();
}

void loop() {
    if (millis() - last_session_refresh > SESSION_INTERVAL_MS) {
        update_session_data();
    }

    // 1. DISCOVERY PHASE: SHOUT the Payload (+18dBm)
    digitalWrite(21, LOW); 
    BLEDevice::setPower(SHOUT_PWR);
    update_adv_payload(0x01, current_session_id, encrypted_time);
    delay(DISCOVERY_WINDOW_MS);
    digitalWrite(21, HIGH); 

    // 2. INTERACTION PHASE: WHISPER the Key (-24dBm)
    // No pulsing - just 100% key airtime during this window
    BLEDevice::setPower(WHISPER_PWR);
    update_adv_payload(0x02, current_session_id, secret_key);
    
    unsigned long interaction_start = millis();
    while (millis() - interaction_start < INTERACTION_WINDOW_MS) {
        if (millis() - last_session_refresh > SESSION_INTERVAL_MS) {
            update_session_data();
            // Refresh the key in the air if the session rolls
            update_adv_payload(0x02, current_session_id, secret_key);
        }
        delay(100); // Wait for session roll or loop end
    }
}
