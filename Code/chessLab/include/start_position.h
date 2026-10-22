#pragma once
#include "chess_types.h"
#include "config.h"

// Modèle de la position de départ, utilisé pendant la phase de mise en place.
class StartPosition {
public:
    struct SquareContent {
        PieceType  type;
        PieceColor color;
    };

    static StartPosition& instance();

    const SquareContent& at(int file, int rank) const { return squares_[file][rank]; }
    bool hasPieceAt(int file, int rank) const { return squares_[file][rank].type != PieceType::None; }
    bool matches(const uint8_t occupancy[BOARD_SIZE][BOARD_SIZE]) const;

private:
    StartPosition();

    SquareContent squares_[BOARD_SIZE][BOARD_SIZE];  // [file][rank]
};
