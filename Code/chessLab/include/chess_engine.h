#pragma once
#include <string>
#include "chess_types.h"

// Résultat d'une position (valeurs identiques au firmware d'origine)
enum class GameResult : int8_t {
    StalemateWhiteToMove = -2,
    BlackWins            = -1,
    Ongoing              = 0,
    WhiteWins            = 1,
    StalemateBlackToMove = 2,
};

// Moteur d'échecs : position, génération des coups légaux, notation SAN.
// Le plateau est stocké en notation FEN (majuscules = blancs, minuscules = noirs).
class ChessEngine {
public:
    ChessEngine();
    virtual ~ChessEngine() = default;

    bool loadFen(const char* fen);

    void        generateLegalMoves(MoveList& legalMoves);
    void        playMove(const Move& move);  // joue le coup et l'ajoute à l'historique
    std::string toSan(const Move& move);

    bool isSideToMoveInCheck() const;
    bool isWhiteToMove() const { return whiteToMove_; }
    char pieceAt(Square square) const { return board_[square]; }

protected:
    // Informations sauvegardées avant chaque coup pour pouvoir l'annuler
    struct PositionState {
        Square  enPassantSquare;
        Square  whiteKingSquare;
        Square  blackKingSquare;
        uint8_t castlingRights;
    };

    static constexpr uint8_t CASTLE_WHITE_SHORT = 0x01;
    static constexpr uint8_t CASTLE_WHITE_LONG  = 0x02;
    static constexpr uint8_t CASTLE_BLACK_SHORT = 0x04;
    static constexpr uint8_t CASTLE_BLACK_LONG  = 0x08;
    static constexpr uint8_t CASTLE_ALL         = 0x0F;

    static constexpr int HISTORY_SIZE = 256;

    char          board_[64];
    bool          whiteToMove_;
    int           halfmoveClock_;
    int           fullmoveNumber_;
    PositionState state_;

    Move          moveHistory_[HISTORY_SIZE];
    uint8_t       moveHistoryCount_;
    PositionState undoStack_[HISTORY_SIZE];
    uint8_t       undoStackCount_;

    void resetHistory();

    void makeMove(const Move& move);
    void unmakeMove(const Move& move);

    void generatePseudoLegalMoves(MoveList& moves) const;
    void generatePawnMoves(MoveList& moves, Square from) const;
    void generateStepMoves(MoveList& moves, Square from, const int8_t (*offsets)[2], int count,
                           MoveType type) const;
    void generateSlidingMoves(MoveList& moves, Square from, const int8_t (*directions)[2],
                              int count) const;
    void generateCastlingMoves(MoveList& moves, Square kingSquare) const;

    bool isSquareAttacked(Square square, bool byWhite) const;
    bool isPieceAttacked(Square square) const;  // attaquée par le camp adverse de la pièce
    bool isOwnPiece(char piece) const;
    bool isEnemyPiece(char piece) const;
    Square sideToMoveKing() const;
    Square sideNotToMoveKing() const;

    bool evaluatePosition(MoveList* moves, GameResult& result);
    void generateAnnotatedLegalMoves(MoveList& legalMoves, bool* givesCheck, bool* givesMate,
                                     bool* givesStalemate);

    enum class SanStyle { Pawn, Castle, Piece, PieceWithFile, PieceWithRank, PieceWithSquare };
    bool formatSan(const Move& move, SanStyle style, char* out, size_t size) const;
};
