#include "chess_game.h"

#include <Arduino.h>
#include <string.h>
#include "hall_sensor_array.h"

ChessGame& ChessGame::instance() {
    static ChessGame game;
    return game;
}

bool ChessGame::loadPosition(const char* fen) {
    return loadFen(fen);
}

// Appelé quand la position de départ est détectée sur le plateau
void ChessGame::startGame() {
    resetHistory();
    for (int square = 0; square < 64; ++square)
        expected_[square] = board_[square] != EMPTY_SQUARE;
    clearInteraction();
}

void ChessGame::update(const uint8_t occupancy[BOARD_SIZE][BOARD_SIZE]) {
    importOccupancy(occupancy);

    switch (interactionState_) {
        case InteractionState::Idle:            handleIdle();            break;
        case InteractionState::PieceLifted:     handlePieceLifted();     break;
        case InteractionState::IllegalMove:     handleIllegalMove();     break;
        case InteractionState::ImpossibleState: handleImpossibleState(); break;
    }
}

void ChessGame::importOccupancy(const uint8_t occupancy[BOARD_SIZE][BOARD_SIZE]) {
    for (int file = 0; file < BOARD_SIZE; ++file)
        for (int rank = 0; rank < BOARD_SIZE; ++rank)
            sensed_[squareAt(file, rank)] = occupancy[file][rank];
}

void ChessGame::acceptSensedAsExpected() {
    memcpy(expected_, sensed_, sizeof(expected_));
}

// ---------------------------------------------------------------------------
//  Machine à états
// ---------------------------------------------------------------------------

void ChessGame::handleIdle() {
    int differences  = 0;
    int changedSquare = -1;
    for (int square = 0; square < 64; ++square) {
        if (sensed_[square] != expected_[square]) {
            ++differences;
            changedSquare = square;
        }
    }
    if (differences == 0) return;

    // Une seule pièce soulevée : c'est le début d'un coup
    if (differences == 1 && expected_[changedSquare] && !sensed_[changedSquare]) {
        onPieceLifted(changedSquare);
        return;
    }
    markImpossibleState();
}

void ChessGame::handlePieceLifted() {
    refreshClearedCaptureSquares();

    // Un coup est terminé quand le plateau correspond exactement à son résultat.
    // Pour une prise, la pièce adverse doit d'abord avoir été retirée.
    for (const Move& move : candidateMoves_) {
        if (!isBoardConsistentWith(move, true)) continue;
        if (move.isCapture() && !captureSquareCleared_[move.to]) continue;

        std::string message = "Coup joue : " + toSan(move) + "\n";
        Serial.print(message.c_str());
        playMove(move);
        acceptSensedAsExpected();
        clearInteraction();
        return;
    }

    bool anyDifference = false;
    for (int square = 0; square < 64; ++square) {
        if (sensed_[square] != expected_[square]) {
            anyDifference = true;
            break;
        }
    }
    if (!anyDifference) {  // pièce reposée sur sa case
        clearInteraction();
        return;
    }

    if (candidateMoves_.empty()) {
        // Pièce sans coup légal : seule sa case d'origine peut différer
        for (int square = 0; square < 64; ++square) {
            if (square != liftedSquare_ && sensed_[square] != expected_[square]) {
                markIllegalMove();
                return;
            }
        }
        return;
    }

    // Coup en cours (ex. : roque à moitié joué) : on attend
    for (const Move& move : candidateMoves_) {
        if (isBoardConsistentWith(move, false)) return;
    }
    markIllegalMove();
}

void ChessGame::handleIllegalMove() {
    int differences = 0;
    for (int square = 0; square < 64; ++square)
        differences += sensed_[square] != expected_[square];

    if (differences == 0) {
        Serial.print("Plateau restaure, reprise du jeu.\n");
        clearInteraction();
    } else if (differences > 2) {
        markImpossibleState();
    }
}

void ChessGame::handleImpossibleState() {
    if (memcmp(sensed_, expected_, sizeof(sensed_)) != 0) return;
    Serial.print("Plateau restaure, reprise du jeu.\n");
    clearInteraction();
}

// ---------------------------------------------------------------------------
//  Transitions
// ---------------------------------------------------------------------------

void ChessGame::onPieceLifted(Square square) {
    liftedSquare_ = square;
    candidateMoves_.clear();

    MoveList legalMoves;
    generateLegalMoves(legalMoves);
    for (int i = 0; i < legalMoves.count; ++i) {
        if (legalMoves.moves[i].from == square) candidateMoves_.push_back(legalMoves.moves[i]);
    }

    interactionState_ = InteractionState::PieceLifted;
    memset(captureSquareCleared_, 0, sizeof(captureSquareCleared_));

    char message[128];
    if (candidateMoves_.empty()) {
        snprintf(message, sizeof(message),
                 "Piece soulevee en %c%c - aucun coup legal depuis cette case (%s).\n",
                 fileChar(square), rankChar(square),
                 whiteToMove_ ? "aux blancs de jouer" : "aux noirs de jouer");
    } else {
        snprintf(message, sizeof(message), "Piece soulevee en %c%c\n", fileChar(square),
                 rankChar(square));
    }
    Serial.print(message);
}

