#include "dash-client-app.h"
#include "ns3/log.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/simulator.h"
#include "ns3/double.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE("DashClientApp");
NS_OBJECT_ENSURE_REGISTERED(DashClientApp);
NS_OBJECT_ENSURE_REGISTERED(DashServerController);

// =========================================================================
// DashServerController Implementation
// =========================================================================

TypeId DashServerController::GetTypeId(void) {
    static TypeId tid = TypeId("ns3::DashServerController")
        .SetParent<Application>()
        .SetGroupName("Applications")
        .AddConstructor<DashServerController>();
    return tid;
}

DashServerController::DashServerController() : m_onOffApp(nullptr), m_controlPort(0), m_controlSocket(nullptr) {}
DashServerController::~DashServerController() {}

void DashServerController::Setup(Ptr<OnOffApplication> app, uint16_t controlPort) {
    m_onOffApp = app;
    m_controlPort = controlPort;
}

void DashServerController::StartApplication(void) {
    if (!m_controlSocket) {
        m_controlSocket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        m_controlSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), m_controlPort));
        m_controlSocket->SetRecvCallback(MakeCallback(&DashServerController::HandleControlRead, this));
    }
    NS_LOG_INFO("DashServerController: Listening for commands on port " << m_controlPort);
}

void DashServerController::StopApplication(void) {
    if (m_controlSocket) {
        m_controlSocket->Close();
        m_controlSocket = nullptr;
    }
}

void DashServerController::HandleControlRead(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    while ((packet = socket->Recv())) {
        if (packet->GetSize() >= 4) {
            uint32_t newBitrate = 0;
            packet->CopyData((uint8_t*)&newBitrate, 4);
            
            if (m_onOffApp) {
                // Update OnOffApplication DataRate
                // OnOffApplication uses "DataRate" attribute.
                NS_LOG_INFO("DashServerController: Changing Bitrate to " << newBitrate << " bps");
                m_onOffApp->SetAttribute("DataRate", DataRateValue(DataRate(newBitrate)));
            }
        }
    }
}

// =========================================================================
// DashClientApp Implementation
// =========================================================================

TypeId DashClientApp::GetTypeId(void) {
    static TypeId tid = TypeId("ns3::DashClientApp")
        .SetParent<Application>()
        .SetGroupName("Applications")
        .AddConstructor<DashClientApp>();
    return tid;
}

DashClientApp::DashClientApp() 
    : m_videoSinkSocket(nullptr), m_controlSocket(nullptr),
      m_serverControlPort(0), m_videoPort(0),
      m_aiInterface(nullptr),
      m_bitrates({1000000}), 
      m_currentBitrateIndex(0), m_lastBitrateIndex(0),
      m_bytesReceivedTotal(0), m_bytesReceivedLastInterval(0),
      m_bufferLevel(0.0), m_isRebuffering(false),
      m_totalBitrateSum(0), m_totalChunks(0), m_qualitySwitches(0),
      m_totalRebufferingTime(Seconds(0)) {}

DashClientApp::~DashClientApp() {}

void DashClientApp::Setup(Address serverControlAddr, uint16_t serverControlPort, uint16_t videoPort, Ns3AiMsgInterfaceImpl<AbrObservation, AbrAction>* aiInterface) {
    m_serverControlAddr = serverControlAddr;
    m_serverControlPort = serverControlPort;
    m_videoPort = videoPort;
    m_aiInterface = aiInterface;
}

void DashClientApp::SetBitrates(const std::vector<uint32_t>& bitrates) {
    m_bitrates = bitrates;
}

void DashClientApp::StartApplication(void) {
    // 1. Setup Video Sink (Listen for Data)
    if (!m_videoSinkSocket) {
        m_videoSinkSocket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        m_videoSinkSocket->Bind(InetSocketAddress(Ipv4Address::GetAny(), m_videoPort));
        m_videoSinkSocket->SetRecvCallback(MakeCallback(&DashClientApp::HandleVideoRead, this));
    }

    // 2. Setup Control Socket (Sender)
    if (!m_controlSocket) {
        m_controlSocket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
        // destination set per packet
    }

    m_lastIntervalTime = Simulator::Now();
    
    // Start Loop (Simulating chunks/intervals)
    // First interval: 1s wait, then measure and act
    m_controlEvent = Simulator::Schedule(Seconds(1.0), &DashClientApp::ControlLoop, this);
    
    // Set initial bitrate (send command immediately)
    Simulator::Schedule(Seconds(0.1), &DashClientApp::SendRateUpdate, this, m_bitrates[0]);
    NS_LOG_INFO("DashClientApp: Started. Listening on " << m_videoPort);
}

