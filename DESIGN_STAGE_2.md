# Design Stage 2: Token & Tunnel Architecture (Refined)

## Rationale
Stage 2 implements a secure, high-speed proximity handshake ("Token & Tunnel") designed for high-traffic payment environments. It uses Service Data UUID filtering and a state-based mutual exclusion strategy to achieve sub-second verification.

### Key Architectural Decisions:
1.  **Service UUID Filtering:** Uses the unique 128-bit Service Data AD record (`0x21`) for discovery. This is more unique and professional than using the reserved `0xFFFF` Manufacturer ID.
2.  **72-bit Entropy:** Reclaims 10 bytes in the advertisement. 1 byte for `Sequence` (0-255) and 9 bytes for a high-entropy one-time token. Secure for the 10-second rotation window.
3.  **Fast Recovery (Early Exit):** The XIAO firmware monitors the GATT state. The moment a sync is successful, it kills the remaining Interaction window and skips the Discovery solid-shout to rotate the token and resume whispering immediately.
4.  **Token-Based Mutual Exclusion:** The Android app tracks the `lastSyncedToken`. It ignores any Whisper packet matching the previous success, ensuring a strict 1-to-1 mapping of physical bumps to transactions.
5.  **Hardware-Level Proximity:** Proximity is enforced by Physics (Antenna removal + -12dBm power). The phone must be within ~10cm to capture the Whisper token.

---

## 1. Technical Lessons Learned

### A. Radio Fragility & CRC
*   **Discovery:** Bluetooth hardware silently discards packets failing CRC. 
*   **Optimization:** A 20ms pulse rate ensures that even if 50% of packets are lost due to physical movement, the phone captures a valid token within 40-60ms.
*   **Power Level:** While -24dBm is achievable, -12dBm was found to be the "sweet spot" for stability when the XIAO has no IPEX antenna.

### B. Advertisement Data Integrity
*   **Discovery:** The Arduino `String` class treats `0x00` as a null terminator, truncating advertisements containing binary tokens or null-filled "Shout" packets.
*   **Fix:** Use `oData.addData(char*, size_t)` to bypass `String` constructors and inject raw length-prefixed binary records.

### C. Handshake Speed
*   **Result:** Moving to "Early Exit" logic reduced the back-to-back sync delay from 10 seconds to **~2 seconds**.

### D. GATT State Synchronization
*   **Discovery:** A race condition was identified between the Android "Handshake Timeout" watchdog and the GATT "Success" callback. This could leave `activeGatt` in a non-null state, permanently blocking future syncs.
*   **Fix:** Implemented `synchronized(this)` blocks around all `activeGatt` access and centralized cleanup into a `cleanupGatt()` method to ensure atomic state resets.

### F. Android BLE Deduplication Cache (The "Token-Derived UUID" Architecture)
*   **Discovery:** Even with the scanner left running, Android caches Advertisement payloads per MAC Address. It often ignores updated payloads if the AD Type and Service UUID remain identical. This caused the phone to endlessly match the old token even after the XIAO rotated it.
*   **Fix:** We moved to a **Token-Derived UUID** architecture. The first 6 bytes of our one-time token are injected directly into the end of our Service UUID. Android hardware filters natively match the first 10 bytes (our app's base UUID), while the last 6 bytes change whenever the token rotates. This perfectly defeats the OS cache because every new token creates a "new" Service UUID.

---

## 2. Packet Structure (Token-Derived UUID Record)
To fit within the strict 31-byte BLE limit, the packet is divided as follows:
*   **Token Size:** 11 bytes (88-bit entropy).

| Segment | Size | Description |
| :--- | :--- | :--- |
| **Flags** | 3 Bytes | `0x06` (LE General Discoverable) |
| **Service UUID** | 18 Bytes | `19ed3841-6934-43cb-8d79-` + `[Token Bytes 0-5]` |
| **Manufacturer Data** | 10 Bytes | `0xFFFF` + `[Sequence (1)]` + `[Token Bytes 6-10]` |

Total AD Size: Exactly 31 Bytes.

---

## 3. Final Refined Flow
1.  **XIAO** whispers an 88-bit token every 20ms at -12dBm.
2.  **Phone** captures the Whisper token at proximity via `ScanFilter`.
3.  **Phone** connects via GATT and writes Token to `AUTH_CHAR`.
4.  **XIAO** verifies Token, prepares `DATA_CHAR`, and prepares for rotation.
5.  **Phone** reads `DATA_CHAR` and disconnects.
6.  **XIAO** detects success, **breaks the interaction loop**, rotates the token, and resumes whispering immediately.
