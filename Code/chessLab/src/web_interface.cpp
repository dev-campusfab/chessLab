#include "web_interface.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <WiFi.h>

#include "app_state.h"
#include "chess_game.h"
#include "piece_names.h"
#include "start_position.h"
#include "web_page.h"

static WebServer        httpServer(HTTP_PORT);
static WebSocketsServer webSocket(WEBSOCKET_PORT);

// Codes d'état d'une case, interprétés par la page web
namespace SquareCode {
constexpr char Empty   = 'E';
constexpr char Piece   = 'P';  // mise en place : pièce bien posée
constexpr char Missing = 'M';  // mise en place : pièce manquante
constexpr char Bad     = 'B';  // case à corriger après un coup illégal
constexpr char Check   = 'H';  // roi en échec
constexpr char Capture = 'C';
constexpr char Legal   = 'L';
}  // namespace SquareCode

namespace {

const char* statusMessage() {
    if (AppState::instance().phase() == GamePhase::Setup)
        return "Placez les pieces sur leur position de depart.";

    const ChessGame& game = ChessGame::instance();
    switch (game.interactionState()) {
        case InteractionState::IllegalMove:
            return "Coup illegal ! Remettez la piece a sa case d'origine.";
        case InteractionState::ImpossibleState:
            return "Etat impossible ! Remettez toutes les pieces a leur position precedente.";
        case InteractionState::PieceLifted:
            return "Piece en main : cyan = coup normal, orange = prise.";
        default:
            return game.isSideToMoveInCheck() ? "Echec !" : "Partie en cours.";
    }
}

char squareCode(int file, int rank, const uint8_t occupancy[BOARD_SIZE][BOARD_SIZE]) {
    if (AppState::instance().phase() == GamePhase::Setup) {
        if (!StartPosition::instance().hasPieceAt(file, rank)) return SquareCode::Empty;
        return occupancy[file][rank] ? SquareCode::Piece : SquareCode::Missing;
    }

    const ChessGame&       game  = ChessGame::instance();
    const InteractionState state = game.interactionState();
    if (state == InteractionState::IllegalMove)
        return game.isSquareMismatched(file, rank) ? SquareCode::Bad : SquareCode::Empty;
    if (game.isKingInCheckAt(file, rank)) return SquareCode::Check;
    if (state != InteractionState::PieceLifted) return SquareCode::Empty;
    if (game.isCaptureTarget(file, rank)) return SquareCode::Capture;
    if (game.isLegalTarget(file, rank)) return SquareCode::Legal;
    return SquareCode::Empty;
}

char squarePiece(int file, int rank) {
    if (AppState::instance().phase() == GamePhase::Setup) {
        const StartPosition::SquareContent& content = StartPosition::instance().at(file, rank);
        return pieceFenChar(content.type, content.color);
    }
    return ChessGame::instance().pieceAt(file, rank);
}

void onWebSocketEvent(uint8_t client, WStype_t type, uint8_t* payload, size_t length) {
    if (type == WStype_CONNECTED)         Serial.printf("Client web connecte (#%u).\n", client);
    else if (type == WStype_DISCONNECTED) Serial.printf("Client web deconnecte (#%u).\n", client);
}

}  // namespace

WebInterface& WebInterface::instance() {
    static WebInterface web;
    return web;
}

void WebInterface::begin() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Connexion WiFi en cours...\n");
    Serial.flush();

    wl_status_t status = WiFi.status();
    for (int attempt = 0; attempt < WIFI_CONNECT_ATTEMPTS && status != WL_CONNECTED; ++attempt) {
        delay(WIFI_CONNECT_RETRY_MS);
        status = WiFi.status();
    }

    char message[80];
    if (status != WL_CONNECTED) {
        snprintf(message, sizeof(message), "WiFi indisponible (code %d), interface web desactivee.\n",
                 status);
        Serial.print(message);
        Serial.flush();
        return;
    }

    wifiConnected_ = true;
    IPAddress ip   = WiFi.localIP();
    snprintf(message, sizeof(message), "WiFi connecte, IP: %d.%d.%d.%d\n", ip[0], ip[1], ip[2], ip[3]);
    Serial.print(message);
    Serial.flush();

    httpServer.on("/", []() { httpServer.send(200, "text/html", INDEX_HTML); });
    httpServer.begin();
    webSocket.begin();
    webSocket.onEvent(onWebSocketEvent);
}

void WebInterface::update(const uint8_t occupancy[BOARD_SIZE][BOARD_SIZE]) {
    if (!wifiConnected_) return;

    httpServer.handleClient();
    webSocket.loop();

    uint32_t now = millis();
    if (now - lastBroadcastMs_ < WEB_BROADCAST_INTERVAL_MS) return;
    lastBroadcastMs_ = now;

    broadcastState(occupancy);
}

// Exemple : {"mode":"playing","impossible":false,"message":"Partie en cours.",
//            "turn":"white","status":"EEE...","pieces":"rnbqkbnr..."}
// status et pieces : 64 caractères, de a8 à h1.
void WebInterface::broadcastState(const uint8_t occupancy[BOARD_SIZE][BOARD_SIZE]) {
    const ChessGame& game = ChessGame::instance();

    JsonDocument doc;
    doc["mode"]       = AppState::instance().phase() == GamePhase::Setup ? "setup" : "playing";
    doc["impossible"] = game.interactionState() == InteractionState::ImpossibleState;
    doc["message"]    = statusMessage();
    doc["turn"]       = game.isWhiteToMove() ? "white" : "black";

    String status, pieces;
    status.reserve(NUM_SQUARES);
    pieces.reserve(NUM_SQUARES);
    for (int rank = BOARD_SIZE - 1; rank >= 0; --rank) {
        for (int file = 0; file < BOARD_SIZE; ++file) {
            status += squareCode(file, rank, occupancy);
            pieces += squarePiece(file, rank);
        }
    }
    doc["status"] = status;
    doc["pieces"] = pieces;

    String json;
    serializeJson(doc, json);
    webSocket.broadcastTXT(json);
}
