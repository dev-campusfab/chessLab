#pragma once
#include <vector>
#include "chess_engine.h"
#include "config.h"

// Où en est le joueur avec les pièces physiques
enum class InteractionState : int {
    Idle           = 0,  // plateau conforme à la position, en attente
    PieceLifted    = 1,  // une pièce est en main
    IllegalMove    = 2,  // coup refusé : remettre la pièce
    ImpossibleState = 3, // plateau incohérent : tout remettre comme avant
};

// Partie réelle : suit le plateau physique (capteurs) et le confronte au moteur.
class ChessGame : public ChessEngine {
public:
    static ChessGame& instance();

    bool loadPosition(const char* fen);
    void startGame();
    void update(const uint8_t occupancy[BOARD_SIZE][BOARD_SIZE]);

    InteractionState interactionState() const { return interactionState_; }

    // Requêtes par coordonnées (file 0..7 = A..H, rank 0..7 = 1..8)
    bool isLegalTarget(int file, int rank) const;
    bool isCaptureTarget(int file, int rank) const;
    bool isSquareMismatched(int file, int rank) const;
    bool isKingInCheckAt(int file, int rank) const;
    using ChessEngine::pieceAt;
    char pieceAt(int file, int rank) const { return board_[squareAt(file, rank)]; }

private:
    ChessGame() = default;

    void importOccupancy(const uint8_t occupancy[BOARD_SIZE][BOARD_SIZE]);
    void acceptSensedAsExpected();

    void handleIdle();
    void handlePieceLifted();
    void handleIllegalMove();
    void handleImpossibleState();

    void onPieceLifted(Square square);
    void markIllegalMove();
    void markImpossibleState();
    void clearInteraction();
    void refreshClearedCaptureSquares();

    bool isBoardConsistentWith(const Move& move, bool includeMove) const;
    static bool moveAffectsSquare(const Move& move, Square square, bool& occupiedAfter);

    uint8_t           sensed_[64]   = {};  // occupation lue par les capteurs (ordre FEN)
    uint8_t           expected_[64] = {};  // occupation attendue d'après la position
    InteractionState  interactionState_ = InteractionState::Idle;
    int               liftedSquare_     = -1;
    uint8_t           captureSquareCleared_[64] = {};  // la pièce prise a été retirée
    std::vector<Move> candidateMoves_;     // coups légaux de la pièce en main
};
