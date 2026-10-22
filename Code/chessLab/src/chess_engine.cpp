#include "chess_engine.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

const char* const START_BOARD =
    "rnbqkbnr"
    "pppppppp"
    "        "
    "        "
    "        "
    "        "
    "PPPPPPPP"
    "RNBQKBNR";

constexpr Square A8 = 0, C8 = 2, D8 = 3, E8 = 4, F8 = 5, G8 = 6, H8 = 7;
constexpr Square A1 = 56, B1 = 57, C1 = 58, D1 = 59, E1 = 60, F1 = 61, G1 = 62, H1 = 63;
constexpr Square B8 = 1;

const int8_t KNIGHT_OFFSETS[8][2] = {
    {1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2},
};
const int8_t KING_OFFSETS[8][2] = {
    {0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1},
};
const int8_t ROOK_DIRECTIONS[4][2]   = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};
const int8_t BISHOP_DIRECTIONS[4][2] = {{1, 1}, {1, -1}, {-1, -1}, {-1, 1}};

// Ordre de génération des promotions (la dame est essayée en premier)
const MoveType PROMOTION_ORDER[4] = {
    MOVE_PROMOTE_QUEEN, MOVE_PROMOTE_KNIGHT, MOVE_PROMOTE_BISHOP, MOVE_PROMOTE_ROOK,
};

bool isOnBoard(int file, int rank) { return file >= 0 && file < 8 && rank >= 0 && rank < 8; }

bool isWhitePiece(char piece) { return piece >= 'A' && piece <= 'Z'; }
bool isBlackPiece(char piece) { return piece >= 'a' && piece <= 'z'; }

// Droits de roque conservés quand une pièce part de / arrive sur cette case
uint8_t castlingMaskFor(Square square) {
    switch (square) {
        case A8: return 0xF7;  // perd q
        case E8: return 0xF3;  // perd k et q
        case H8: return 0xFB;  // perd k
        case A1: return 0xFD;  // perd Q
        case E1: return 0xFC;  // perd K et Q
        case H1: return 0xFE;  // perd K
        default: return 0xFF;
    }
}

const char* promotionSuffix(MoveType type) {
    switch (type) {
        case MOVE_PROMOTE_QUEEN:  return "=Q";
        case MOVE_PROMOTE_ROOK:   return "=R";
        case MOVE_PROMOTE_BISHOP: return "=B";
        case MOVE_PROMOTE_KNIGHT: return "=N";
        default:                  return "";
    }
}

const char* skipBlanks(const char* text) {
    while (*text == ' ' || *text == '\t') ++text;
    return text;
}

}  // namespace

// ---------------------------------------------------------------------------
//  Construction / FEN
// ---------------------------------------------------------------------------

ChessEngine::ChessEngine() {
    whiteToMove_ = true;
    memcpy(board_, START_BOARD, sizeof(board_));
    state_.enPassantSquare = NO_SQUARE;
    state_.whiteKingSquare = E1;
    state_.blackKingSquare = E8;
    state_.castlingRights  = CASTLE_ALL;
    halfmoveClock_  = 0;
    fullmoveNumber_ = 1;
    resetHistory();
    memset(moveHistory_, 0, sizeof(moveHistory_));
    memset(undoStack_, 0, sizeof(undoStack_));
}

void ChessEngine::resetHistory() {
    // L'entrée 0 de l'historique est un coup nul servant de sentinelle
    moveHistoryCount_ = 1;
    moveHistory_[0]   = Move{0, 0, MOVE_NORMAL, 0};
    undoStackCount_   = 0;
}

