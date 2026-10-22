#pragma once

enum class GamePhase : int {
    Setup   = 0,  // on attend que les pièces soient sur leur position de départ
    Playing = 1,
};

class AppState {
public:
    static AppState& instance() {
        static AppState state;
        return state;
    }

    GamePhase phase() const { return phase_; }
    void setPhase(GamePhase phase) { phase_ = phase; }

private:
    AppState() = default;

    GamePhase phase_ = GamePhase::Setup;
};
