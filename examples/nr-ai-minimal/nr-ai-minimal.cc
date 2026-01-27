/*
 * 基于 contrib/nr/examples/cttc-nr-demo.cc 修改
 * 添加了 ns3-ai 模块挂载
 */

#include "ns3/antenna-module.h"
#include "ns3/applications-module.h"
#include "ns3/buildings-module.h"
#include "ns3/config-store-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/point-to-point-module.h"

// --- AI Module Includes ---
#include "nr-ai-minimal.h"
#include "ns3/ai-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NrAiMinimalDemo");

// --- AI Global Variables ---
static Ns3AiMsgInterfaceImpl<UeObservation, UeAction>* g_msgInterface = nullptr;
// 我们需要保存 UE Phy 指针以便在回调中修改参数
// 在这个简单示例中，我们只控制第一个 UE
static Ptr<NrUePhy> g_targetUePhy; 

// --- AI Callback ---
void OnSinrReport(uint16_t cellId, uint16_t rnti, double sinr, uint16_t bwpId)
{
    // 安全检查
    if (!g_msgInterface) return;
    auto* cpp2py = g_msgInterface->GetCpp2PyVector();
    auto* py2cpp = g_msgInterface->GetPy2CppVector();
    if (!cpp2py || !py2cpp || cpp2py->empty() || py2cpp->empty()) return;

    // 1. 发送观测 (RNTI, SINR)
    g_msgInterface->CppSendBegin();
    auto& obs = cpp2py->at(0);
    obs.rnti = rnti;
    obs.sinr = sinr;
    g_msgInterface->CppSendEnd();

    // 2. 接收动作 (TxPower) - 阻塞等待 Python
    g_msgInterface->CppRecvBegin();
    auto& act = py2cpp->at(0);
    
    // 3. 执行动作
    if (g_targetUePhy) {
        g_targetUePhy->SetTxPower(act.txPower);
    }
    g_msgInterface->CppRecvEnd();

    NS_LOG_UNCOND("AI-Loop: RNTI=" << rnti << " SINR=" << sinr << " -> Set TxPower=" << act.txPower);
}

