#include "start_position.h"

#include <string.h>

StartPosition& StartPosition::instance() {
    static StartPosition position;
    return position;
}

StartPosition::StartPosition() {
    static const PieceType BACK_RANK[BOARD_SIZE] = {
        PieceType::Rook, PieceType::Knight, PieceType::Bishop, PieceType::Queen,
        PieceType::King, PieceType::Bishop, PieceType::Knight, PieceType::Rook,
    };

    memset(squares_, 0, sizeof(squares_));
    for (int file = 0; file < BOARD_SIZE; ++file) {
        squares_[file][0] = {BACK_RANK[file], PieceColor::White};
        squares_[file][1] = {PieceType::Pawn, PieceColor::White};
        squares_[file][6] = {PieceType::Pawn, PieceColor::Black};
        squares_[file][7] = {BACK_RANK[file], PieceColor::Black};
    }
}

// Vrai si chaque case occupée l'est exactement comme dans la position de départ
bool StartPosition::matches(const uint8_t occupancy[BOARD_SIZE][BOARD_SIZE]) const {
    for (int file = 0; file < BOARD_SIZE; ++file)
        for (int rank = 0; rank < BOARD_SIZE; ++rank)
            if ((bool)occupancy[file][rank] != hasPieceAt(file, rank)) return false;
    return true;
}
