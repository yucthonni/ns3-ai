#ifndef NR_SIONNA_AI_H
#define NR_SIONNA_AI_H
#include <cstdint>

struct UeObs {
    uint16_t rnti;
    double sinr;
};

struct UeAct {
    double txPower;
};
#endif
