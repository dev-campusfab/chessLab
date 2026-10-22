#pragma once
#include <stdint.h>

// Broches du circuit d'alimentation : sortie de coupure et bouton marche/arrêt.
class PowerSwitch {
public:
    static PowerSwitch& instance();

    void keepPowerOn();
    void cutPower();
    bool isButtonPressed() const;

private:
    PowerSwitch();
};

// Extinction sur appui long du bouton ou après une longue inactivité.
class PowerManager {
public:
    static PowerManager& instance();

    void begin();
    void update();
    void resetInactivityTimer();
    void shutdown();

private:
    PowerManager() = default;

    void checkLongPress();
    void checkInactivity();

    uint32_t lastActivityMs_     = 0;
    uint32_t buttonPressStartMs_ = 0;
    bool     buttonWasPressed_   = false;
};