void DashClientApp::StopApplication(void) {
    if (m_videoSinkSocket) { m_videoSinkSocket->Close(); m_videoSinkSocket = nullptr; }
    if (m_controlSocket) { m_controlSocket->Close(); m_controlSocket = nullptr; }
    Simulator::Cancel(m_controlEvent);
}

void DashClientApp::HandleVideoRead(Ptr<Socket> socket) {
    Ptr<Packet> packet;
    while ((packet = socket->Recv())) {
        uint32_t sz = packet->GetSize();
        m_bytesReceivedTotal += sz;
        m_bytesReceivedLastInterval += sz;
    }
}

void DashClientApp::ControlLoop() {
    Time now = Simulator::Now();
    double duration = (now - m_lastIntervalTime).GetSeconds();
    if (duration <= 0) duration = 1.0;

    // Calculate throughput in this interval
    double throughputMbps = (m_bytesReceivedLastInterval * 8.0) / duration / 1e6;
    m_bytesReceivedLastInterval = 0;
    m_lastIntervalTime = now;

    // Logic: Buffer Consumption
    // Simple logic: We consume 1s of buffer every 1s of real time.
    // We add buffer based on how much data we got vs how much a chunk of that bitrate "costs".
    // Cost of 1s of video at CurrentBitrate = CurrentBitrate bits.
    // BytesReceived * 8 / CurrentBitrate = Seconds of video received.
    
    double currentRateBits = (double)m_bitrates[m_currentBitrateIndex];
    if (currentRateBits > 0) {
        double videoSecondsReceived = (throughputMbps * 1e6 * duration) / currentRateBits;
        m_bufferLevel += videoSecondsReceived;
    }
    
    // Drain 1s (playback)
    m_bufferLevel -= duration;
    if (m_bufferLevel < 0) {
        m_bufferLevel = 0;
        if (!m_isRebuffering) {
            m_isRebuffering = true;
            m_lastRebufferingStart = now;
        }
    } else {
        if (m_isRebuffering) {
            m_isRebuffering = false;
            m_totalRebufferingTime += (now - m_lastRebufferingStart);
        }
    }

    NS_LOG_INFO("Time: " << now.GetSeconds() << "s | Throughput: " << throughputMbps << " Mbps | Buffer: " << m_bufferLevel << "s | BitrateIdx: " << (int)m_currentBitrateIndex);

    // AI Decision
    if (m_aiInterface) {
        AbrObservation obs;
        obs.bufferLevelSec = m_bufferLevel;
        obs.lastChunkThroughput = throughputMbps;
        obs.chunkIndex = m_totalChunks;
        
        // Mock Decision: Pick random or cycle, or just stick to one for test
        // For overload test, let's try to adapt logic:
        // If throughput < currentRate, lower quality? No, AI handles that.
        // Mock: Always index 0 (Lowest) or Max? User said "overload".
        // Let's stick to Index 0 for consistency unless changed manually.
        // Actually, let's simulate a simple "Greedy" logic here for testing if no Python:
        // if throughput > next_rate, go up.
        
        // Use Mock: Default 0
        // m_currentBitrateIndex = 0; 
        
        // Or keep current (set via SendRateUpdate initially)
    }
    
    // Metrics
    m_totalBitrateSum += m_bitrates[m_currentBitrateIndex];
    m_totalChunks++;
    
    if (m_currentBitrateIndex != m_lastBitrateIndex) {
        m_qualitySwitches++;
        SendRateUpdate(m_bitrates[m_currentBitrateIndex]);
        m_lastBitrateIndex = m_currentBitrateIndex;
    }

    // Schedule next loop
    m_controlEvent = Simulator::Schedule(Seconds(1.0), &DashClientApp::ControlLoop, this);
}

void DashClientApp::SendRateUpdate(uint32_t bitrate) {
    if (m_controlSocket) {
        Ptr<Packet> p = Create<Packet>((uint8_t*)&bitrate, sizeof(bitrate));
        m_controlSocket->SendTo(p, 0, InetSocketAddress(Ipv4Address::ConvertFrom(m_serverControlAddr), m_serverControlPort));
        NS_LOG_INFO("DashClientApp: Sent Rate Update: " << bitrate << " bps");
    }
}

DashClientApp::DashStats DashClientApp::GetStats() const {
    DashStats stats;
    stats.totalChunks = m_totalChunks;
    stats.qualitySwitches = m_qualitySwitches;
    stats.totalRebufferingTimeSec = m_totalRebufferingTime.GetSeconds();
    if (m_isRebuffering) {
        stats.totalRebufferingTimeSec += (Simulator::Now() - m_lastRebufferingStart).GetSeconds();
    }
    // Avg bitrate of "decisions" made
    stats.avgBitrateMbps = (m_totalChunks > 0) ? (double)m_totalBitrateSum / m_totalChunks / 1e6 : 0.0;
    return stats;
}

} // namespace ns3