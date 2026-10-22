#include "piece_names.h"

#include <Arduino.h>
#include <ctype.h>

const char* pieceSymbol(PieceType type) {
    switch (type) {
        case PieceType::Pawn:   return "P";
        case PieceType::Knight: return "N";
        case PieceType::Bishop: return "B";
        case PieceType::Rook:   return "R";
        case PieceType::Queen:  return "Q";
        case PieceType::King:   return "K";
        default:                return ".";
    }
}

const char* pieceNameFrench(PieceType type) {
    switch (type) {
        case PieceType::Pawn:   return "Pion";
        case PieceType::Knight: return "Cavalier";
        case PieceType::Bishop: return "Fou";
        case PieceType::Rook:   return "Tour";
        case PieceType::Queen:  return "Dame";
        case PieceType::King:   return "Roi";
        default:                return "-";
    }
}

const char* pieceNameEnglish(PieceType type) {
    switch (type) {
        case PieceType::Pawn:   return "Pawn";
        case PieceType::Knight: return "Knight";
        case PieceType::Bishop: return "Bishop";
        case PieceType::Rook:   return "Rook";
        case PieceType::Queen:  return "Queen";
        case PieceType::King:   return "King";
        default:                return "-";
    }
}

char pieceFenChar(PieceType type, PieceColor color) {
    if (type == PieceType::None || color == PieceColor::None) return ' ';
    char symbol = pieceSymbol(type)[0];
    return color == PieceColor::Black ? tolower(symbol) : symbol;
}

void printPieceRoles() {
    static const PieceType ROLES[] = {
        PieceType::King, PieceType::Queen,  PieceType::Rook,
        PieceType::Bishop, PieceType::Knight, PieceType::Pawn,
    };

    char text[512];
    int  length = snprintf(text, sizeof(text), "\nRoles des pieces:\n");
    for (PieceType type : ROLES) {
        const char* note = type == PieceType::Knight ? " pour eviter la confusion avec le King" : "";
        length += snprintf(text + length, sizeof(text) - length, "%s : %s (symbole : %s%s)\n",
                           pieceNameFrench(type), pieceNameEnglish(type), pieceSymbol(type), note);
    }
    Serial.print(text);
    Serial.flush();
}