void ChessGame::markIllegalMove() {
    interactionState_ = InteractionState::IllegalMove;
    Serial.print("Coup illegal ! Remettez la piece a sa case d'origine.\n");
}

void ChessGame::markImpossibleState() {
    interactionState_ = InteractionState::ImpossibleState;
    liftedSquare_     = -1;
    candidateMoves_.clear();
    Serial.print("Etat impossible ! Remettez toutes les pieces a leur position precedente.\n");
}

void ChessGame::clearInteraction() {
    liftedSquare_     = -1;
    interactionState_ = InteractionState::Idle;
    candidateMoves_.clear();
}

// Note les cases de prise dont la pièce adverse a été retirée (lecture brute du capteur)
void ChessGame::refreshClearedCaptureSquares() {
    HallSensorArray& sensors = HallSensorArray::instance();
    for (const Move& move : candidateMoves_) {
        if (!move.isCapture() || captureSquareCleared_[move.to]) continue;
        if (!sensors.isPieceDetected(fileOf(move.to), rankOf(move.to)))
            captureSquareCleared_[move.to] = 1;
    }
}

// ---------------------------------------------------------------------------
//  Comparaison plateau physique / coup
// ---------------------------------------------------------------------------

// includeMove = true  : le plateau doit montrer le coup entièrement joué.
// includeMove = false : les cases touchées par le coup sont ignorées (coup en cours).
bool ChessGame::isBoardConsistentWith(const Move& move, bool includeMove) const {
    for (Square square = 0; square < 64; ++square) {
        bool occupiedAfter;
        if (!moveAffectsSquare(move, square, occupiedAfter)) {
            if (sensed_[square] != expected_[square]) return false;
        } else if (includeMove) {
            if (sensed_[square] != occupiedAfter) return false;
        }
    }
    return true;
}

bool ChessGame::moveAffectsSquare(const Move& move, Square square, bool& occupiedAfter) {
    if (square == move.from) { occupiedAfter = false; return true; }
    if (square == move.to)   { occupiedAfter = true;  return true; }

    // Tour du roque et pion pris en passant
    switch (move.type) {
        case MOVE_WHITE_CASTLE_SHORT:
            if (square == 63) { occupiedAfter = false; return true; }  // h1
            if (square == 61) { occupiedAfter = true;  return true; }  // f1
            break;
        case MOVE_BLACK_CASTLE_SHORT:
            if (square == 7)  { occupiedAfter = false; return true; }  // h8
            if (square == 5)  { occupiedAfter = true;  return true; }  // f8
            break;
        case MOVE_WHITE_CASTLE_LONG:
            if (square == 56) { occupiedAfter = false; return true; }  // a1
            if (square == 59) { occupiedAfter = true;  return true; }  // d1
            break;
        case MOVE_BLACK_CASTLE_LONG:
            if (square == 0)  { occupiedAfter = false; return true; }  // a8
            if (square == 3)  { occupiedAfter = true;  return true; }  // d8
            break;
        case MOVE_WHITE_EN_PASSANT:
            if (square == move.to + 8) { occupiedAfter = false; return true; }
            break;
        case MOVE_BLACK_EN_PASSANT:
            if (square == move.to - 8) { occupiedAfter = false; return true; }
            break;
        default:
            break;
    }
    return false;
}

// ---------------------------------------------------------------------------
//  Requêtes pour les LED et l'interface web
// ---------------------------------------------------------------------------

bool ChessGame::isLegalTarget(int file, int rank) const {
    if (interactionState_ != InteractionState::PieceLifted) return false;
    Square square = squareAt(file, rank);
    for (const Move& move : candidateMoves_)
        if (move.to == square) return true;
    return false;
}

bool ChessGame::isCaptureTarget(int file, int rank) const {
    if (interactionState_ != InteractionState::PieceLifted) return false;
    Square square = squareAt(file, rank);
    for (const Move& move : candidateMoves_)
        if (move.to == square && move.isCapture()) return true;
    return false;
}

bool ChessGame::isSquareMismatched(int file, int rank) const {
    Square square = squareAt(file, rank);
    return sensed_[square] != expected_[square];
}

bool ChessGame::isKingInCheckAt(int file, int rank) const {
    if (sideToMoveKing() != squareAt(file, rank)) return false;
    return isSideToMoveInCheck();
}
