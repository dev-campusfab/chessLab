#include "sensor_mux.h"

#include <Arduino.h>
#include "config.h"

SensorMux& SensorMux::instance() {
    static SensorMux mux;
    return mux;
}

SensorMux::SensorMux() {
    static const uint8_t SELECT_PINS[] = {
        DECODER_A0_PIN, DECODER_A1_PIN, DECODER_A2_PIN, MUX_S0_PIN, MUX_S1_PIN, MUX_S2_PIN,
    };
    pinMode(HALL_ANALOG_PIN, INPUT);
    for (uint8_t pin : SELECT_PINS) pinMode(pin, OUTPUT);
}

void SensorMux::selectFile(int file) {
    digitalWrite(DECODER_A0_PIN, file & 1);
    digitalWrite(DECODER_A1_PIN, (file >> 1) & 1);
    digitalWrite(DECODER_A2_PIN, (file >> 2) & 1);
}

void SensorMux::selectRank(int rank) {
    digitalWrite(MUX_S0_PIN, rank & 1);
    digitalWrite(MUX_S1_PIN, (rank >> 1) & 1);
    digitalWrite(MUX_S2_PIN, (rank >> 2) & 1);
}

// Moyenne tronquée : on trie les échantillons et on jette les extrêmes,
// ce qui rend la mesure insensible aux pics causés par le WiFi.
uint16_t SensorMux::readSquare(int file, int rank) {
    selectFile(file);
    selectRank(rank);
    delayMicroseconds(HALL_SETTLE_US);

    int samples[HALL_SAMPLE_COUNT];
    for (int i = 0; i < HALL_SAMPLE_COUNT; ++i) {
        samples[i] = analogRead(HALL_ANALOG_PIN);
        delayMicroseconds(HALL_SAMPLE_INTERVAL_US);
    }

    for (int i = 1; i < HALL_SAMPLE_COUNT; ++i) {
        int value = samples[i];
        int j     = i - 1;
        while (j >= 0 && samples[j] > value) {
            samples[j + 1] = samples[j];
            --j;
        }
        samples[j + 1] = value;
    }

    int sum = 0;
    for (int i = HALL_TRIM_COUNT; i < HALL_SAMPLE_COUNT - HALL_TRIM_COUNT; ++i) sum += samples[i];
    return sum / (HALL_SAMPLE_COUNT - 2 * HALL_TRIM_COUNT);
}