bool ChessEngine::loadFen(const char* fen) {
    char   newBoard[64];
    Square whiteKing = NO_SQUARE;
    Square blackKing = NO_SQUARE;
    memset(newBoard, EMPTY_SQUARE, sizeof(newBoard));

    // 1. Placement des pièces
    const char* cursor = skipBlanks(fen);
    int file = 0, rank = 7;
    for (; *cursor && *cursor != ' ' && *cursor != '\t'; ++cursor) {
        char c = *cursor;
        if (c == '/') {
            if (file != 8 || rank == 0) return false;
            file = 0;
            --rank;
        } else if (c >= '1' && c <= '8') {
            file += c - '0';
            if (file > 8) return false;
        } else if (strchr("PNBRQKpnbrqk", c)) {
            if (file >= 8) return false;
            Square square    = squareAt(file++, rank);
            newBoard[square] = c;
            if (c == 'K') whiteKing = square;
            if (c == 'k') blackKing = square;
        } else {
            return false;
        }
    }
    if (rank != 0 || file != 8) return false;
    if (whiteKing == NO_SQUARE || blackKing == NO_SQUARE) return false;

    // 2. Camp au trait
    cursor = skipBlanks(cursor);
    bool white;
    if ((*cursor & 0xDF) == 'W')      white = true;
    else if ((*cursor & 0xDF) == 'B') white = false;
    else return false;
    ++cursor;

    // 3. Droits de roque
    cursor = skipBlanks(cursor);
    uint8_t castling = 0;
    if (*cursor == '-') {
        ++cursor;
    } else {
        for (; *cursor && *cursor != ' ' && *cursor != '\t'; ++cursor) {
            switch (*cursor) {
                case 'K': castling |= CASTLE_WHITE_SHORT; break;
                case 'Q': castling |= CASTLE_WHITE_LONG;  break;
                case 'k': castling |= CASTLE_BLACK_SHORT; break;
                case 'q': castling |= CASTLE_BLACK_LONG;  break;
                default:  return false;
            }
        }
    }

    // 4. Case de prise en passant
    cursor = skipBlanks(cursor);
    Square enPassant = NO_SQUARE;
    if (*cursor == '-') {
        ++cursor;
    } else if (*cursor) {
        char epFile = tolower(*cursor);
        char epRank = cursor[1];
        if (epFile < 'a' || epFile > 'h' || epRank < '1' || epRank > '8') return false;
        enPassant = squareAt(epFile - 'a', epRank - '1');
        cursor += 2;
    }

    // 5. Compteurs (facultatifs)
    cursor = skipBlanks(cursor);
    int halfmoves = 0, fullmoves = 1;
    if (isdigit((unsigned char)*cursor)) {
        halfmoves = atoi(cursor);
        while (isdigit((unsigned char)*cursor)) ++cursor;
        cursor = skipBlanks(cursor);
        if (isdigit((unsigned char)*cursor)) fullmoves = atoi(cursor);
    }

    memcpy(board_, newBoard, sizeof(board_));
    whiteToMove_            = white;
    state_.enPassantSquare  = enPassant;
    state_.whiteKingSquare  = whiteKing;
    state_.blackKingSquare  = blackKing;
    state_.castlingRights   = castling;
    halfmoveClock_          = halfmoves;
    fullmoveNumber_         = fullmoves;
    resetHistory();
    return true;
}

// ---------------------------------------------------------------------------
//  Jouer / annuler un coup
// ---------------------------------------------------------------------------

void ChessEngine::playMove(const Move& move) {
    moveHistory_[moveHistoryCount_++] = move;
    if (!whiteToMove_) ++fullmoveNumber_;

    bool isPawnMove = (board_[move.from] & 0xDF) == 'P';
    halfmoveClock_  = (isPawnMove || move.isCapture()) ? 0 : halfmoveClock_ + 1;

    makeMove(move);
}

