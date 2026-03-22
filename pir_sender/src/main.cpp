/*
 * pir_sender/src/main.cpp
 * ─────────────────────────────────────────────────────────────────────────────
 * PIR Sender Node - Seeed Studio XIAO ESP32-C6
 *
 * Wiring (HW777 PIR sensor):
 *   HW777 VCC  →  3.3 V  (pin 3V3 on XIAO)
 *   HW777 GND  →  GND
 *   HW777 OUT  →  D0  (GPIO 2)
 *
 * Behaviour:
 *   • Reads the PIR output and sends a MSG_MOTION packet whenever the state
 *     changes (motion detected or cleared).
 *   • Sends a MSG_PING broadcast every PING_INTERVAL_MS milliseconds so
 *     receivers can detect when this node goes offline.
 *   • Uses the Wi-Fi broadcast address (FF:FF:FF:FF:FF:FF) as the ESP-NOW
 *     peer so any receiver on the same Wi-Fi channel will pick up packets.
 * ─────────────────────────────────────────────────────────────────────────────
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "common.h"

// ── Pin definition ────────────────────────────────────────────────────────────
// D0 on XIAO ESP32-C6 = GPIO 2.  Change this if you wire to a different pin.
static constexpr int PIR_PIN = 2;

// ── ESP-NOW broadcast address ─────────────────────────────────────────────────
static uint8_t broadcastAddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// ── State ─────────────────────────────────────────────────────────────────────
static unsigned long lastPingMs   = 0;
static bool          lastMotion   = false;
static bool          espNowReady  = false;

// ── Helpers ───────────────────────────────────────────────────────────────────
static void sendMsg(MessageType type, bool motionDetected = false) {
    EspNowMessage msg = {};
    msg.type           = type;
    msg.timestamp      = static_cast<uint32_t>(millis());
    msg.motionDetected = motionDetected;
    WiFi.macAddress(msg.senderMac);

    esp_err_t result = esp_now_send(broadcastAddr,
                                    reinterpret_cast<const uint8_t *>(&msg),
                                    sizeof(msg));
    if (result != ESP_OK) {
        Serial.printf("[ESPNOW] Send error: %s\n", esp_err_to_name(result));
    }
}

// Called by the ESP-NOW stack after each send attempt
static void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
    Serial.printf("[ESPNOW] TX %s\n",
                  status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500); // allow USB CDC to enumerate

    Serial.println("\n=== PIR Sender Node ===");

    // Configure PIR pin
    pinMode(PIR_PIN, INPUT);

    // Wi-Fi must be in station mode for ESP-NOW
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    Serial.printf("MAC address: %s\n", WiFi.macAddress().c_str());

    // Initialise ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("[ERROR] esp_now_init() failed - halting");
        while (true) { delay(1000); }
    }

    esp_now_register_send_cb(onDataSent);

    // Register the broadcast peer (no encryption)
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, broadcastAddr, 6);
    peer.channel = 0;       // 0 = use current Wi-Fi channel
    peer.encrypt = false;

    if (esp_now_add_peer(&peer) != ESP_OK) {
        Serial.println("[ERROR] esp_now_add_peer() failed - halting");
        while (true) { delay(1000); }
    }

    espNowReady = true;
    Serial.printf("PIR sensor on GPIO %d\n", PIR_PIN);
    Serial.printf("Ping interval: %d ms\n", PING_INTERVAL_MS);
    Serial.println("Ready.\n");
}

// ── Main loop ─────────────────────────────────────────────────────────────────
void loop() {
    if (!espNowReady) { return; }

    // ── PIR state change detection ────────────────────────────────────────────
    bool motion = (digitalRead(PIR_PIN) == HIGH);
    if (motion != lastMotion) {
        lastMotion = motion;
        sendMsg(MSG_MOTION, motion);
        Serial.printf("[PIR] Motion %s\n", motion ? "DETECTED" : "CLEARED");
    }

    // ── Periodic ping ─────────────────────────────────────────────────────────
    unsigned long now = millis();
    if (now - lastPingMs >= PING_INTERVAL_MS) {
        lastPingMs = now;
        sendMsg(MSG_PING);
        Serial.println("[PING] Sent");
    }

    delay(50); // ~20 Hz polling is more than sufficient for PIR
}
