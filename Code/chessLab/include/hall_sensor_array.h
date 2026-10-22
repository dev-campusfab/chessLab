#pragma once
#include <stdint.h>
#include "config.h"

// Matrice 8x8 de capteurs Hall avec anti-rebond par case.
class HallSensorArray {
public:
    static HallSensorArray& instance();

    void scanAll();
    void getOccupancy(uint8_t occupancy[BOARD_SIZE][BOARD_SIZE]) const;  // [file][rank]
    bool isPieceDetected(int file, int rank) const;  // lecture instantanée, sans anti-rebond
    void printDebugBoard();

private:
    HallSensorArray() = default;

    struct SquareSensor {
        uint16_t rawValue      = 0;
        bool     pieceDetected = false;  // dernière lecture
        bool     stable        = false;  // lecture inchangée depuis HALL_DEBOUNCE_MS
        uint32_t lastChangeMs  = 0;
    };

    void updateSquare(int file, int rank);
    void setStableOccupancy(int file, int rank, bool occupied);

    uint32_t     lastDebugPrintMs_ = 0;
    SquareSensor squares_[BOARD_SIZE][BOARD_SIZE];  // [file][rank]
    uint64_t     stableOccupancy_ = 0;              // bit (rank * 8 + file)
};