void ChessEngine::makeMove(const Move& move) {
    undoStack_[undoStackCount_++] = state_;

    // Correction : le firmware d'origine ne masquait les droits de roque que
    // sur la case d'arrivée ; un roi revenu sur e1 pouvait encore roquer.
    state_.castlingRights &= castlingMaskFor(move.from) & castlingMaskFor(move.to);
    state_.enPassantSquare = NO_SQUARE;

    const bool white = whiteToMove_;
    switch (move.type) {
        case MOVE_KING:
            board_[move.to]   = board_[move.from];
            board_[move.from] = EMPTY_SQUARE;
            if (white) state_.whiteKingSquare = move.to;
            else       state_.blackKingSquare = move.to;
            break;

        case MOVE_WHITE_CASTLE_SHORT:
            board_[E1] = EMPTY_SQUARE; board_[H1] = EMPTY_SQUARE;
            board_[F1] = 'R';          board_[G1] = 'K';
            state_.whiteKingSquare = G1;
            break;
        case MOVE_BLACK_CASTLE_SHORT:
            board_[E8] = EMPTY_SQUARE; board_[H8] = EMPTY_SQUARE;
            board_[F8] = 'r';          board_[G8] = 'k';
            state_.blackKingSquare = G8;
            break;
        case MOVE_WHITE_CASTLE_LONG:
            board_[E1] = EMPTY_SQUARE; board_[A1] = EMPTY_SQUARE;
            board_[D1] = 'R';          board_[C1] = 'K';
            state_.whiteKingSquare = C1;
            break;
        case MOVE_BLACK_CASTLE_LONG:
            board_[E8] = EMPTY_SQUARE; board_[A8] = EMPTY_SQUARE;
            board_[D8] = 'r';          board_[C8] = 'k';
            state_.blackKingSquare = C8;
            break;

        case MOVE_PROMOTE_QUEEN:
        case MOVE_PROMOTE_ROOK:
        case MOVE_PROMOTE_BISHOP:
        case MOVE_PROMOTE_KNIGHT: {
            static const char PROMOTED[4] = {'Q', 'R', 'B', 'N'};
            char piece        = PROMOTED[move.type - MOVE_PROMOTE_QUEEN];
            board_[move.from] = EMPTY_SQUARE;
            board_[move.to]   = white ? piece : tolower(piece);
            break;
        }

        case MOVE_WHITE_DOUBLE_PUSH:
            board_[move.from]      = EMPTY_SQUARE;
            board_[move.to]        = 'P';
            state_.enPassantSquare = move.to + 8;
            break;
        case MOVE_BLACK_DOUBLE_PUSH:
            board_[move.from]      = EMPTY_SQUARE;
            board_[move.to]        = 'p';
            state_.enPassantSquare = move.to - 8;
            break;

        case MOVE_WHITE_EN_PASSANT:
            board_[move.from]   = EMPTY_SQUARE;
            board_[move.to]     = 'P';
            board_[move.to + 8] = EMPTY_SQUARE;
            break;
        case MOVE_BLACK_EN_PASSANT:
            board_[move.from]   = EMPTY_SQUARE;
            board_[move.to]     = 'p';
            board_[move.to - 8] = EMPTY_SQUARE;
            break;

        case MOVE_NORMAL:
        default:
            board_[move.to]   = board_[move.from];
            board_[move.from] = EMPTY_SQUARE;
            break;
    }

    whiteToMove_ = !whiteToMove_;
}

void ChessEngine::unmakeMove(const Move& move) {
    state_       = undoStack_[--undoStackCount_];
    whiteToMove_ = !whiteToMove_;
    const bool white = whiteToMove_;

    switch (move.type) {
        case MOVE_WHITE_CASTLE_SHORT:
            board_[E1] = 'K'; board_[F1] = EMPTY_SQUARE; board_[G1] = EMPTY_SQUARE; board_[H1] = 'R';
            return;
        case MOVE_BLACK_CASTLE_SHORT:
            board_[E8] = 'k'; board_[F8] = EMPTY_SQUARE; board_[G8] = EMPTY_SQUARE; board_[H8] = 'r';
            return;
        case MOVE_WHITE_CASTLE_LONG:
            board_[A1] = 'R'; board_[C1] = EMPTY_SQUARE; board_[D1] = EMPTY_SQUARE; board_[E1] = 'K';
            return;
        case MOVE_BLACK_CASTLE_LONG:
            board_[A8] = 'r'; board_[C8] = EMPTY_SQUARE; board_[D8] = EMPTY_SQUARE; board_[E8] = 'k';
            return;

        case MOVE_WHITE_EN_PASSANT:
            board_[move.from]   = 'P';
            board_[move.to]     = EMPTY_SQUARE;
            board_[move.to + 8] = 'p';
            return;
        case MOVE_BLACK_EN_PASSANT:
            board_[move.from]   = 'p';
            board_[move.to]     = EMPTY_SQUARE;
            board_[move.to - 8] = 'P';
            return;

        case MOVE_PROMOTE_QUEEN:
        case MOVE_PROMOTE_ROOK:
        case MOVE_PROMOTE_BISHOP:
        case MOVE_PROMOTE_KNIGHT:
            board_[move.from] = white ? 'P' : 'p';
            board_[move.to]   = move.captured;
            return;

        default:
            board_[move.from] = board_[move.to];
            board_[move.to]   = move.captured;
            return;
    }
}

