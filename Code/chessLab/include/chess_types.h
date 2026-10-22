#pragma once
#include <stdint.h>

// Types de pièces (valeurs identiques au firmware d'origine)
enum class PieceType : uint8_t {
    None   = 0,
    Pawn   = 1,
    Knight = 2,
    Bishop = 3,
    Rook   = 4,
    Queen  = 5,
    King   = 6,
};

enum class PieceColor : uint8_t {
    None  = 0,
    White = 1,
    Black = 2,
};

// Une case est un index 0..63 dans l'ordre FEN : 0 = a8, 7 = h8, 56 = a1, 63 = h1.
typedef uint8_t Square;
constexpr Square NO_SQUARE = 64;

constexpr char EMPTY_SQUARE = ' ';

inline Square squareAt(int file, int rank) { return (7 - rank) * 8 + file; }
inline int    fileOf(Square square)        { return square & 7; }
inline int    rankOf(Square square)        { return 7 - (square >> 3); }
inline char   fileChar(Square square)      { return 'a' + fileOf(square); }
inline char   rankChar(Square square)      { return '1' + rankOf(square); }

enum MoveType : uint8_t {
    MOVE_NORMAL              = 0,
    MOVE_KING                = 1,   // déplacement simple du roi (met à jour sa position)
    MOVE_WHITE_CASTLE_SHORT  = 2,
    MOVE_BLACK_CASTLE_SHORT  = 3,
    MOVE_WHITE_CASTLE_LONG   = 4,
    MOVE_BLACK_CASTLE_LONG   = 5,
    MOVE_PROMOTE_QUEEN       = 6,
    MOVE_PROMOTE_ROOK        = 7,
    MOVE_PROMOTE_BISHOP      = 8,
    MOVE_PROMOTE_KNIGHT      = 9,
    MOVE_WHITE_DOUBLE_PUSH   = 10,
    MOVE_BLACK_DOUBLE_PUSH   = 11,
    MOVE_WHITE_EN_PASSANT    = 12,
    MOVE_BLACK_EN_PASSANT    = 13,
};

// Coup codé sur 4 octets, comme dans le firmware d'origine.
struct Move {
    Square   from;
    Square   to;
    MoveType type;
    char     captured;  // pièce prise (notation FEN) ou EMPTY_SQUARE

    bool isCapture() const { return captured != EMPTY_SQUARE; }
    bool operator==(const Move& other) const {
        return from == other.from && to == other.to &&
               type == other.type && captured == other.captured;
    }
};

constexpr int MAX_MOVES = 218;  // nombre maximal de coups possibles dans une position

struct MoveList {
    int  count = 0;
    Move moves[MAX_MOVES];

    void add(Square from, Square to, MoveType type, char captured) {
        if (count < MAX_MOVES) moves[count++] = Move{from, to, type, captured};
    }
};
