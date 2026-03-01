# Design Stage 2: Token & Tunnel Architecture

## Rationale
Stage 1 (XOR Uptime) successfully proved that dual-power proximity synchronization works. Stage 2 optimizes this model for "Millions of Users" production requirements: infinite payload support, elite cryptographic security, and empirical radio diagnostics for antenna-less hardware.

### Key Architectural Decisions:
1.  **Entropy Maximization:** We reclaim every bit of the standard 31-byte BLE advertisement. By using a 24-byte payload, we provide 184 bits of security entropy—physically impossible to brute-force during a transaction window.
2.  **Implicit Typing:** We waste zero bytes on "Status" or "Type" flags. The app distinguishes a "Shout" from a "Whisper" by looking for trailing zeros in the 24-byte block. 
3.  **MAC Independence:** Any merchant's "Shout" wakes the app; any "Whisper" with a valid token triggers the handshake. This ensures speed and versatility in crowded environments.
4.  **Fragility Analytics:** Since the Bluetooth hardware silently discards corrupted packets (CRC failure), we use a 1-byte Sequence Counter to visualize "Radio Stutter" and empirically measure the limits of our antenna-less "Physical Firewall."

---

## 1. Packet Structure (Legacy BLE Advertisement - 31 Bytes)

| Segment | Size | Description |
| :--- | :--- | :--- |
| **BLE Mandatory** | 3 Bytes | Standard Flags and Length headers. |
| **Manufacturer Header** | 4 Bytes | Length, Type (0xFF), Company ID (0xFFFF). |
| **Stage 2 Payload** | **24 Bytes** | Reclaimed space for Token and Diagnostics. |

### Payload Data Breakdown (24 Bytes)

#### A. The SHOUT (Discovery Window)
*   **Power:** +18dBm (High Range)
*   **Purpose:** Wake the Android app and promote it to High-Speed Scanning.
*   **Structure:** `[0x00] (Byte 0) + [23 Bytes of Zeros]`
*   *Note: The zero-block can later be replaced with a public Merchant ID.*

#### B. The WHISPER (Proximity Window)
*   **Power:** -21dBm to -24dBm (Ultra-low Range / Physical Firewall)
*   **Purpose:** Securely deliver the 184-bit "One-Time Secret" token.
*   **Structure:** `[Sequence 1-255] (Byte 0) + [184-bit Random Token] (Bytes 1-23)`

---

## 2. Radio Diagnostic Model: "Sequence Delta"
Because we cannot see "bad" packets, the Android app tracks the `Sequence` byte:
*   **Clean Link:** App sees `10, 11, 12, 13`. (Perfect reception).
*   **Fragile Link:** App sees `10, 15, 22`. 
    *   *Interpretation:* We just lost 11 packets to CRC failure. This proves the signal is at the absolute edge of the phone's sensitivity.
*   **Analytics Goal:** Log the "Arrival Rate" (e.g., "Received 3/10 packets") to help calibrate the final hardware enclosure.

---

## 3. Handshake Logic (The Stage 3 Preview)
The whispered **184-bit Token** is not just a password; it will eventually become the **AES-128 Encryption Key** for the Stage 3 GATT connection. 
1.  Phone captures Whisper Token.
2.  Phone connects via GATT.
3.  XIAO transmits the 100+ byte Payment URI over the encrypted tunnel.
