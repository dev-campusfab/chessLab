// ============================================================================
//  ChessLab — échiquier connecté (ESP32-C6)
//
//  Capteurs Hall sous chaque case, LED sous chaque case, moteur d'échecs
//  embarqué et interface web temps réel.
//
//  Déroulement :
//    1. Mise en place : les LED guident le placement des pièces
//       (vert = bien posée, rouge = manquante).
//    2. Partie : quand une pièce est soulevée, ses coups légaux s'allument
//       (cyan = déplacement, orange = prise). Le coup est validé quand le
//       plateau correspond à son résultat. Un coup illégal ou un plateau
//       incohérent est signalé en rouge jusqu'à correction.
// ============================================================================

#include <Arduino.h>
#include <string.h>

#include "app_state.h"
#include "chess_game.h"
#include "config.h"
#include "hall_sensor_array.h"
#include "led_matrix.h"
#include "piece_names.h"
#include "power_manager.h"
#include "start_position.h"
#include "web_interface.h"

static uint8_t previousOccupancy[BOARD_SIZE][BOARD_SIZE];

static void initSerial() {
    Serial.setTxBufferSize(SERIAL_TX_BUFFER_SIZE);
    Serial.begin(SERIAL_BAUD_RATE);
    for (int attempt = 0; attempt < SERIAL_WAIT_ATTEMPTS && !Serial; ++attempt) delay(100);
    delay(200);
    Serial.setTimeout(SERIAL_STREAM_TIMEOUT);
}

void setup() {
    PowerManager::instance().begin();
    initSerial();
    Serial.print("ChessLab Starting...\n");
    delay(100);

    LedMatrix::instance().begin();
    printPieceRoles();
    WebInterface::instance().begin();
}

void loop() {
    PowerManager::instance().update();

    HallSensorArray& sensors = HallSensorArray::instance();
    sensors.scanAll();

    uint8_t occupancy[BOARD_SIZE][BOARD_SIZE];
    sensors.getOccupancy(occupancy);

    // Toute pièce déplacée repousse l'extinction automatique
    if (memcmp(occupancy, previousOccupancy, sizeof(occupancy)) != 0) {
        PowerManager::instance().resetInactivityTimer();
        memcpy(previousOccupancy, occupancy, sizeof(occupancy));
    }

    AppState& app = AppState::instance();
    if (app.phase() == GamePhase::Setup) {
        LedMatrix::instance().showSetupGuide(occupancy);
        if (StartPosition::instance().matches(occupancy)) {
            Serial.print("Position de depart correcte, debut de la partie.\n");
            LedMatrix::instance().flashStartConfirmation();
            ChessGame::instance().startGame();
            app.setPhase(GamePhase::Playing);
        }
    } else {
        ChessGame::instance().update(occupancy);
        LedMatrix::instance().showGame();
    }

    sensors.printDebugBoard();
    WebInterface::instance().update(occupancy);
}
