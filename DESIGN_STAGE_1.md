# Design Stage 1: The "Clock Pulse" Proof-of-Concept

## Rationale
Stage 1 was the initial exploration of the "Shout & Whisper" concept. The goal was to prove that an Android phone could detect a dual-power BLE signal and perform a secure data operation without a standard persistent connection.

---

## 1. Core Logic: XOR Uptime Sync
Instead of a complex GATT tunnel, Stage 1 used simple XOR arithmetic to validate proximity.

*   **The Shared Secret:** Time.
*   **The Handshake:**
    1.  **XIAO SHOUT (+18dBm):** Broadcasts `Encrypted_Time = (Current_Uptime ^ Secret_Key)`.
    2.  **XIAO WHISPER (-21dBm):** Broadcasts `Secret_Key`.
    3.  **Android App:** Receives both packets. If `(Encrypted_Time ^ Secret_Key)` results in a valid multiple of 10 seconds, the app considers the proximity "verified."

---

## 2. Technical Discoveries

### A. The "Heartbeat" Discovery
*   Initially, the XIAO alternated every 2 seconds. This was too slow for a "snappy" user experience. 
*   **Lesson:** The phone needs a long "Shout" (1s) to wake the OS, but the "Whisper" can be fast (200ms) once the app is already listening.

### B. Advertising Overhead
*   We discovered that calling `advertising->stop()` and `start()` every few hundred milliseconds creates significant "dead air" where no signal is transmitted.
*   **Lesson:** Use on-the-fly payload swapping without stopping the BLE stack to ensure 100% airtime.

### C. Android 14 Background Limits
*   This stage proved that `PendingIntent` background scans are the only reliable way to wake up an app from a "dead" state without user interaction.
*   **Lesson:** Apps must use `startForegroundService` immediately upon wake-up to satisfy Android 14's strict background execution rules.

---

## 3. Data Structure (PoC Legacy)
*   **Manufacturer ID:** 0xFFFF (Mock)
*   **Payload (6 Bytes):**
    *   `[0]` Type (0x01 = Shout, 0x02 = Whisper)
    *   `[1-2]` Session ID (16-bit)
    *   `[3-5]` Data (24-bit XOR result or Key)

---

## 4. Conclusion
Stage 1 successfully validated the physics of the physical firewall. It proved that a -21dBm signal is enough to trigger a sync within inches while remaining mostly invisible at range. This success paved the way for the 184-bit Token and GATT Tunnel of Stage 2.
