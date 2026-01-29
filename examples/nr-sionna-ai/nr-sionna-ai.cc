#include "nr-sionna-ai.h"
#include "ns3/core-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/ai-module.h"
#include "ns3/traces-channel-model.h"
#include "ns3/applications-module.h"
#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NrSionnaAiMinimal");

static Ns3AiMsgInterfaceImpl<UeObs, UeAct>* g_msgInterface = nullptr;
static Ptr<NrUePhy> g_uePhy;

// --- 工具函数：读取 Trace 位置 ---
std::vector<Vector> ReadPos(std::string f) {
    std::vector<Vector> p; std::ifstream file(f); std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line); std::string v; std::vector<double> c;
        while (std::getline(ss, v, ',')) c.push_back(std::stod(v));
        if (c.size() >= 3) p.push_back(Vector(c[0], c[1], c[2]));
    }
    return p;
}

// --- AI 交互回调 ---
void OnSinrReport(uint16_t cellId, uint16_t rnti, double sinr, uint16_t bwpId) {
    if (!g_msgInterface) return;
    
    // 发送观测
    g_msgInterface->CppSendBegin();
    auto& obs = g_msgInterface->GetCpp2PyVector()->at(0);
    obs.rnti = rnti; obs.sinr = sinr;
    g_msgInterface->CppSendEnd();

    // 接收动作
    g_msgInterface->CppRecvBegin();
    auto& act = g_msgInterface->GetPy2CppVector()->at(0);
    g_uePhy->SetTxPower(act.txPower);
    g_msgInterface->CppRecvEnd();

    NS_LOG_UNCOND("AI-Sync: SINR=" << sinr << " -> Set TxPower=" << act.txPower);
}

int main(int argc, char *argv[]) {
    // 1. AI 接口初始化
    auto interface = Ns3AiMsgInterface::Get();
    interface->SetIsMemoryCreator(false);
    interface->SetUseVector(true);
    interface->SetHandleFinish(true);
    g_msgInterface = interface->GetInterface<UeObs, UeAct>();

    // 2. 节点与 Sionna 轨迹加载
    std::string path = "contrib/nr/utils/channels/trace-based/Scenarios/Etoile/";
    NodeContainer gnb, ue; gnb.Create(1); ue.Create(1);
    MobilityHelper mobility; mobility.SetMobilityModel("ns3::WaypointMobilityModel");
    mobility.Install(gnb); mobility.Install(ue);

    auto gPos = ReadPos(path + "Output/Ns3/NodesPosition/device0.csv");
    if (!gPos.empty()) gnb.Get(0)->GetObject<MobilityModel>()->SetPosition(gPos[0]);
    auto uPos = ReadPos(path + "Output/Ns3/NodesPosition/device1.csv");
    auto* ueMob = dynamic_cast<WaypointMobilityModel*>(PeekPointer(ue.Get(0)->GetObject<MobilityModel>()));
    for (size_t i = 0; i < uPos.size(); ++i) ueMob->AddWaypoint(Waypoint(MilliSeconds(100 * i), uPos[i]));

    // 3. NR 协议栈与 Sionna 信道
    auto nrHelper = CreateObject<NrHelper>();
    auto epcHelper = CreateObject<NrPointToPointEpcHelper>();
    nrHelper->SetEpcHelper(epcHelper);

    Ptr<TracesChannelModel> traces = CreateObject<TracesChannelModel>("contrib/nr/utils/channels/trace-based/Scenarios/", "Etoile");
    Config::SetDefault("ns3::TracesSpectrumPropagationLossModel::ChannelModel", PointerValue(traces));
    auto channelHelper = CreateObject<NrChannelHelper>();
    channelHelper->ConfigureSpectrumFactory(TracesSpectrumPropagationLossModel::GetTypeId());
    
    CcBwpCreator ccBwpCreator;
    auto band = ccBwpCreator.CreateOperationBandContiguousCc({28e9, 100e6, 1});
    channelHelper->AssignChannelsToBands({band}, NrChannelHelper::INIT_FADING);
    auto bwp = CcBwpCreator::GetAllBwps({band});

    // 优化：简化波束扫描
    Config::SetDefault("ns3::CellScanBeamforming::UseAngularScanning", BooleanValue(true));
    Config::SetDefault("ns3::CellScanBeamforming::TxAzimuthStep", DoubleValue(20.0));
    Config::SetDefault("ns3::CellScanBeamforming::RxAzimuthStep", DoubleValue(20.0));

    NetDeviceContainer gDev = nrHelper->InstallGnbDevice(gnb, bwp);
    NetDeviceContainer uDev = nrHelper->InstallUeDevice(ue, bwp);
    g_uePhy = uDev.Get(0)->GetObject<NrUeNetDevice>()->GetPhy(0);

    // 4. 网络层与应用 (触发流量)
    InternetStackHelper internet; internet.Install(ue);
    auto uIp = epcHelper->AssignUeIpv4Address(uDev);
    nrHelper->AttachToClosestGnb(uDev, gDev);

    UdpServerHelper server(1234); server.Install(ue);
    UdpClientHelper client(uIp.GetAddress(0), 1234);
    client.SetAttribute("Interval", TimeValue(MilliSeconds(20)));
    client.Install(epcHelper->GetPgwNode());

    // 5. 挂载 AI 钩子
    g_uePhy->TraceConnectWithoutContext("DlDataSinr", MakeCallback(&OnSinrReport));

    Simulator::Stop(Seconds(0.5));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
