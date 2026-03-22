# esp32c6xiao

PlatformIO firmware for the **Seeed Studio XIAO ESP32-C6** featuring:

- **PIR motion detection** using the HW777 passive-infrared sensor
- **ESP-NOW** wireless communication (peer-to-peer, no router required)
- **Ping / heartbeat** system so receivers detect when a sender node goes offline

---

## Repository layout

```
esp32c6xiao/
├── pir_sender/          # Firmware for the sensor node (PIR + ESP-NOW transmitter)
│   ├── platformio.ini
│   ├── include/
│   │   └── common.h     # Shared message structs & constants
│   └── src/
│       └── main.cpp
└── receiver/            # Firmware for the hub / base-station node
    ├── platformio.ini
    ├── include/
    │   └── common.h     # Shared message structs & constants
    └── src/
        └── main.cpp
```

---

## Hardware

### Components (per sensor node)

| Component | Notes |
|-----------|-------|
| Seeed Studio XIAO ESP32-C6 | Main MCU, Wi-Fi / ESP-NOW radio |
| HW777 PIR motion sensor | 3.3 V compatible, digital OUT |

### Wiring – HW777 to XIAO ESP32-C6

| HW777 pin | XIAO pin | Notes |
|-----------|----------|-------|
| VCC | 3V3 | 3.3 V power |
| GND | GND | Ground |
| OUT | D0 (GPIO 2) | Digital signal – HIGH when motion detected |

> **Tip:** If you want to use a different GPIO, change `PIR_PIN` in
> `pir_sender/src/main.cpp`.

---

## How it works

### Sender node (`pir_sender`)

1. Polls the PIR sensor output at ~20 Hz.
2. On a **state change** (motion detected or cleared), broadcasts a
   `MSG_MOTION` packet via ESP-NOW.
3. Every **5 seconds** broadcasts a `MSG_PING` heartbeat packet.
4. All packets are sent to the Wi-Fi broadcast address
   `FF:FF:FF:FF:FF:FF` so any receiver on the same channel picks them up
   without prior pairing.

### Receiver node (`receiver`)

1. Listens passively for ESP-NOW broadcasts.
2. Maintains a table of up to **10** sender nodes, identified by MAC address.
3. On each received packet the sender is marked **ONLINE** (if it wasn't
   already) and its `lastSeen` timestamp is updated.
4. Checks every second whether any known node has exceeded
   `NODE_TIMEOUT_MS` (default **15 seconds** – 3× the ping interval).
   If so, the node is declared **OFFLINE** and logged to Serial.
5. Motion events from any sender are printed immediately to Serial.

### Ping / offline detection

```
Sender          Receiver
  │──── PING ────►│   lastSeen updated → node ONLINE
  │──── PING ────►│   lastSeen updated
  │  (power loss) │
  │               │   15 s elapse → node declared OFFLINE
  │               │   … reconnects …
  │──── PING ────►│   node declared ONLINE again
```

---

## Building and flashing

### Prerequisites

- [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html)
  (CLI or VS Code extension)

### Flash the sender node

```bash
cd pir_sender
pio run --target upload
pio device monitor      # view Serial output
```

### Flash the receiver node

```bash
cd receiver
pio run --target upload
pio device monitor      # view Serial output
```

---

## Serial output examples

### Sender

```
=== PIR Sender Node ===
MAC address: AA:BB:CC:DD:EE:FF
PIR sensor on GPIO 2
Ping interval: 5000 ms
Ready.

[PING] Sent
[PIR] Motion DETECTED
[ESPNOW] TX OK
[PIR] Motion CLEARED
[ESPNOW] TX OK
[PING] Sent
```

### Receiver

```
=== Receiver / Hub Node ===
MAC address: 11:22:33:44:55:66
Node timeout: 15000 ms
Listening for ESP-NOW broadcasts...

[NODE] New sender discovered: AA:BB:CC:DD:EE:FF
[NODE] ONLINE:  AA:BB:CC:DD:EE:FF
[PING] Heartbeat from AA:BB:CC:DD:EE:FF  (node uptime 5012 ms)
[PIR]  Motion DETECTED  from AA:BB:CC:DD:EE:FF
[PIR]  Motion CLEARED   from AA:BB:CC:DD:EE:FF
[NODE] OFFLINE: AA:BB:CC:DD:EE:FF  (last seen 15043 ms ago)
```

---

## Configuration

| Constant | File | Default | Description |
|----------|------|---------|-------------|
| `PIR_PIN` | `pir_sender/src/main.cpp` | `2` (D0) | GPIO connected to HW777 OUT |
| `PING_INTERVAL_MS` | `pir_sender/include/common.h` | `5000` | ms between heartbeat pings |
| `NODE_TIMEOUT_MS` | `receiver/include/common.h` | `15000` | ms before a node is offline |
| `MAX_NODES` | `receiver/src/main.cpp` | `10` | Max concurrent sender nodes |

---

## License

MIT