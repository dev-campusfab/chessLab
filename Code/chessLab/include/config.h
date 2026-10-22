#pragma once
#include <stddef.h>
#include <stdint.h>

// ============================================================================
//  ChessLab — configuration matérielle et réglages
//  (reconstruit depuis le firmware actif app0, ESP32-C6, build du 31/03/2026)
// ============================================================================

// --- WiFi (mode station uniquement, pas de point d'accès de secours) --------
#define WIFI_SSID      "Campusfab"
#define WIFI_PASSWORD  "wifablab"
constexpr int      WIFI_CONNECT_ATTEMPTS  = 30;
constexpr uint32_t WIFI_CONNECT_RETRY_MS  = 300;

// --- Interface web ----------------------------------------------------------
constexpr uint16_t HTTP_PORT                 = 80;
constexpr uint16_t WEBSOCKET_PORT            = 81;
constexpr uint32_t WEB_BROADCAST_INTERVAL_MS = 200;

// --- Port série (USB CDC) ---------------------------------------------------
constexpr uint32_t SERIAL_BAUD_RATE        = 115200;
constexpr size_t   SERIAL_TX_BUFFER_SIZE   = 1024;
constexpr int      SERIAL_WAIT_ATTEMPTS    = 30;    // x 100 ms pour attendre l'hôte USB
constexpr uint32_t SERIAL_STREAM_TIMEOUT   = 30000;

// --- Plateau ----------------------------------------------------------------
constexpr int BOARD_SIZE  = 8;
constexpr int NUM_SQUARES = BOARD_SIZE * BOARD_SIZE;

// --- Bande LED (WS2812, une LED par case, index = file * 8 + rank) ----------
constexpr uint8_t  LED_PIN        = 19;
constexpr uint16_t NUM_LEDS       = NUM_SQUARES;
constexpr uint8_t  LED_BRIGHTNESS = 10;

constexpr uint32_t COLOR_OFF       = 0x000000;
constexpr uint32_t COLOR_BOOT      = 0xC8C8C8;
constexpr uint32_t COLOR_WHITE     = 0xFFFFFF;
constexpr uint32_t COLOR_RED       = 0xFF0000;
constexpr uint32_t COLOR_GREEN     = 0x00FF00;
constexpr uint32_t COLOR_CYAN      = 0x00FFFF;  // case d'arrivée possible
constexpr uint32_t COLOR_ORANGE    = 0xFF8C00;  // prise possible
constexpr uint32_t COLOR_PURPLE    = 0x9400D3;  // roi en échec

// --- Capteurs à effet Hall --------------------------------------------------
// Le décodeur (A0..A2) choisit la colonne (file A..H),
// le multiplexeur (S0..S2) choisit la rangée (rank 1..8).
constexpr uint8_t HALL_ANALOG_PIN = 3;
constexpr uint8_t DECODER_A0_PIN  = 18;
constexpr uint8_t DECODER_A1_PIN  = 16;
constexpr uint8_t DECODER_A2_PIN  = 17;
constexpr uint8_t MUX_S0_PIN      = 0;
constexpr uint8_t MUX_S1_PIN      = 1;
constexpr uint8_t MUX_S2_PIN      = 2;

constexpr uint32_t HALL_SETTLE_US          = 50;
constexpr int      HALL_SAMPLE_COUNT       = 30;
constexpr uint32_t HALL_SAMPLE_INTERVAL_US = 10;
constexpr int      HALL_TRIM_COUNT         = 6;     // échantillons jetés à chaque extrémité
constexpr int      HALL_IDLE_LEVEL         = 1600;  // lecture ADC sans aimant
constexpr int      HALL_DETECTION_DELTA    = 150;   // écart minimal pour détecter une pièce
constexpr uint32_t HALL_DEBOUNCE_MS        = 300;

constexpr uint32_t DEBUG_BOARD_PRINT_INTERVAL_MS = 1000;

// --- Gestion de l'alimentation ----------------------------------------------
constexpr uint8_t  POWER_OFF_PIN         = 5;       // HIGH = coupe l'alimentation
constexpr uint8_t  POWER_BUTTON_PIN      = 4;       // actif à l'état bas (pull-up interne)
constexpr uint32_t POWER_LONG_PRESS_MS   = 3000;
constexpr uint32_t INACTIVITY_TIMEOUT_MS = 600000;  // 10 minutes sans bouger une pièce
