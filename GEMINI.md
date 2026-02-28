# XIAO Chirp: "Shout & Whisper" Architecture

This document defines the firmware architecture for the XIAO ESP32-S3 payment gateway, specializing in high-speed, secure QR-to-BLE transmission via physical proximity (The "Bump").

## The Vision
Eliminate the manual "Open App -> Point Camera -> Scan QR" flow. The XIAO device scans the register's QR code and transmits it directly to the user's phone via a secure, physics-based proximity handshake.

---

## 1. Dual-Beacon Synchronization

The system uses signal attenuation (the inverse-square law) as a security layer.

### Phase A: The Shout (Discovery & Warm-up)
*   **Purpose:** Wake the Android app from deep sleep and elevate it to a Foreground Service.
*   **Power Level:** **+20dBm** (Maximum range).
*   **Payload:** `[Status: 0x01 (Shout), SessionID: XXXX, Data: Encrypted_QR_Blob]`
*   **Behavior:** Broadcasts continuously while a valid QR code is visible to the XIAO Sense camera.

### Phase B: The Whisper (Intent & Proximity)
*   **Purpose:** Provide the decryption key only to the phone physically touching the device.
*   **Trigger:** Hardware interrupt from a vibration switch (The "Bump").
*   **Power Level:** **-21dBm to -24dBm** (Ultra-low power, ~10cm range).
*   **Payload:** `[Status: 0x02 (Whisper), SessionID: XXXX, Data: Decryption_Key]`
*   **Behavior:** Broadcasts for a brief 2-second "burst" immediately following a physical bump.

---

## 2. Security Model: "Physics as a Firewall"
By broadcasting the `Decryption_Key` at -21dBm, we ensure that:
1.  **Isolation:** Only the phone in the "Whisper Zone" (touching the device) can receive the key.
2.  **Anti-Eavesdropping:** A malicious phone 2 meters away can "hear" the encrypted Shout but lacks the signal sensitivity to "hear" the low-power Whisper.
3.  **Atomic Verification:** The Android app only triggers if it receives **Data (Whisper)** + **Physics (Accelerometer Spike)** simultaneously.

---

## 3. Implementation Roadmap

### XIAO Sense Firmware (C++)
*   **Camera Task:** Constant polling of the register screen using the `ESP32 Camera` and `QR Recognition` libraries.
*   **BLE Task:**
    *   Dynamic Manufacturer Data swapping.
    *   Fast Power Switching: Using `esp_ble_tx_power_set()` to transition between +20dBm and -21dBm.
*   **Interrupt Task:** GPIO interrupt handler for the vibration switch to trigger the 2-second Whisper burst.

### Android Service (Kotlin)
*   **Persistent Scan:** Piggyback on the existing system-level BLE scan.
*   **Packet Parser:** Extracting the `Status` and `SessionID` from the `ManufacturerSpecificData` byte array.
*   **Sync Logic:** Matching the `SessionID` between a previous Shout and a new Whisper to decrypt and launch the banking URI.

---

## 4. Hardware Configuration
*   **XIAO ESP32-S3 Sense:** Required for the onboard camera.
*   **Vibration Switch:** Connected to a GPIO with internal pull-up/pull-down.
*   **Status LED (GPIO 21):** 
    *   Slow Pulse: Searching for QR.
    *   Solid: QR Locked (Shouting).
    *   Rapid Flash: Whisper Burst Active.
