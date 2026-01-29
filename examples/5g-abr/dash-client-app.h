#ifndef DASH_CLIENT_APP_H
#define DASH_CLIENT_APP_H

#include "ns3/application.h"
#include "ns3/socket.h"
#include "ns3/address.h"
#include "ns3/traced-callback.h"
#include "ns3/event-id.h"
#include "ns3/onoff-application.h"
#include "abr-ai-interface.h"
#include "ns3/ai-module.h"

namespace ns3 {

// -------------------------------------------------------------------------
// DashServerController: Controls the OnOffApplication (Bitrate) remotely
// -------------------------------------------------------------------------
class DashServerController : public Application {
public:
    static TypeId GetTypeId (void);
    DashServerController();
    virtual ~DashServerController();

    // Link to the OnOffApplication we want to control
    void Setup(Ptr<OnOffApplication> app, uint16_t controlPort);

protected:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

private:
    void HandleControlRead(Ptr<Socket> socket);
    
    Ptr<OnOffApplication> m_onOffApp;
    uint16_t m_controlPort;
    Ptr<Socket> m_controlSocket; // UDP listener for commands
};

// -------------------------------------------------------------------------
// DashClientApp: Sink + ABR Logic
// -------------------------------------------------------------------------
class DashClientApp : public Application
{
public:
    static TypeId GetTypeId(void);
    DashClientApp();
    virtual ~DashClientApp();

    void Setup(Address serverControlAddr, uint16_t serverControlPort, uint16_t videoPort, Ns3AiMsgInterfaceImpl<AbrObservation, AbrAction>* aiInterface);
    void SetBitrates(const std::vector<uint32_t>& bitrates);
    
    // Stats for reporting
    struct DashStats {
        double avgBitrateMbps;
        uint32_t qualitySwitches;
        double totalRebufferingTimeSec;
        uint32_t totalChunks;
    };
    DashStats GetStats() const;

protected:
    virtual void StartApplication(void);
    virtual void StopApplication(void);

private:
    void HandleVideoRead(Ptr<Socket> socket);
    void ControlLoop(); // Runs periodically (e.g. 1s) to simulate chunk logic
    void SendRateUpdate(uint32_t bitrate); // Send UDP command to ServerController

    // Sockets
    Ptr<Socket> m_videoSinkSocket; // Receives OnOffApplication traffic
    Ptr<Socket> m_controlSocket;   // Sends commands
    
    Address m_serverControlAddr;
    uint16_t m_serverControlPort;
    uint16_t m_videoPort;

    // AI & ABR
    Ns3AiMsgInterfaceImpl<AbrObservation, AbrAction>* m_aiInterface;
    std::vector<uint32_t> m_bitrates;
    uint8_t m_currentBitrateIndex;
    uint8_t m_lastBitrateIndex;

    // Timers & State
    EventId m_controlEvent;
    
    // Measurement State
    uint64_t m_bytesReceivedTotal;
    uint64_t m_bytesReceivedLastInterval;
    Time m_lastIntervalTime;
    
    // Playback State
    float m_bufferLevel; 
    bool m_isRebuffering;
    Time m_lastRebufferingStart;
    
    // Stats
    uint64_t m_totalBitrateSum; 
    uint32_t m_totalChunks;
    uint32_t m_qualitySwitches;
    Time m_totalRebufferingTime;
};

} // namespace ns3

#endif
