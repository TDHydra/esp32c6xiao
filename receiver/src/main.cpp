/*
 * receiver/src/main.cpp
 * ─────────────────────────────────────────────────────────────────────────────
 * Receiver / Hub Node - Seeed Studio XIAO ESP32-C6
 *
 * Behaviour:
 *   • Listens for ESP-NOW packets broadcast by one or more PIR sender nodes.
 *   • Processes MSG_MOTION packets and prints motion events to Serial.
 *   • Tracks the last-seen time for every discovered sender node.
 *   • Periodically checks timeouts: if a node has not sent any packet
 *     (ping or motion) within NODE_TIMEOUT_MS, it is declared OFFLINE.
 *     When it is heard again it is declared ONLINE.
 *
 * Supported sender nodes: up to MAX_NODES concurrent nodes.
 * ─────────────────────────────────────────────────────────────────────────────
 */

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include "common.h"

// ── Configuration ─────────────────────────────────────────────────────────────
static constexpr int MAX_NODES = 10;

// ── Node tracking table ───────────────────────────────────────────────────────
struct NodeInfo {
    uint8_t      mac[6];
    unsigned long lastSeenMs;
    bool         online;
    bool         inUse;
};

static NodeInfo nodes[MAX_NODES];

// ── Helpers ───────────────────────────────────────────────────────────────────
static String macToString(const uint8_t *mac) {
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

// Return a pointer to the NodeInfo for a given MAC, creating a new entry if
// this is the first packet seen from that sender.  Returns nullptr if the
// table is full.
static NodeInfo *findOrCreateNode(const uint8_t *mac) {
    for (int i = 0; i < MAX_NODES; i++) {
        if (nodes[i].inUse && memcmp(nodes[i].mac, mac, 6) == 0) {
            return &nodes[i];
        }
    }
    for (int i = 0; i < MAX_NODES; i++) {
        if (!nodes[i].inUse) {
            memcpy(nodes[i].mac, mac, 6);
            nodes[i].inUse     = true;
            nodes[i].online    = false; // will be set true on first packet
            nodes[i].lastSeenMs = 0;
            Serial.printf("[NODE] New sender discovered: %s\n",
                          macToString(mac).c_str());
            return &nodes[i];
        }
    }
    Serial.println("[WARN] Node table full - ignoring sender");
    return nullptr;
}

// Mark a node as seen now and handle online/offline transitions
static void touchNode(NodeInfo *node) {
    node->lastSeenMs = millis();
    if (!node->online) {
        node->online = true;
        Serial.printf("[NODE] ONLINE:  %s\n", macToString(node->mac).c_str());
    }
}

// ── ESP-NOW receive callback ──────────────────────────────────────────────────
static void onDataReceived(const esp_now_recv_info_t *info,
                            const uint8_t *data, int len) {
    if (len < static_cast<int>(sizeof(EspNowMessage))) {
        Serial.printf("[WARN] Short packet (%d bytes) - ignored\n", len);
        return;
    }

    const EspNowMessage *msg = reinterpret_cast<const EspNowMessage *>(data);

    NodeInfo *node = findOrCreateNode(msg->senderMac);
    if (!node) { return; }

    touchNode(node);

    switch (msg->type) {
        case MSG_PING:
            Serial.printf("[PING] Heartbeat from %s  (node uptime %lu ms)\n",
                          macToString(msg->senderMac).c_str(),
                          static_cast<unsigned long>(msg->timestamp));
            break;

        case MSG_MOTION:
            Serial.printf("[PIR]  Motion %-8s from %s\n",
                          msg->motionDetected ? "DETECTED" : "CLEARED",
                          macToString(msg->senderMac).c_str());
            break;

        default:
            Serial.printf("[WARN] Unknown message type %d from %s\n",
                          static_cast<int>(msg->type),
                          macToString(msg->senderMac).c_str());
            break;
    }
}

// ── Setup ─────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(500); // allow USB CDC to enumerate

    Serial.println("\n=== Receiver / Hub Node ===");

    memset(nodes, 0, sizeof(nodes));

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    Serial.printf("MAC address:  %s\n", WiFi.macAddress().c_str());
    Serial.printf("Node timeout: %d ms\n", NODE_TIMEOUT_MS);

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ERROR] esp_now_init() failed - halting");
        while (true) { delay(1000); }
    }

    esp_now_register_recv_cb(onDataReceived);

    Serial.println("Listening for ESP-NOW broadcasts...\n");
}

// ── Main loop ─────────────────────────────────────────────────────────────────
void loop() {
    unsigned long now = millis();

    // Check every second whether any known node has timed out
    static unsigned long lastCheckMs = 0;
    if (now - lastCheckMs >= 1000) {
        lastCheckMs = now;

        for (int i = 0; i < MAX_NODES; i++) {
            NodeInfo &n = nodes[i];
            if (!n.inUse || !n.online) { continue; }

            if (now - n.lastSeenMs > NODE_TIMEOUT_MS) {
                n.online = false;
                Serial.printf("[NODE] OFFLINE: %s  (last seen %lu ms ago)\n",
                              macToString(n.mac).c_str(),
                              now - n.lastSeenMs);
            }
        }
    }

    delay(10);
}
