#ifndef NR_TRACES_AI_H
#define NR_TRACES_AI_H
#include <cstdint>

struct TraceObs {
    uint16_t rnti;
    double sinr;
};

struct TraceAct {
    double txPower;
};
#endif