// ---------------------------------------------------------------------------
//  Génération des coups
// ---------------------------------------------------------------------------

bool ChessEngine::isOwnPiece(char piece) const {
    return whiteToMove_ ? isWhitePiece(piece) : isBlackPiece(piece);
}

bool ChessEngine::isEnemyPiece(char piece) const {
    return whiteToMove_ ? isBlackPiece(piece) : isWhitePiece(piece);
}

Square ChessEngine::sideToMoveKing() const {
    return whiteToMove_ ? state_.whiteKingSquare : state_.blackKingSquare;
}

Square ChessEngine::sideNotToMoveKing() const {
    return whiteToMove_ ? state_.blackKingSquare : state_.whiteKingSquare;
}

void ChessEngine::generatePseudoLegalMoves(MoveList& moves) const {
    moves.count = 0;
    for (Square square = 0; square < 64; ++square) {
        char piece = board_[square];
        if (!isOwnPiece(piece)) continue;

        switch (piece & 0xDF) {  // en majuscule
            case 'P': generatePawnMoves(moves, square); break;
            case 'N': generateStepMoves(moves, square, KNIGHT_OFFSETS, 8, MOVE_NORMAL); break;
            case 'B': generateSlidingMoves(moves, square, BISHOP_DIRECTIONS, 4); break;
            case 'R': generateSlidingMoves(moves, square, ROOK_DIRECTIONS, 4); break;
            case 'Q':
                generateSlidingMoves(moves, square, ROOK_DIRECTIONS, 4);
                generateSlidingMoves(moves, square, BISHOP_DIRECTIONS, 4);
                break;
            case 'K':
                generateStepMoves(moves, square, KING_OFFSETS, 8, MOVE_KING);
                generateCastlingMoves(moves, square);
                break;
        }
    }
}

void ChessEngine::generatePawnMoves(MoveList& moves, Square from) const {
    const bool white         = whiteToMove_;
    const int  forward       = white ? 1 : -1;
    const int  startRank     = white ? 1 : 6;
    const int  promotionRank = white ? 6 : 1;  // rangée de départ d'un pion qui va promouvoir
    const int  file          = fileOf(from);
    const int  rank          = rankOf(from);
    const bool promotes      = rank == promotionRank;

    // Prises (dont la prise en passant)
    for (int side = -1; side <= 1; side += 2) {
        if (!isOnBoard(file + side, rank + forward)) continue;
        Square target = squareAt(file + side, rank + forward);
        if (target == state_.enPassantSquare) {
            moves.add(from, target, white ? MOVE_WHITE_EN_PASSANT : MOVE_BLACK_EN_PASSANT,
                      white ? 'p' : 'P');
        } else if (isEnemyPiece(board_[target])) {
            if (promotes) {
                for (MoveType promotion : PROMOTION_ORDER) moves.add(from, target, promotion, board_[target]);
            } else {
                moves.add(from, target, MOVE_NORMAL, board_[target]);
            }
        }
    }

    // Avances d'une ou deux cases
    Square oneStep = squareAt(file, rank + forward);
    if (board_[oneStep] != EMPTY_SQUARE) return;
    if (promotes) {
        for (MoveType promotion : PROMOTION_ORDER) moves.add(from, oneStep, promotion, EMPTY_SQUARE);
        return;
    }
    moves.add(from, oneStep, MOVE_NORMAL, EMPTY_SQUARE);

    if (rank == startRank) {
        Square twoSteps = squareAt(file, rank + 2 * forward);
        if (board_[twoSteps] == EMPTY_SQUARE) {
            moves.add(from, twoSteps, white ? MOVE_WHITE_DOUBLE_PUSH : MOVE_BLACK_DOUBLE_PUSH,
                      EMPTY_SQUARE);
        }
    }
}

