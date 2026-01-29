#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/nr-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/grid-scenario-helper.h"
#include "ns3/ideal-beamforming-algorithm.h"
#include "ns3/antenna-module.h"
#include "ns3/ai-module.h"

#include "abr-ai-interface.h"
#include "dash-client-app.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("5gAbrSim");

int main(int argc, char* argv[]) {
    // 1. 仿真参数
    uint16_t gNbNum = 1;
    uint16_t ueNum = 1;
    bool logging = true;
    bool useAi = true;
    bool isMemoryCreator = true; 
    
    // Default Bitrates: 20Mbps, 40Mbps, 80Mbps
    uint32_t bitrate1 = 20000000;
    uint32_t bitrate2 = 40000000;
    uint32_t bitrate3 = 80000000;

    CommandLine cmd;
    cmd.AddValue("logging", "Enable logging", logging);
    cmd.AddValue("useAi", "Enable AI interface", useAi);
    cmd.AddValue("isMemoryCreator", "Is this process the shared memory creator?", isMemoryCreator);
    cmd.AddValue("bitrate1", "Bitrate for Level 1 (bps)", bitrate1);
    cmd.AddValue("bitrate2", "Bitrate for Level 2 (bps)", bitrate2);
    cmd.AddValue("bitrate3", "Bitrate for Level 3 (bps)", bitrate3);
    cmd.Parse(argc, argv);

    if (logging) {
        LogComponentEnable("DashClientApp", LOG_LEVEL_INFO);
        // LogComponentEnable("DashServerController", LOG_LEVEL_INFO); // Enable if needed
        LogComponentEnable("5gAbrSim", LOG_LEVEL_INFO);
        LogComponentDisable("IdealBeamformingHelper", LOG_LEVEL_ALL);
        LogComponentDisable("NrChannel", LOG_LEVEL_ALL);
        LogComponentDisable("ThreeGppSpectrumPropagationLossModel", LOG_LEVEL_ALL);
    }

    // 2. 初始化 AI 接口
    Ns3AiMsgInterfaceImpl<AbrObservation, AbrAction>* aiInterface = nullptr;
    if (useAi) {
        auto interface = Ns3AiMsgInterface::Get();
        interface->SetIsMemoryCreator(isMemoryCreator); 
        interface->SetUseVector(false);       
        aiInterface = interface->GetInterface<AbrObservation, AbrAction>();
    }

    // 3. 创建 5G 节点
    GridScenarioHelper gridScenario;
    gridScenario.SetRows(1);
    gridScenario.SetColumns(gNbNum);
    gridScenario.SetHorizontalBsDistance(10.0);
    gridScenario.SetVerticalBsDistance(10.0);
    gridScenario.SetBsHeight(10.0);
    gridScenario.SetUtHeight(1.5);
    gridScenario.SetSectorization(GridScenarioHelper::SINGLE);
    gridScenario.SetBsNumber(gNbNum);
    gridScenario.SetUtNumber(ueNum);
    gridScenario.SetScenarioHeight(3); 
    gridScenario.SetScenarioLength(3);
    gridScenario.CreateScenario();

    NodeContainer gnbNodes = gridScenario.GetBaseStations();
    NodeContainer ueNodes = gridScenario.GetUserTerminals();

    // 4. 配置 NR 协议栈
    Ptr<NrPointToPointEpcHelper> epcHelper = CreateObject<NrPointToPointEpcHelper>();
    Ptr<IdealBeamformingHelper> idealBeamformingHelper = CreateObject<IdealBeamformingHelper>();
    idealBeamformingHelper->SetAttribute("BeamformingMethod", TypeIdValue(DirectPathBeamforming::GetTypeId()));

    Ptr<NrHelper> nrHelper = CreateObject<NrHelper>();
    nrHelper->SetBeamformingHelper(idealBeamformingHelper);
    nrHelper->SetEpcHelper(epcHelper);

    epcHelper->SetAttribute("S1uLinkDelay", TimeValue(MilliSeconds(0)));

    nrHelper->SetGnbAntennaAttribute("NumRows", UintegerValue(4));
    nrHelper->SetGnbAntennaAttribute("NumColumns", UintegerValue(8));
    nrHelper->SetGnbAntennaAttribute("AntennaElement", PointerValue(CreateObject<IsotropicAntennaModel>()));
    nrHelper->SetGnbPhyAttribute("TxPower", DoubleValue(40.0));

    nrHelper->SetUeAntennaAttribute("NumRows", UintegerValue(2));
    nrHelper->SetUeAntennaAttribute("NumColumns", UintegerValue(4));
    nrHelper->SetUeAntennaAttribute("AntennaElement", PointerValue(CreateObject<IsotropicAntennaModel>()));
    nrHelper->SetUePhyAttribute("TxPower", DoubleValue(23.0));

    // 频谱配置
    CcBwpCreator ccBwpCreator;
    const uint8_t numCcPerBand = 1;
    CcBwpCreator::SimpleOperationBandConf bandConf(28e9, 100e6, numCcPerBand);
    OperationBandInfo band = ccBwpCreator.CreateOperationBandContiguousCc(bandConf);

    Ptr<NrChannelHelper> channelHelper = CreateObject<NrChannelHelper>();
    channelHelper->ConfigureFactories("UMa", "Default", "ThreeGpp");
    channelHelper->AssignChannelsToBands({band});

    // 安装设备
    NetDeviceContainer gnbNetDev = nrHelper->InstallGnbDevice(gnbNodes, CcBwpCreator::GetAllBwps({band}));
    NetDeviceContainer ueNetDev = nrHelper->InstallUeDevice(ueNodes, CcBwpCreator::GetAllBwps({band}));
    
    int64_t randomStream = 1;
    nrHelper->AssignStreams(gnbNetDev, randomStream);
    nrHelper->AssignStreams(ueNetDev, randomStream);

    // 5. 互联网与路由
    auto [remoteHost, remoteHostIpv4Address] = epcHelper->SetupRemoteHost("10Gb/s", 1500, MilliSeconds(10));
    Ipv4Address serverAddress = remoteHost->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();
    
    InternetStackHelper internet;
    internet.Install(ueNodes);

    Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueNetDev));
    nrHelper->AttachToClosestGnb(ueNetDev, gnbNetDev);

    // 6. 安装应用
    uint16_t videoPort = 9000;
    uint16_t controlPort = 8080;

    // --- Server Side (RemoteHost) ---
    // A. Traffic Generator (OnOffApplication)
    // Sends TO the Client (UE IP)
    OnOffHelper onOffHelper("ns3::UdpSocketFactory", InetSocketAddress(ueIpIface.GetAddress(0), videoPort));
    onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate(bitrate1))); // Start with bitrate1
    onOffHelper.SetAttribute("PacketSize", UintegerValue(1450));
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]")); // Continuous stream (CBR)
    
    ApplicationContainer serverApps = onOffHelper.Install(remoteHost);
    Ptr<OnOffApplication> onOffApp = DynamicCast<OnOffApplication>(serverApps.Get(0));
    serverApps.Start(Seconds(1.0)); // Traffic start

    // B. Controller (Listens for rate updates)
    Ptr<DashServerController> serverController = CreateObject<DashServerController>();
    serverController->Setup(onOffApp, controlPort);
    remoteHost->AddApplication(serverController);
    serverController->SetStartTime(Seconds(0.1));

    // --- Client Side (UE) ---
    // DashClientApp (Sink + Controller)
    Ptr<DashClientApp> clientApp = CreateObject<DashClientApp>();
    clientApp->Setup(serverAddress, controlPort, videoPort, aiInterface);
    clientApp->SetBitrates({bitrate1, bitrate2, bitrate3});
    ueNodes.Get(0)->AddApplication(clientApp);
    clientApp->SetStartTime(Seconds(1.0));
    
    // 7. 运行仿真
    
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    
    Simulator::Stop(Seconds(15.0));
    Simulator::Run();
    
    // --- 统计数据输出 ---
    std::cout << "\n--------------------------------------------------" << std::endl;
    std::cout << "              5G ABR Simulation Report            " << std::endl;
    std::cout << "--------------------------------------------------" << std::endl;

    // 1. 网络层指标 (FlowMonitor)
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    
    double totalThroughput = 0.0;
    double avgDelay = 0.0;
    double avgJitter = 0.0;
    uint32_t flowCount = 0;

    for (auto const& [flowId, flowStats] : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);
        
        // Filter relevant flows (Video or Control)
        bool isVideo = (t.destinationPort == videoPort);
        bool isControl = (t.destinationPort == controlPort);

        if (isVideo || isControl) { 
             double throughput = flowStats.rxBytes * 8.0 / 15.0 / 1e6; // Mbps
             double delay = flowStats.rxPackets > 0 ? flowStats.delaySum.GetSeconds() / flowStats.rxPackets * 1000 : 0; // ms
             double jitter = flowStats.rxPackets > 0 ? flowStats.jitterSum.GetSeconds() / flowStats.rxPackets * 1000 : 0; // ms
             
             std::string dir;
             if (isVideo) dir = "Video Downlink (Server -> Client)";
             else if (isControl) dir = "Control Uplink (Client -> Server)";
             else dir = "Unknown";

             std::cout << "[Flow " << flowId << "] " << dir << ":" << std::endl;
             std::cout << "  Throughput: " << throughput << " Mbps" << std::endl;
             std::cout << "  Avg Delay:  " << delay << " ms" << std::endl;
             std::cout << "  Avg Jitter: " << jitter << " ms" << std::endl;
             std::cout << "  Rx Packets: " << flowStats.rxPackets << std::endl;
             std::cout << "  Tx Packets: " << flowStats.txPackets << std::endl;
             
             if (isVideo) { // Only count video for Summary
                 totalThroughput += throughput;
                 avgDelay += delay;
                 avgJitter += jitter;
                 flowCount++;
             }
        }
    }

    if (flowCount > 0) {
        std::cout << "\n[Network Summary (Video)]" << std::endl;
        std::cout << "  Avg DL Throughput: " << totalThroughput / flowCount << " Mbps" << std::endl;
        std::cout << "  Avg DL Latency:    " << avgDelay / flowCount << " ms" << std::endl;
        std::cout << "  Avg DL Jitter:     " << avgJitter / flowCount << " ms" << std::endl;
    }

    // 2. 应用层指标 (DashClient)
    std::cout << "\n[Application Summary]" << std::endl;
    auto appStats = clientApp->GetStats();
    std::cout << "  Total Intervals:   " << appStats.totalChunks << std::endl;
    std::cout << "  Avg Bitrate:       " << appStats.avgBitrateMbps << " Mbps" << std::endl;
    std::cout << "  Quality Switches:  " << appStats.qualitySwitches << std::endl;
    std::cout << "  Rebuffering Time:  " << appStats.totalRebufferingTimeSec << " s" << std::endl;
    std::cout << "--------------------------------------------------" << std::endl;

    Simulator::Destroy();

    return 0;
}
