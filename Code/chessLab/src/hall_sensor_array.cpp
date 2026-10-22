#include "hall_sensor_array.h"

#include <Arduino.h>
#include "piece_names.h"
#include "sensor_mux.h"
#include "start_position.h"

HallSensorArray& HallSensorArray::instance() {
    static HallSensorArray sensors;
    return sensors;
}

void HallSensorArray::scanAll() {
    for (int rank = 0; rank < BOARD_SIZE; ++rank)
        for (int file = 0; file < BOARD_SIZE; ++file)
            updateSquare(file, rank);
}

void HallSensorArray::updateSquare(int file, int rank) {
    SquareSensor& sensor = squares_[file][rank];
    uint16_t raw = SensorMux::instance().readSquare(file, rank);
    sensor.rawValue = raw;

    // Un aimant fait varier la tension dans un sens ou dans l'autre selon sa polarité
    bool detected = abs((int)raw - HALL_IDLE_LEVEL) > HALL_DETECTION_DELTA;
    if (detected != sensor.pieceDetected) {
        sensor.lastChangeMs  = millis();
        sensor.stable        = false;
        sensor.pieceDetected = detected;
    }

    if (millis() - sensor.lastChangeMs > HALL_DEBOUNCE_MS) {
        sensor.stable = true;
        setStableOccupancy(file, rank, sensor.pieceDetected);
    }
}

void HallSensorArray::setStableOccupancy(int file, int rank, bool occupied) {
    uint64_t bit = 1ULL << (rank * 8 + file);
    if (occupied) stableOccupancy_ |= bit;
    else          stableOccupancy_ &= ~bit;
}

void HallSensorArray::getOccupancy(uint8_t occupancy[BOARD_SIZE][BOARD_SIZE]) const {
    for (int file = 0; file < BOARD_SIZE; ++file)
        for (int rank = 0; rank < BOARD_SIZE; ++rank)
            occupancy[file][rank] = (stableOccupancy_ >> (rank * 8 + file)) & 1;
}

bool HallSensorArray::isPieceDetected(int file, int rank) const {
    return squares_[file][rank].pieceDetected;
}

// Affiche le plateau détecté une fois par seconde :
// '.' case vide, lettre = pièce attendue en position de départ, 'X' = pièce hors position.
void HallSensorArray::printDebugBoard() {
    if (lastDebugPrintMs_ != 0 && millis() - lastDebugPrintMs_ < DEBUG_BOARD_PRINT_INTERVAL_MS)
        return;
    lastDebugPrintMs_ = millis();

    uint8_t occupancy[BOARD_SIZE][BOARD_SIZE];
    getOccupancy(occupancy);

    char text[512];
    int  length = snprintf(text, sizeof(text), "\n  A B C D E F G H\n");
    for (int rankLabel = BOARD_SIZE; rankLabel >= 1; --rankLabel) {
        length += snprintf(text + length, sizeof(text) - length, "%d ", rankLabel);
        for (int file = 0; file < BOARD_SIZE; ++file) {
            const char* symbol = ".";
            if (occupancy[file][rankLabel - 1]) {
                const StartPosition::SquareContent& expected =
                    StartPosition::instance().at(file, rankLabel - 1);
                symbol = expected.type != PieceType::None ? pieceSymbol(expected.type) : "X";
            }
            length += snprintf(text + length, sizeof(text) - length, "%s ", symbol);
        }
        length += snprintf(text + length, sizeof(text) - length, "\n");
    }
    Serial.print(text);
    Serial.flush();
}
