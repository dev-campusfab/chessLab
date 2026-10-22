#include "power_manager.h"

#include <Arduino.h>
#include "config.h"

// ---------------------------------------------------------------------------
//  PowerSwitch
// ---------------------------------------------------------------------------

PowerSwitch& PowerSwitch::instance() {
    static PowerSwitch powerSwitch;
    return powerSwitch;
}

PowerSwitch::PowerSwitch() {
    pinMode(POWER_OFF_PIN, OUTPUT);
    pinMode(POWER_BUTTON_PIN, INPUT_PULLUP);
    keepPowerOn();
}

void PowerSwitch::keepPowerOn() { digitalWrite(POWER_OFF_PIN, LOW); }
void PowerSwitch::cutPower()    { digitalWrite(POWER_OFF_PIN, HIGH); }

bool PowerSwitch::isButtonPressed() const { return digitalRead(POWER_BUTTON_PIN) == LOW; }

// ---------------------------------------------------------------------------
//  PowerManager
// ---------------------------------------------------------------------------

PowerManager& PowerManager::instance() {
    static PowerManager manager;
    return manager;
}

void PowerManager::begin() {
    PowerSwitch::instance().keepPowerOn();
    lastActivityMs_ = millis();
}

void PowerManager::update() {
    checkLongPress();
    checkInactivity();
}

void PowerManager::resetInactivityTimer() {
    lastActivityMs_ = millis();
}

void PowerManager::shutdown() {
    PowerSwitch::instance().cutPower();
    delay(100);
}

void PowerManager::checkLongPress() {
    bool pressed = PowerSwitch::instance().isButtonPressed();
    if (pressed) {
        if (!buttonWasPressed_) buttonPressStartMs_ = millis();
        if (millis() - buttonPressStartMs_ > POWER_LONG_PRESS_MS) {
            Serial.print("PowerManager: long press detected, shutting down...\n");
            shutdown();
        }
    }
    buttonWasPressed_ = pressed;
}

void PowerManager::checkInactivity() {
    if (millis() - lastActivityMs_ > INACTIVITY_TIMEOUT_MS) {
        Serial.print("PowerManager: inactivity timeout, shutting down...\n");
        shutdown();
    }
}
