# Design Stage 2: Token & Tunnel Architecture (Consolidated)

## Rationale
Stage 2 implements a secure, high-speed proximity handshake designed for high-traffic environments. It replaces simple beacons with a bidirectional GATT tunnel protected by a 184-bit physical firewall.

### Key Architectural Decisions:
1.  **Entropy Maximization:** Reclaims every bit of the 31-byte BLE advertisement. Uses a 24-byte payload providing 184 bits of security entropy (Bytes 1-23).
2.  **Implicit Typing:** Bytes 1-23 are zeros for "Shout" and non-zero for "Whisper." This eliminates the need for status flags.
3.  **Asymmetric Phase Timing:** 
    *   **Phase 1 (Discovery):** 1s SOLID Shout (+18dBm). Wake-up call.
    *   **Phase 2 (Interaction):** 9s Pulsed (200ms Shout / 200ms Whisper @ -24dBm). 
    *   **Optimization:** Within the 200ms pulses, the radio fires every 20ms. The `Sequence` byte increments with every 20ms burst to provide high-resolution link diagnostics.
4.  **Hardware-Level Proximity:** Proximity is enforced by Physics (Antenna removal + -24dBm power). The phone must be within ~10cm to capture the Whisper token.
5.  **Immediate Security Rotation:** The 184-bit token rotates immediately upon a successful GATT Auth match, ensuring one-time usage.

---

## 1. Technical Lessons Learned

### A. Radio Fragility & CRC
*   **Discovery:** Bluetooth hardware silently discards packets failing CRC. 
*   **Fix:** Implementation of a 1-byte Sequence Counter (0-255) allowed the app to calculate the "Arrival Rate." 
*   **Result:** Even at -24dBm, the link quality was nearly perfect (0-1 drops per sync) when held close.

### B. GATT Timing Stability
*   **Discovery:** Rapidly reading a characteristic immediately after an Auth write acknowledgment can cause GATT stack hangs.
*   **Fix:** Added a 100ms stabilization delay on the Android side and ensured the XIAO sets the return value *before* acknowledging the Auth write.

### C. OS Interaction (Android 14)
*   **Discovery:** Starting multiple background scans via `PendingIntent` without clean stops causes "Intent Storms" and OS-level throttling.
*   **Fix:** Suspend background scanning once the service is in the foreground and use a live `ScanCallback`.

---

## 2. Packet Structure (Legacy BLE Advertisement)

| Segment | Size | Description |
| :--- | :--- | :--- |
| **Header** | 7 Bytes | Flags, AD Type, Company ID (0xFFFF). |
| **Payload** | **24 Bytes** | Reclaimed space. |

### Payload Data Breakdown
*   **SHOUT:** `[0x00] (Seq) + [23 Bytes of Zeros]`
*   **WHISPER:** `[Sequence 1-255] + [184-bit Random Token]`

---

## 3. Handshake Flow (Stage 3 Basis)
1.  **Phone** wakes up on Shout (+18dBm).
2.  **Phone** captures Whisper Token at proximity.
3.  **Phone** connects via GATT and writes Token to `AUTH_CHAR`.
4.  **XIAO** verifies Token, updates `DATA_CHAR`, and prepares for disconnect.
5.  **Phone** reads `DATA_CHAR` (The Payment Payload) and disconnects.
6.  **XIAO** rotates token immediately.
