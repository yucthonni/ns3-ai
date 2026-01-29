#ifndef ABR_AI_INTERFACE_H
#define ABR_AI_INTERFACE_H

#include <cstdint>

// 状态空间: 包含Buffer状态、吞吐量以及预留给后续RTT分布和GapProbe的字段
struct AbrObservation {
    float bufferLevelSec;      // 当前缓冲区时长 (秒)
    float lastChunkThroughput; // 上一个块的平均吞吐量 (Mbps)
    float rttMeanMs;           // 平均RTT (ms) - 预留
    float rttVarMs;            // RTT方差 - 预留
    float probeLossRate;       // GapProbe丢包率 - 预留
    uint32_t chunkIndex;       // 当前请求的是第几个Chunk
};

// 动作空间: 选择下一个Chunk的码率
struct AbrAction {
    uint8_t bitrateIndex;      // 码率档位索引 (例如 0=Low, 1=Medium, 2=High)
};

#endif
