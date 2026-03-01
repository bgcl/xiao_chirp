# Design Stage 2: Token & Tunnel Architecture (Refined)

## Rationale
Stage 2 optimizes for "Millions of Users" production requirements: infinite payload support, elite cryptographic security, and empirical radio diagnostics. It introduces a physical firewall backed by a 184-bit "One-Time Secret" and high-speed GATT interaction.

### Key Architectural Decisions:
1.  **Entropy Maximization:** Reclaims every bit of the standard 31-byte BLE advertisement. Uses a 24-byte payload providing 184 bits of security entropy (Bytes 1-23).
2.  **Implicit Typing:** Bytes 1-23 are zeros for "Shout" and non-zero for "Whisper."
3.  **Asymmetric Pulsing:** To ensure no phone is left "asleep," the interaction phase alternates between Shout and Whisper, providing multiple wake-up opportunities even if the initial header is missed.
4.  **Immediate Rotation:** The 184-bit token is rotated immediately upon any failed GATT authentication attempt, neutralizing potential brute-force or "guessing" attacks.

---

## 1. Packet Structure (Legacy BLE Advertisement - 31 Bytes)

| Segment | Size | Description |
| :--- | :--- | :--- |
| **BLE Mandatory** | 3 Bytes | Standard Flags and Length headers. |
| **Manufacturer Header** | 4 Bytes | Length, Type (0xFF), Company ID (0xFFFF). |
| **Stage 2 Payload** | **24 Bytes** | Reclaimed space for Token and Diagnostics. |

### Payload Data Breakdown (24 Bytes)

#### A. The SHOUT (Discovery)
*   **Power:** +18dBm (High Range)
*   **Structure:** `[0x00] (Byte 0) + [23 Bytes of Zeros]`

#### B. The WHISPER (Security & Sync)
*   **Power:** -21dBm to -24dBm (Ultra-low Range / Physical Firewall)
*   **Structure:** `[Sequence 1-255] (Byte 0) + [184-bit Random Token] (Bytes 1-23)`

---

## 2. The Macro Cycle (10 Seconds)

1.  **Phase 1: Discovery Header (1 second)**
    *   100% Shout duty cycle (+18dBm). 
    *   LED: SOLID ON.
    *   Goal: Force-wake all nearby phones.
2.  **Phase 2: Interaction Window (9 seconds)**
    *   50/50 Pulse: Alternating every 200ms between **Shout** (+18dBm) and **Whisper** (-21dBm).
    *   LED: SOLID OFF.
    *   Goal: Reliable proximity sync for the user while allowing late-arriving phones to still "wake up."

---

## 3. GATT Tunnel & Auth Logic (Stage 3 Ready)

*   **Service UUID:** `19ed3841-6934-43cb-8d79-f1cc9c343434`
*   **Auth Characteristic (Write):** Receives the 184-bit whispered token.
*   **Time Characteristic (Read):** Provides XIAO `millis()` uptime if Auth matches.
*   **Security Enforcement:**
    *   On **Match**: Send data -> Disconnect.
    *   On **Mismatch**: **Rotate Token immediately** -> Disconnect.
    *   On **Timeout**: If no interaction for 60s, rotate token.