int main(int argc, char* argv[])
{
    // --- AI Interface Init ---
    auto interface = Ns3AiMsgInterface::Get();
    interface->SetIsMemoryCreator(false);
    interface->SetUseVector(true);
    interface->SetHandleFinish(true);
    g_msgInterface = interface->GetInterface<UeObservation, UeAction>();

    // --- Standard NR Simulation Setup (from cttc-nr-demo.cc) ---
    uint16_t gNbNum = 1;
    uint16_t ueNumPergNb = 2;
    bool logging = false;
    bool doubleOperationalBand = false; // Simplified to single band

    // Traffic parameters
    uint32_t udpPacketSizeBe = 1252;
    uint32_t lambdaBe = 1000;

    Time simTime = MilliSeconds(1000);
    Time udpAppStartTime = MilliSeconds(400);

    // NR parameters
    uint16_t numerologyBwp1 = 4;
    double centralFrequencyBand1 = 28e9;
    double bandwidthBand1 = 50e6;
    double totalTxPower = 35;

    CommandLine cmd(__FILE__);
    cmd.AddValue("logging", "Enable logging", logging);
    cmd.Parse(argc, argv);

    if (logging)
    {
        LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
        LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    }

    Config::SetDefault("ns3::NrRlcUm::MaxTxBufferSize", UintegerValue(999999999));

    int64_t randomStream = 1;
    GridScenarioHelper gridScenario;
    gridScenario.SetRows(1);
    gridScenario.SetColumns(gNbNum);
    gridScenario.SetHorizontalBsDistance(10.0);
    gridScenario.SetVerticalBsDistance(10.0);
    gridScenario.SetBsHeight(10);
    gridScenario.SetUtHeight(1.5);
    gridScenario.SetSectorization(GridScenarioHelper::SINGLE);
    gridScenario.SetBsNumber(gNbNum);
    gridScenario.SetUtNumber(ueNumPergNb * gNbNum);
    gridScenario.SetScenarioHeight(3);
    gridScenario.SetScenarioLength(3);
    randomStream += gridScenario.AssignStreams(randomStream);
    gridScenario.CreateScenario();

    NodeContainer ueVoiceContainer;
    for (uint32_t j = 0; j < gridScenario.GetUserTerminals().GetN(); ++j)
    {
        ueVoiceContainer.Add(gridScenario.GetUserTerminals().Get(j));
    }

    Ptr<NrPointToPointEpcHelper> nrEpcHelper = CreateObject<NrPointToPointEpcHelper>();
    Ptr<IdealBeamformingHelper> idealBeamformingHelper = CreateObject<IdealBeamformingHelper>();
    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();

    nrHelper->SetBeamformingHelper(idealBeamformingHelper);
    nrHelper->SetEpcHelper(nrEpcHelper);

    CcBwpCreator ccBwpCreator;
    const uint8_t numCcPerBand = 1;
    CcBwpCreator::SimpleOperationBandConf bandConf1(centralFrequencyBand1, bandwidthBand1, numCcPerBand);
    OperationBandInfo band1 = ccBwpCreator.CreateOperationBandContiguousCc(bandConf1);

    Ptr<NrChannelHelper> channelHelper = CreateObject<NrChannelHelper>();
    channelHelper->ConfigureFactories("UMi", "Default", "ThreeGpp");
    channelHelper->SetChannelConditionModelAttribute("UpdatePeriod", TimeValue(MilliSeconds(0)));
    channelHelper->SetPathlossAttribute("ShadowingEnabled", BooleanValue(false));
    channelHelper->AssignChannelsToBands({band1});
    
    BandwidthPartInfoPtrVector allBwps = CcBwpCreator::GetAllBwps({band1});

    // Beamforming method
    idealBeamformingHelper->SetAttribute("BeamformingMethod", TypeIdValue(DirectPathBeamforming::GetTypeId()));
    nrEpcHelper->SetAttribute("S1uLinkDelay", TimeValue(MilliSeconds(0)));

    // Antennas
    nrHelper->SetUeAntennaAttribute("NumRows", UintegerValue(2));
    nrHelper->SetUeAntennaAttribute("NumColumns", UintegerValue(4));
    nrHelper->SetUeAntennaAttribute("AntennaElement", PointerValue(CreateObject<IsotropicAntennaModel>()));

    nrHelper->SetGnbAntennaAttribute("NumRows", UintegerValue(4));
    nrHelper->SetGnbAntennaAttribute("NumColumns", UintegerValue(8));
    nrHelper->SetGnbAntennaAttribute("AntennaElement", PointerValue(CreateObject<IsotropicAntennaModel>()));

    // BWP Manager
    uint32_t bwpIdForVoice = 0;
    nrHelper->SetGnbBwpManagerAlgorithmAttribute("GBR_CONV_VOICE", UintegerValue(bwpIdForVoice));
    nrHelper->SetUeBwpManagerAlgorithmAttribute("GBR_CONV_VOICE", UintegerValue(bwpIdForVoice));

    // Install Devices
    NetDeviceContainer gnbNetDev = nrHelper->InstallGnbDevice(gridScenario.GetBaseStations(), allBwps);
    NetDeviceContainer ueVoiceNetDev = nrHelper->InstallUeDevice(ueVoiceContainer, allBwps);

    randomStream += nrHelper->AssignStreams(gnbNetDev, randomStream);
    randomStream += nrHelper->AssignStreams(ueVoiceNetDev, randomStream);

    nrHelper->GetGnbPhy(gnbNetDev.Get(0), 0)->SetAttribute("Numerology", UintegerValue(numerologyBwp1));
    nrHelper->GetGnbPhy(gnbNetDev.Get(0), 0)->SetAttribute("TxPower", DoubleValue(totalTxPower));

    // IP Stack
    auto [remoteHost, remoteHostIpv4Address] = nrEpcHelper->SetupRemoteHost("100Gb/s", 2500, Seconds(0.000));
    InternetStackHelper internet;
    internet.Install(gridScenario.GetUserTerminals());
    Ipv4InterfaceContainer ueVoiceIpIface = nrEpcHelper->AssignUeIpv4Address(NetDeviceContainer(ueVoiceNetDev));

    // Attach
    nrHelper->AttachToClosestGnb(ueVoiceNetDev, gnbNetDev);

    // --- AI HOOK: Connect Trace Source ---
    // 获取第一个 UE 的 Phy 指针，保存到全局变量，并连接 Trace
    Ptr<NrUeNetDevice> targetUeDevice = DynamicCast<NrUeNetDevice>(ueVoiceNetDev.Get(0));
    g_targetUePhy = targetUeDevice->GetPhy(0);
    // 连接 SINR 报告回调
    g_targetUePhy->TraceConnectWithoutContext("DlDataSinr", MakeCallback(&OnSinrReport));
    // -------------------------------------

    // Application (UDP)
    uint16_t dlPortVoice = 1235;
    ApplicationContainer serverApps;
    UdpServerHelper dlPacketSinkVoice(dlPortVoice);
    serverApps.Add(dlPacketSinkVoice.Install(ueVoiceContainer));

    UdpClientHelper dlClientVoice;
    dlClientVoice.SetAttribute("MaxPackets", UintegerValue(0xFFFFFFFF));
    dlClientVoice.SetAttribute("PacketSize", UintegerValue(udpPacketSizeBe));
    dlClientVoice.SetAttribute("Interval", TimeValue(Seconds(1.0 / lambdaBe)));

    NrEpsBearer voiceBearer(NrEpsBearer::GBR_CONV_VOICE);
    Ptr<NrEpcTft> voiceTft = Create<NrEpcTft>();
    NrEpcTft::PacketFilter dlpfVoice;
    dlpfVoice.localPortStart = dlPortVoice;
    dlpfVoice.localPortEnd = dlPortVoice;
    voiceTft->Add(dlpfVoice);

    ApplicationContainer clientApps;
    for (uint32_t i = 0; i < ueVoiceContainer.GetN(); ++i)
    {
        Ptr<Node> ue = ueVoiceContainer.Get(i);
        Ptr<NetDevice> ueDevice = ueVoiceNetDev.Get(i);
        Address ueAddress = ueVoiceIpIface.GetAddress(i);

        dlClientVoice.SetAttribute("Remote", AddressValue(addressUtils::ConvertToSocketAddress(ueAddress, dlPortVoice)));
        clientApps.Add(dlClientVoice.Install(remoteHost));
        nrHelper->ActivateDedicatedEpsBearer(ueDevice, voiceBearer, voiceTft);
    }

    serverApps.Start(udpAppStartTime);
    clientApps.Start(udpAppStartTime);
    serverApps.Stop(simTime);
    clientApps.Stop(simTime);

    // Run Simulation
    Simulator::Stop(simTime);
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}