void ChessEngine::generateStepMoves(MoveList& moves, Square from, const int8_t (*offsets)[2],
                                    int count, MoveType type) const {
    const int file = fileOf(from);
    const int rank = rankOf(from);
    for (int i = 0; i < count; ++i) {
        int targetFile = file + offsets[i][0];
        int targetRank = rank + offsets[i][1];
        if (!isOnBoard(targetFile, targetRank)) continue;

        Square target = squareAt(targetFile, targetRank);
        char   piece  = board_[target];
        if (piece == EMPTY_SQUARE || isEnemyPiece(piece)) moves.add(from, target, type, piece);
    }
}

void ChessEngine::generateSlidingMoves(MoveList& moves, Square from,
                                       const int8_t (*directions)[2], int count) const {
    for (int i = 0; i < count; ++i) {
        int targetFile = fileOf(from) + directions[i][0];
        int targetRank = rankOf(from) + directions[i][1];
        while (isOnBoard(targetFile, targetRank)) {
            Square target = squareAt(targetFile, targetRank);
            char   piece  = board_[target];
            if (piece == EMPTY_SQUARE) {
                moves.add(from, target, MOVE_NORMAL, EMPTY_SQUARE);
            } else {
                if (isEnemyPiece(piece)) moves.add(from, target, MOVE_NORMAL, piece);
                break;
            }
            targetFile += directions[i][0];
            targetRank += directions[i][1];
        }
    }
}

void ChessEngine::generateCastlingMoves(MoveList& moves, Square kingSquare) const {
    const uint8_t rights = state_.castlingRights;

    if (kingSquare == E1) {
        if ((rights & CASTLE_WHITE_SHORT) && board_[F1] == EMPTY_SQUARE &&
            board_[G1] == EMPTY_SQUARE && board_[H1] == 'R' && !isSquareAttacked(E1, false) &&
            !isSquareAttacked(F1, false) && !isSquareAttacked(G1, false)) {
            moves.add(E1, G1, MOVE_WHITE_CASTLE_SHORT, EMPTY_SQUARE);
        }
        if ((rights & CASTLE_WHITE_LONG) && board_[B1] == EMPTY_SQUARE &&
            board_[C1] == EMPTY_SQUARE && board_[D1] == EMPTY_SQUARE && board_[A1] == 'R' &&
            !isSquareAttacked(E1, false) && !isSquareAttacked(D1, false) &&
            !isSquareAttacked(C1, false)) {
            moves.add(E1, C1, MOVE_WHITE_CASTLE_LONG, EMPTY_SQUARE);
        }
    } else if (kingSquare == E8) {
        if ((rights & CASTLE_BLACK_SHORT) && board_[F8] == EMPTY_SQUARE &&
            board_[G8] == EMPTY_SQUARE && board_[H8] == 'r' && !isSquareAttacked(E8, true) &&
            !isSquareAttacked(F8, true) && !isSquareAttacked(G8, true)) {
            moves.add(E8, G8, MOVE_BLACK_CASTLE_SHORT, EMPTY_SQUARE);
        }
        if ((rights & CASTLE_BLACK_LONG) && board_[B8] == EMPTY_SQUARE &&
            board_[C8] == EMPTY_SQUARE && board_[D8] == EMPTY_SQUARE && board_[A8] == 'r' &&
            !isSquareAttacked(E8, true) && !isSquareAttacked(D8, true) &&
            !isSquareAttacked(C8, true)) {
            moves.add(E8, C8, MOVE_BLACK_CASTLE_LONG, EMPTY_SQUARE);
        }
    }
}

// ---------------------------------------------------------------------------
//  Attaques, échec, mat, pat
// ---------------------------------------------------------------------------

