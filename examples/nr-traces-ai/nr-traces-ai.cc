#include "nr-traces-ai.h"
#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/ai-module.h"
#include "ns3/traces-channel-model.h"
#include "ns3/nr-point-to-point-epc-helper.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NrTracesAiExample");

static Ns3AiMsgInterfaceImpl<TraceObs, TraceAct>* g_msgInterface = nullptr;
static Ptr<NrUePhy> g_uePhy;
static double g_lastSinr = 0.0; // 缓存最新的 SINR

// --- 回调函数：仅负责更新缓存，不阻塞仿真 ---
void UpdateSinrCache(uint16_t cellId, uint16_t rnti, double sinr, uint16_t bwpId) {
    g_lastSinr = sinr;
}

// --- 工具：读取 Sionna 位置 ---
std::vector<Vector> ReadPositions(std::string filename) {
    std::vector<Vector> positions;
    std::ifstream file(filename);
    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string value;
        std::vector<double> coords;
        while (std::getline(ss, value, ',')) coords.push_back(std::stod(value));
        if (coords.size() >= 3) positions.push_back(Vector(coords[0], coords[1], coords[2]));
    }
    return positions;
}

// --- 周期性 AI 交互 (每100ms) ---
void RunAiLoop() {
    if (!g_msgInterface || !g_uePhy) return;

    // 1. 发送观测 (使用缓存的 SINR)
    g_msgInterface->CppSendBegin();
    auto& obs = g_msgInterface->GetCpp2PyVector()->at(0);
    obs.rnti = 1;
    obs.sinr = g_lastSinr;
    g_msgInterface->CppSendEnd();

    // 2. 接收决策
    g_msgInterface->CppRecvBegin();
    auto& act = g_msgInterface->GetPy2CppVector()->at(0);
    g_uePhy->SetTxPower(act.txPower);
    g_msgInterface->CppRecvEnd();

    NS_LOG_UNCOND("AI-Sync at " << Simulator::Now().GetSeconds() << "s | SINR=" << g_lastSinr << " -> Set TxPower=" << act.txPower);

    // 3. 循环
    Simulator::Schedule(MilliSeconds(100), &RunAiLoop);
}

int main(int argc, char *argv[]) {
    // --- AI 接口初始化 ---
    auto interface = Ns3AiMsgInterface::Get();
    interface->SetIsMemoryCreator(false);
    interface->SetUseVector(true);
    interface->SetHandleFinish(true);
    g_msgInterface = interface->GetInterface<TraceObs, TraceAct>();

    // --- 加载 Sionna 环境 ---
    std::string tracesFolder = "contrib/nr/utils/channels/trace-based/Scenarios/";
    std::string scenarioName = "Etoile";
    
    NodeContainer gnb, ue; gnb.Create(1); ue.Create(1);
    MobilityHelper mobility; mobility.SetMobilityModel("ns3::WaypointMobilityModel");
    mobility.Install(gnb); mobility.Install(ue);

    auto gPos = ReadPositions(tracesFolder + scenarioName + "/Output/Ns3/NodesPosition/device0.csv");
    if (!gPos.empty()) gnb.Get(0)->GetObject<MobilityModel>()->SetPosition(gPos[0]);
    auto uPos = ReadPositions(tracesFolder + scenarioName + "/Output/Ns3/NodesPosition/device1.csv");
    Ptr<WaypointMobilityModel> ueMob = DynamicCast<WaypointMobilityModel>(ue.Get(0)->GetObject<MobilityModel>());
    for (size_t i = 0; i < uPos.size(); ++i) ueMob->AddWaypoint(Waypoint(MilliSeconds(100 * i), uPos[i]));

    // --- NR 协议栈与 Sionna 信道 ---
    auto nrHelper = CreateObject<NrHelper>();
    auto epcHelper = CreateObject<NrPointToPointEpcHelper>();
    nrHelper->SetEpcHelper(epcHelper);

    Ptr<TracesChannelModel> tracesModel = CreateObject<TracesChannelModel>(tracesFolder, scenarioName);
    Config::SetDefault("ns3::TracesSpectrumPropagationLossModel::ChannelModel", PointerValue(tracesModel));
    auto channelHelper = CreateObject<NrChannelHelper>();
    channelHelper->ConfigureSpectrumFactory(TracesSpectrumPropagationLossModel::GetTypeId());
    
    CcBwpCreator ccBwpCreator;
    auto band = ccBwpCreator.CreateOperationBandContiguousCc({28e9, 100e6, 1});
    channelHelper->AssignChannelsToBands({band}, NrChannelHelper::INIT_FADING);
    auto allBwps = CcBwpCreator::GetAllBwps({band});

    // --- 性能优化：波束扫描 ---
    auto idealBfHelper = CreateObject<IdealBeamformingHelper>();
    idealBfHelper->SetAttribute("BeamformingMethod", TypeIdValue(CellScanBeamforming::GetTypeId()));
    idealBfHelper->SetAttribute("BeamformingPeriodicity", TimeValue(MilliSeconds(100)));
    Config::SetDefault("ns3::CellScanBeamforming::UseAngularScanning", BooleanValue(true));
    Config::SetDefault("ns3::CellScanBeamforming::TxAzimuthStep", DoubleValue(10.0));
    Config::SetDefault("ns3::CellScanBeamforming::RxAzimuthStep", DoubleValue(20.0));
    nrHelper->SetBeamformingHelper(idealBfHelper);

    nrHelper->SetGnbAntennaAttribute("NumRows", UintegerValue(8));
    nrHelper->SetGnbAntennaAttribute("NumColumns", UintegerValue(8));
    nrHelper->SetUeAntennaAttribute("NumRows", UintegerValue(2));
    nrHelper->SetUeAntennaAttribute("NumColumns", UintegerValue(4));

    NetDeviceContainer gDevs = nrHelper->InstallGnbDevice(gnb, allBwps);
    NetDeviceContainer uDevs = nrHelper->InstallUeDevice(ue, allBwps);
    g_uePhy = uDevs.Get(0)->GetObject<NrUeNetDevice>()->GetPhy(0);

    // --- 网络与应用 ---
    InternetStackHelper internet; internet.Install(ue);
    auto ueIp = epcHelper->AssignUeIpv4Address(uDevs);
    nrHelper->AttachToClosestGnb(uDevs, gDevs);

    UdpServerHelper server(1234); server.Install(ue);
    UdpClientHelper client(ueIp.GetAddress(0), 1234);
    client.SetAttribute("Interval", TimeValue(MilliSeconds(50))); 
    client.Install(epcHelper->GetPgwNode());

    // --- 关键：连接 Trace 以更新缓存 ---
    g_uePhy->TraceConnectWithoutContext("DlDataSinr", MakeCallback(&UpdateSinrCache));

    // --- 启动 AI 定时器循环 ---
    Simulator::Schedule(MilliSeconds(450), &RunAiLoop);

    NS_LOG_UNCOND("Starting Fast AI-Trace Simulation...");
    Simulator::Stop(Seconds(1.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}