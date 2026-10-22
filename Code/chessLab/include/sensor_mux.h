#pragma once
#include <stdint.h>

// Sélection d'un capteur Hall par décodeur (colonne) + multiplexeur (rangée)
// et lecture filtrée de sa tension sur l'ADC.
class SensorMux {
public:
    static SensorMux& instance();

    uint16_t readSquare(int file, int rank);

private:
    SensorMux();

    void selectFile(int file);
    void selectRank(int rank);
};