bool ChessEngine::isSquareAttacked(Square square, bool byWhite) const {
    const int  file = fileOf(square);
    const int  rank = rankOf(square);
    const char pawn   = byWhite ? 'P' : 'p';
    const char knight = byWhite ? 'N' : 'n';
    const char bishop = byWhite ? 'B' : 'b';
    const char rook   = byWhite ? 'R' : 'r';
    const char queen  = byWhite ? 'Q' : 'q';
    const char king   = byWhite ? 'K' : 'k';

    // Pions : un pion blanc attaque vers le haut, donc il se trouve une rangée en dessous
    const int pawnRank = byWhite ? rank - 1 : rank + 1;
    for (int side = -1; side <= 1; side += 2) {
        if (isOnBoard(file + side, pawnRank) && board_[squareAt(file + side, pawnRank)] == pawn)
            return true;
    }

    for (const auto& offset : KNIGHT_OFFSETS) {
        int f = file + offset[0], r = rank + offset[1];
        if (isOnBoard(f, r) && board_[squareAt(f, r)] == knight) return true;
    }
    for (const auto& offset : KING_OFFSETS) {
        int f = file + offset[0], r = rank + offset[1];
        if (isOnBoard(f, r) && board_[squareAt(f, r)] == king) return true;
    }

    auto rayHits = [&](const int8_t (*directions)[2], char slider) {
        for (int i = 0; i < 4; ++i) {
            int f = file + directions[i][0], r = rank + directions[i][1];
            while (isOnBoard(f, r)) {
                char piece = board_[squareAt(f, r)];
                if (piece != EMPTY_SQUARE) {
                    if (piece == slider || piece == queen) return true;
                    break;
                }
                f += directions[i][0];
                r += directions[i][1];
            }
        }
        return false;
    };
    return rayHits(ROOK_DIRECTIONS, rook) || rayHits(BISHOP_DIRECTIONS, bishop);
}

bool ChessEngine::isPieceAttacked(Square square) const {
    return isSquareAttacked(square, isBlackPiece(board_[square]));
}

bool ChessEngine::isSideToMoveInCheck() const {
    return isPieceAttacked(sideToMoveKing());
}

void ChessEngine::generateLegalMoves(MoveList& legalMoves) {
    MoveList pseudoMoves;
    generatePseudoLegalMoves(pseudoMoves);

    legalMoves.count = 0;
    for (int i = 0; i < pseudoMoves.count; ++i) {
        const Move& move = pseudoMoves.moves[i];
        makeMove(move);
        bool leavesKingSafe = !isPieceAttacked(sideNotToMoveKing());
        unmakeMove(move);
        if (leavesKingSafe) legalMoves.moves[legalMoves.count++] = move;
    }
}

// Renvoie false si la position est illégale (le camp qui n'a pas le trait est en échec).
// Sinon, indique si la partie est terminée (mat ou pat).
bool ChessEngine::evaluatePosition(MoveList* moves, GameResult& result) {
    result = GameResult::Ongoing;
    if (isPieceAttacked(sideNotToMoveKing())) return false;

    MoveList localMoves;
    MoveList& pseudoMoves = moves ? *moves : localMoves;
    generatePseudoLegalMoves(pseudoMoves);

    bool hasLegalMove = false;
    for (int i = 0; i < pseudoMoves.count && !hasLegalMove; ++i) {
        makeMove(pseudoMoves.moves[i]);
        hasLegalMove = !isPieceAttacked(sideNotToMoveKing());
        unmakeMove(pseudoMoves.moves[i]);
    }

    if (!hasLegalMove) {
        const bool blackToMove = !whiteToMove_;
        if (isPieceAttacked(sideToMoveKing()))
            result = blackToMove ? GameResult::WhiteWins : GameResult::BlackWins;
        else
            result = blackToMove ? GameResult::StalemateBlackToMove : GameResult::StalemateWhiteToMove;
    }
    return true;
}

void ChessEngine::generateAnnotatedLegalMoves(MoveList& legalMoves, bool* givesCheck,
                                              bool* givesMate, bool* givesStalemate) {
    MoveList pseudoMoves;
    generatePseudoLegalMoves(pseudoMoves);

    legalMoves.count = 0;
    for (int i = 0; i < pseudoMoves.count; ++i) {
        const Move& move = pseudoMoves.moves[i];
        makeMove(move);
        GameResult result;
        bool isLegal  = evaluatePosition(nullptr, result);
        bool checking = isPieceAttacked(sideToMoveKing());
        unmakeMove(move);
        if (!isLegal) continue;

        bool mate = result == GameResult::WhiteWins || result == GameResult::BlackWins;
        bool stalemate = result == GameResult::StalemateWhiteToMove ||
                         result == GameResult::StalemateBlackToMove;
        int index             = legalMoves.count++;
        legalMoves.moves[index] = move;
        givesMate[index]      = mate;
        givesStalemate[index] = stalemate;
        givesCheck[index]     = checking && !mate;
    }
}

