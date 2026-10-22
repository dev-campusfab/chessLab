#pragma once
#include "chess_types.h"

const char* pieceSymbol(PieceType type);       // "K", "Q", "R", "B", "N", "P" ou "."
const char* pieceNameFrench(PieceType type);   // "Roi", "Dame", ...
const char* pieceNameEnglish(PieceType type);  // "King", "Queen", ...
char        pieceFenChar(PieceType type, PieceColor color);  // minuscule pour les noirs

void printPieceRoles();
