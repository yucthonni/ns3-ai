#ifndef NR_AI_MINIMAL_H
#define NR_AI_MINIMAL_H
#include <cstdint>

struct UeObservation {
    uint16_t rnti;
    double sinr;
};

struct UeAction {
    double txPower;
};
#endif
