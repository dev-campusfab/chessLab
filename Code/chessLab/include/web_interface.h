#pragma once
#include <stdint.h>
#include "config.h"

// Connexion WiFi, page web (port 80) et diffusion de l'état du plateau
// en JSON sur un WebSocket (port 81).
class WebInterface {
public:
    static WebInterface& instance();

    void begin();
    void update(const uint8_t occupancy[BOARD_SIZE][BOARD_SIZE]);

private:
    WebInterface() = default;

    void broadcastState(const uint8_t occupancy[BOARD_SIZE][BOARD_SIZE]);

    bool     wifiConnected_    = false;
    uint32_t lastBroadcastMs_  = 0;
};
