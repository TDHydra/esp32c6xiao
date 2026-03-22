#pragma once

#include <stdint.h>

// Ping interval - sender broadcasts a ping every PING_INTERVAL_MS milliseconds
#define PING_INTERVAL_MS 5000

// Message types exchanged over ESP-NOW
typedef enum : uint8_t {
    MSG_PING   = 0, // Heartbeat - no payload beyond header
    MSG_MOTION = 1, // PIR motion state change
} MessageType;

// Fixed-size message payload for all ESP-NOW packets
typedef struct {
    MessageType type;        // MSG_PING or MSG_MOTION
    uint8_t     senderMac[6]; // MAC address of the originating node
    uint32_t    timestamp;    // millis() on the sender at time of send
    bool        motionDetected; // only meaningful when type == MSG_MOTION
} EspNowMessage;