// ---------------------------------------------------------------------------
//  Notation algébrique abrégée (SAN)
// ---------------------------------------------------------------------------

bool ChessEngine::formatSan(const Move& move, SanStyle style, char* out, size_t size) const {
    const char piece   = toupper(board_[move.from]);
    const bool capture = move.isCapture();
    const char fromFile = fileChar(move.from), fromRank = rankChar(move.from);
    const char toFile   = fileChar(move.to),   toRank   = rankChar(move.to);

    switch (style) {
        case SanStyle::Pawn:
            if (piece != 'P') return false;
            if (capture) snprintf(out, size, "%cx%c%c%s", fromFile, toFile, toRank, promotionSuffix(move.type));
            else         snprintf(out, size, "%c%c%s", toFile, toRank, promotionSuffix(move.type));
            return true;

        case SanStyle::Castle:
            if (move.type == MOVE_WHITE_CASTLE_SHORT || move.type == MOVE_BLACK_CASTLE_SHORT) {
                snprintf(out, size, "O-O");
                return true;
            }
            if (move.type == MOVE_WHITE_CASTLE_LONG || move.type == MOVE_BLACK_CASTLE_LONG) {
                snprintf(out, size, "O-O-O");
                return true;
            }
            return false;

        case SanStyle::Piece:
            snprintf(out, size, capture ? "%cx%c%c" : "%c%c%c", piece, toFile, toRank);
            return true;
        case SanStyle::PieceWithFile:
            snprintf(out, size, capture ? "%c%cx%c%c" : "%c%c%c%c", piece, fromFile, toFile, toRank);
            return true;
        case SanStyle::PieceWithRank:
            snprintf(out, size, capture ? "%c%cx%c%c" : "%c%c%c%c", piece, fromRank, toFile, toRank);
            return true;
        case SanStyle::PieceWithSquare:
            snprintf(out, size, capture ? "%c%c%cx%c%c" : "%c%c%c%c%c", piece, fromFile, fromRank,
                     toFile, toRank);
            return true;
    }
    return false;
}

std::string ChessEngine::toSan(const Move& move) {
    MoveList legalMoves;
    bool     givesCheck[MAX_MOVES];
    bool     givesMate[MAX_MOVES];
    bool     givesStalemate[MAX_MOVES];
    generateAnnotatedLegalMoves(legalMoves, givesCheck, givesMate, givesStalemate);

    int index = -1;
    for (int i = 0; i < legalMoves.count; ++i) {
        if (legalMoves.moves[i] == move) {
            index = i;
            break;
        }
    }
    if (index < 0) return "--";

    // Du plus court au plus explicite : on garde la première écriture non ambiguë
    static const SanStyle STYLES[] = {
        SanStyle::Pawn,          SanStyle::Castle,        SanStyle::Piece,
        SanStyle::PieceWithFile, SanStyle::PieceWithRank, SanStyle::PieceWithSquare,
    };

    char text[16] = "--";
    for (SanStyle style : STYLES) {
        if (!formatSan(move, style, text, sizeof(text))) continue;

        bool needsUniqueness = style == SanStyle::Piece || style == SanStyle::PieceWithFile ||
                               style == SanStyle::PieceWithRank;
        if (!needsUniqueness) break;

        int  sameText = 0;
        char other[16];
        for (int i = 0; i < legalMoves.count; ++i) {
            if (formatSan(legalMoves.moves[i], style, other, sizeof(other)) && strcmp(other, text) == 0)
                ++sameText;
        }
        if (sameText == 1) break;
    }

    std::string san = text;
    if (givesMate[index])       san += '#';
    else if (givesCheck[index]) san += '+';
    return san;
}
