#pragma once
#include <stdint.h>
#include "config.h"

// Affichage sur la bande de 64 LED placée sous le plateau.
class LedMatrix {
public:
    static LedMatrix& instance();

    void begin();
    void showSetupGuide(const uint8_t occupancy[BOARD_SIZE][BOARD_SIZE]);
    void showGame();
    void flashStartConfirmation();

private:
    LedMatrix() = default;

    static uint16_t ledIndex(int file, int rank) { return file * BOARD_SIZE + rank; }
};
