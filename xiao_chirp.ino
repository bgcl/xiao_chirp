#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

// Configuration
#define SHOUT_PWR ESP_PWR_LVL_P18    // +18dBm (S3 Max is ~18-20)
#define WHISPER_PWR ESP_PWR_LVL_N24  // -24dBm (Lowest possible)
#define SESSION_INTERVAL_MS 10000    // Refresh data every 10s
#define DISCOVERY_WINDOW_MS 1000     // Solid shout for 1s
#define PULSE_WINDOW_MS 200          // Pulse shout/whisper for 200ms

// BLE State
BLEAdvertising *pAdvertising;

// Application State
uint16_t current_session_id = 0;
uint32_t secret_key = 0;
uint32_t encrypted_time = 0;
unsigned long last_session_refresh = 0;

void update_session_data() {
    current_session_id = (uint16_t)random(1, 65535);
    secret_key = (uint32_t)random(1, 0xFFFFFF); // 24-bit key
    
    // Use actual uptime in seconds, rounded down to nearest 10
    uint32_t uptime_sec = millis() / 1000;
    uint32_t current_time_rounded = (uptime_sec / 10) * 10;
    
    encrypted_time = current_time_rounded ^ secret_key;
    
    last_session_refresh = millis();
    Serial.printf("New Session: %04X | Time: %u | Key: %06X | Enc: %06X\n", 
                  current_session_id, current_time_rounded, secret_key, encrypted_time);
}

void set_adv_payload(uint8_t type, uint16_t session, uint32_t data_val) {
    pAdvertising->stop();
    
    // Total 10 bytes: 1 Len + 1 Type (0xFF) + 2 CompanyID + 1 Status + 2 Session + 3 Data
    uint8_t payload[10];
    payload[0] = 0x09; // Length (Type + ID + Status + Session + Data)
    payload[1] = 0xFF; // Manufacturer Specific Data type
    payload[2] = 0xFF; // Company ID Lo
    payload[3] = 0xFF; // Company ID Hi
    
    payload[4] = type; // 0x01 Shout, 0x02 Whisper
    payload[5] = (session >> 8) & 0xFF;
    payload[6] = session & 0xFF;
    payload[7] = (data_val >> 16) & 0xFF;
    payload[8] = (data_val >> 8) & 0xFF;
    payload[9] = data_val & 0xFF;

    BLEAdvertisementData oData;
    oData.setFlags(0x06);
    oData.addData((char*)payload, 10);
    pAdvertising->setAdvertisementData(oData);
    
    pAdvertising->start();
}

void setup() {
    Serial.begin(115200);
    pinMode(21, OUTPUT); // Built-in LED
    digitalWrite(21, HIGH); // Active LOW off

    BLEDevice::init("XIAO_CHIRP_POC");
    pAdvertising = BLEDevice::getAdvertising();
    
    update_session_data();
}

void loop() {
    if (millis() - last_session_refresh > SESSION_INTERVAL_MS) {
        update_session_data();
    }

    // 2. DISCOVERY PHASE: Solid Shout
    Serial.println(">>> Discovery: SHOUTING");
    digitalWrite(21, LOW); // LED ON (Active LOW)
    
    BLEDevice::setPower(SHOUT_PWR);
    set_adv_payload(0x01, current_session_id, encrypted_time);
    
    delay(DISCOVERY_WINDOW_MS);
    digitalWrite(21, HIGH); // LED OFF

    // 3. INTERACTION PHASE: Pulse Loop
    Serial.println(">>> Interaction: PULSING");
    unsigned long interaction_start = millis();
    while (millis() - interaction_start < 4000) {
        if (millis() - last_session_refresh > SESSION_INTERVAL_MS) {
            update_session_data();
        }

        // Shout Pulse
        BLEDevice::setPower(SHOUT_PWR);
        set_adv_payload(0x01, current_session_id, encrypted_time);
        delay(PULSE_WINDOW_MS);

        // Whisper Pulse
        BLEDevice::setPower(WHISPER_PWR);
        set_adv_payload(0x02, current_session_id, secret_key);
        delay(PULSE_WINDOW_MS);
    }
}
