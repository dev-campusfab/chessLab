#include "led_matrix.h"

#include <Adafruit_NeoPixel.h>
#include "chess_game.h"
#include "start_position.h"

static Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

LedMatrix& LedMatrix::instance() {
    static LedMatrix leds;
    return leds;
}

void LedMatrix::begin() {
    Serial.print("Initializing LED Stripe...\n");
    strip.begin();
    strip.setBrightness(LED_BRIGHTNESS);
    strip.fill(COLOR_BOOT);
    strip.show();
    delay(2000);
    strip.fill(COLOR_OFF);
    strip.show();
}

// Mise en place : vert = pièce bien posée, rouge = case de départ encore vide
void LedMatrix::showSetupGuide(const uint8_t occupancy[BOARD_SIZE][BOARD_SIZE]) {
    const StartPosition& start = StartPosition::instance();
    for (int rank = 0; rank < BOARD_SIZE; ++rank) {
        for (int file = 0; file < BOARD_SIZE; ++file) {
            uint32_t color = COLOR_OFF;
            if (start.hasPieceAt(file, rank)) color = occupancy[file][rank] ? COLOR_GREEN : COLOR_RED;
            strip.setPixelColor(ledIndex(file, rank), color);
        }
    }
    strip.show();
}

// En partie : roi en échec en violet, et pour la pièce en main
// les prises en orange et les autres cases d'arrivée en cyan.
void LedMatrix::showGame() {
    const ChessGame&       game  = ChessGame::instance();
    const InteractionState state = game.interactionState();

    if (state == InteractionState::ImpossibleState) {
        strip.fill(COLOR_RED);
    } else {
        for (int rank = 0; rank < BOARD_SIZE; ++rank) {
            for (int file = 0; file < BOARD_SIZE; ++file) {
                uint32_t color = COLOR_OFF;
                if (state == InteractionState::IllegalMove) {
                    if (game.isSquareMismatched(file, rank)) color = COLOR_RED;
                } else if (game.isKingInCheckAt(file, rank)) {
                    color = COLOR_PURPLE;
                } else if (state == InteractionState::PieceLifted) {
                    if (game.isCaptureTarget(file, rank))    color = COLOR_ORANGE;
                    else if (game.isLegalTarget(file, rank)) color = COLOR_CYAN;
                }
                strip.setPixelColor(ledIndex(file, rank), color);
            }
        }
    }
    strip.show();
}

void LedMatrix::flashStartConfirmation() {
    strip.fill(COLOR_WHITE);
    strip.show();
    delay(400);
    strip.fill(COLOR_OFF);
    strip.show();
    delay(150);
}
