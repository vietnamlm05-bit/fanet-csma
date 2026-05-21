#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>
#include <filesystem>

using namespace ns3;

namespace
{

struct RangeTracker
{
    std::vector<bool> inRange;
    uint64_t exitEvents{0};
    uint64_t samples{0};
    uint64_t activeNodeSamples{0};
    uint64_t outOfRangeSamples{0};
};

struct FlowSnapshot
{
    uint64_t txPackets{0};
    uint64_t rxPackets{0};
    uint64_t rxBytes{0};
    uint64_t rxPayloadBytes{0};
    double delaySumSeconds{0.0};
    uint32_t measuredFlows{0};
};

struct RangeSnapshot
{
    uint64_t exitEvents{0};
    uint64_t activeNodeSamples{0};
    uint64_t outOfRangeSamples{0};
};

struct MilestoneResult
{
    double intervalStart{0.0};
    double intervalEnd{0.0};
    uint32_t activeClients{0};
    uint64_t txPackets{0};
    uint64_t rxPackets{0};
    uint64_t rxBytes{0};
    uint64_t rxPayloadBytes{0};
    uint64_t lostPackets{0};
    double pdr{0.0};
    double lossRate{0.0};
    double offeredLoadMbps{0.0};
    double throughputMbps{0.0};
    double avgDelayMs{0.0};
    uint64_t gatewayRangeExitEvents{0};
    double linkTearingRate{0.0};
    double outOfRangePercent{0.0};
};

struct LaunchConfig
{
    double clientStart{2.0};
    bool staggeredLaunch{true};
    uint32_t dronesPerWave{5};
    double launchInterval{300.0};
};

struct RangeSampleContext
{
    NodeContainer nodes;
    double communicationRange{0.0};
    Time interval;
    LaunchConfig launch;
    RangeTracker* tracker{nullptr};
};

struct MilestoneContext
{
    Ptr<FlowMonitor> monitor;
    Ptr<Ipv4FlowClassifier> classifier;
    Ipv4Address gatewayAddress;
    uint16_t gatewayPort{0};
    uint32_t packetSize{0};
    double packetRate{0.0};
    uint32_t nClients{0};
    LaunchConfig launch;
    RangeTracker* rangeTracker{nullptr};
    FlowSnapshot* previousFlow{nullptr};
    RangeSnapshot* previousRange{nullptr};
    double* previousTime{nullptr};
    std::vector<MilestoneResult>* results{nullptr};
};

std::string
RandomVariableString(const std::string& name, double minValue, double maxValue)
{
    std::ostringstream oss;
    oss << "ns3::" << name << "[Min=" << minValue << "|Max=" << maxValue << "]";
    return oss.str();
}

std::string
ConstantVariableString(double value)
{
    std::ostringstream oss;
    oss << "ns3::ConstantRandomVariable[Constant=" << value << "]";
    return oss.str();
}

std::string
OutputModeName(bool mobile)
{
    return mobile ? "mobile" : "static";
}

void
InstallStaticFanetMobility(NodeContainer nodes, double areaSideMeters)
{
    NodeContainer gateway;
    gateway.Add(nodes.Get(0));

    MobilityHelper gatewayMobility;
    Ptr<ListPositionAllocator> gatewayPosition = CreateObject<ListPositionAllocator>();
    gatewayPosition->Add(Vector(areaSideMeters * 0.5, areaSideMeters * 0.5, 0.0));
    gatewayMobility.SetPositionAllocator(gatewayPosition);
    gatewayMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    gatewayMobility.Install(gateway);

    NodeContainer clients;
    for (uint32_t i = 1; i < nodes.GetN(); ++i)
    {
        clients.Add(nodes.Get(i));
    }

    MobilityHelper clientMobility;
    clientMobility.SetPositionAllocator(
        "ns3::RandomRectanglePositionAllocator",
        "X",
        StringValue(RandomVariableString("UniformRandomVariable", 0.0, areaSideMeters)),
        "Y",
        StringValue(RandomVariableString("UniformRandomVariable", 0.0, areaSideMeters)));
    clientMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    clientMobility.Install(clients);
}

void
InstallMobileFanetMobility(NodeContainer nodes,
                           double areaSideMeters,
                           double minSpeed,
                           double maxSpeed,
                           double pauseSeconds)
{
    NodeContainer gateway;
    gateway.Add(nodes.Get(0));

    MobilityHelper gatewayMobility;
    Ptr<ListPositionAllocator> gatewayPosition = CreateObject<ListPositionAllocator>();
    gatewayPosition->Add(Vector(areaSideMeters * 0.5, areaSideMeters * 0.5, 0.0));
    gatewayMobility.SetPositionAllocator(gatewayPosition);
    gatewayMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    gatewayMobility.Install(gateway);

    NodeContainer clients;
    for (uint32_t i = 1; i < nodes.GetN(); ++i)
    {
        clients.Add(nodes.Get(i));
    }

    ObjectFactory positionFactory;
    positionFactory.SetTypeId("ns3::RandomRectanglePositionAllocator");
    positionFactory.Set("X",
                        StringValue(RandomVariableString("UniformRandomVariable", 0.0, areaSideMeters)));
    positionFactory.Set("Y",
                        StringValue(RandomVariableString("UniformRandomVariable", 0.0, areaSideMeters)));
    Ptr<PositionAllocator> waypointAllocator =
        positionFactory.Create()->GetObject<PositionAllocator>();

    MobilityHelper clientMobility;
    clientMobility.SetPositionAllocator(waypointAllocator);
    clientMobility.SetMobilityModel(
        "ns3::RandomWaypointMobilityModel",
        "Speed",
        StringValue(RandomVariableString("UniformRandomVariable", minSpeed, maxSpeed)),
        "Pause",
        StringValue(ConstantVariableString(pauseSeconds)),
        "PositionAllocator",
        PointerValue(waypointAllocator));
    clientMobility.Install(clients);
}

double
DistanceToGateway(NodeContainer nodes, uint32_t nodeIndex)
{
    Ptr<MobilityModel> gatewayMobility = nodes.Get(0)->GetObject<MobilityModel>();
    Ptr<MobilityModel> nodeMobility = nodes.Get(nodeIndex)->GetObject<MobilityModel>();
    return CalculateDistance(gatewayMobility->GetPosition(), nodeMobility->GetPosition());
}

void
InitializeRangeTracker(NodeContainer nodes, double communicationRange, RangeTracker& tracker)
{
    tracker.inRange.assign(nodes.GetN(), true);
    for (uint32_t i = 1; i < nodes.GetN(); ++i)
    {
        tracker.inRange[i] = DistanceToGateway(nodes, i) <= communicationRange;
    }
}

void
SampleGatewayRange(RangeSampleContext* context)
{
    const double now = Simulator::Now().GetSeconds();
    for (uint32_t i = 1; i < context->nodes.GetN(); ++i)
    {
        const uint32_t clientNumber = i;
        const uint32_t wave =
            context->launch.staggeredLaunch ? (clientNumber - 1) / context->launch.dronesPerWave : 0;
        const double launchTime =
            context->launch.clientStart + (static_cast<double>(wave) * context->launch.launchInterval);
        if (now < launchTime)
        {
            continue;
        }

        const bool nowInRange = DistanceToGateway(context->nodes, i) <= context->communicationRange;
        if (context->tracker->inRange[i] && !nowInRange)
        {
            ++context->tracker->exitEvents;
        }
        if (!nowInRange)
        {
            ++context->tracker->outOfRangeSamples;
        }
        context->tracker->inRange[i] = nowInRange;
        ++context->tracker->activeNodeSamples;
    }

    ++context->tracker->samples;
    Simulator::Schedule(context->interval, &SampleGatewayRange, context);
}

double
GetClientLaunchTime(uint32_t clientNumber,
                    double clientStart,
                    bool staggeredLaunch,
                    uint32_t dronesPerWave,
                    double launchInterval)
{
    if (!staggeredLaunch)
    {
        return clientStart;
    }

    const uint32_t wave = (clientNumber - 1) / dronesPerWave;
    return clientStart + (static_cast<double>(wave) * launchInterval);
}

uint32_t
CountActiveClients(uint32_t nClients,
                   double timeSeconds,
                   double clientStart,
                   bool staggeredLaunch,
                   uint32_t dronesPerWave,
                   double launchInterval)
{
    uint32_t activeClients = 0;
    for (uint32_t clientNumber = 1; clientNumber <= nClients; ++clientNumber)
    {
        if (GetClientLaunchTime(clientNumber, clientStart, staggeredLaunch, dronesPerWave, launchInterval) <=
            timeSeconds)
        {
            ++activeClients;
        }
    }
    return activeClients;
}

RangeSnapshot
GetRangeSnapshot(const RangeTracker& tracker)
{
    RangeSnapshot snapshot;
    snapshot.exitEvents = tracker.exitEvents;
    snapshot.activeNodeSamples = tracker.activeNodeSamples;
    snapshot.outOfRangeSamples = tracker.outOfRangeSamples;
    return snapshot;
}

FlowSnapshot
CollectFlowSnapshot(Ptr<FlowMonitor> monitor,
                    Ptr<Ipv4FlowClassifier> classifier,
                    Ipv4Address gatewayAddress,
                    uint16_t gatewayPort,
                    uint32_t packetSize)
{
    FlowSnapshot snapshot;
    monitor->CheckForLostPackets();

    const auto stats = monitor->GetFlowStats();
    for (const auto& stat : stats)
    {
        const Ipv4FlowClassifier::FiveTuple tuple = classifier->FindFlow(stat.first);
        if (tuple.destinationAddress == gatewayAddress && tuple.destinationPort == gatewayPort &&
            tuple.protocol == 17)
        {
            snapshot.txPackets += stat.second.txPackets;
            snapshot.rxPackets += stat.second.rxPackets;
            snapshot.rxBytes += stat.second.rxBytes;
            snapshot.rxPayloadBytes += stat.second.rxPackets * packetSize;
            snapshot.delaySumSeconds += stat.second.delaySum.GetSeconds();
            ++snapshot.measuredFlows;
        }
    }

    return snapshot;
}

void
RecordMilestone(MilestoneContext* context)
{
    const double now = Simulator::Now().GetSeconds();
    const FlowSnapshot currentFlow =
        CollectFlowSnapshot(context->monitor,
                            context->classifier,
                            context->gatewayAddress,
                            context->gatewayPort,
                            context->packetSize);
    const RangeSnapshot currentRange = GetRangeSnapshot(*context->rangeTracker);
    const double duration = now - *context->previousTime;

    MilestoneResult result;
    result.intervalStart = *context->previousTime;
    result.intervalEnd = now;
    result.activeClients = CountActiveClients(context->nClients,
                                              now,
                                              context->launch.clientStart,
                                              context->launch.staggeredLaunch,
                                              context->launch.dronesPerWave,
                                              context->launch.launchInterval);
    result.txPackets = currentFlow.txPackets - context->previousFlow->txPackets;
    result.rxPackets = currentFlow.rxPackets - context->previousFlow->rxPackets;
    result.rxBytes = currentFlow.rxBytes - context->previousFlow->rxBytes;
    result.rxPayloadBytes = currentFlow.rxPayloadBytes - context->previousFlow->rxPayloadBytes;
    result.lostPackets = result.txPackets >= result.rxPackets ? result.txPackets - result.rxPackets : 0;
    result.pdr =
        result.txPackets > 0 ? (static_cast<double>(result.rxPackets) / result.txPackets) * 100.0 : 0.0;
    result.lossRate = result.txPackets > 0
                          ? (static_cast<double>(result.lostPackets) / result.txPackets) * 100.0
                          : 0.0;
    result.offeredLoadMbps =
        static_cast<double>(result.activeClients) * context->packetSize * 8.0 * context->packetRate /
        1000000.0;
    result.throughputMbps =
        duration > 0.0 ? static_cast<double>(result.rxPayloadBytes) * 8.0 / duration / 1000000.0
                       : 0.0;
    result.avgDelayMs =
        result.rxPackets > 0
            ? ((currentFlow.delaySumSeconds - context->previousFlow->delaySumSeconds) /
               static_cast<double>(result.rxPackets)) *
                  1000.0
            : 0.0;
    result.gatewayRangeExitEvents = currentRange.exitEvents - context->previousRange->exitEvents;
    result.linkTearingRate =
        duration > 0.0 ? static_cast<double>(result.gatewayRangeExitEvents) / duration : 0.0;

    const uint64_t activeNodeSamples =
        currentRange.activeNodeSamples - context->previousRange->activeNodeSamples;
    const uint64_t outOfRangeSamples =
        currentRange.outOfRangeSamples - context->previousRange->outOfRangeSamples;
    result.outOfRangePercent =
        activeNodeSamples > 0 ? (static_cast<double>(outOfRangeSamples) / activeNodeSamples) * 100.0 : 0.0;

    context->results->push_back(result);
    *context->previousFlow = currentFlow;
    *context->previousRange = currentRange;
    *context->previousTime = now;
}

bool
CsvNeedsHeader(const std::string& csvFile)
{
    std::ifstream existing(csvFile);
    return !existing.good() || existing.peek() == std::ifstream::traits_type::eof();
}

void
EnsureParentDirectory(const std::string& filePath)
{
    if (filePath.empty())
    {
        return;
    }

    const std::filesystem::path path(filePath);
    const std::filesystem::path parent = path.parent_path();
    if (parent.empty())
    {
        return;
    }

    std::error_code error;
    std::filesystem::create_directories(parent, error);
    if (error)
    {
        std::cerr << "Warning: cannot create output directory " << parent.string() << ": "
                  << error.message() << std::endl;
    }
}

std::string
FormatDouble(double value, uint32_t precision = 6)
{
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << value;
    return oss.str();
}

void
PrintSection(const std::string& title)
{
    std::cout << "\n-- " << title << " --\n";
}

void
PrintRow(const std::string& label, const std::string& value, const std::string& unit = "")
{
    std::cout << "  " << std::left << std::setw(34) << label << " : " << std::left
              << std::setw(16) << value;
    if (!unit.empty())
    {
        std::cout << unit;
    }
    std::cout << "\n";
}

} // namespace

int
main(int argc, char* argv[])
{
    uint32_t nNodes = 30;
    bool mobile = true;
    bool enablePcap = false;
    bool writeCsv = true;
    bool writeMilestoneCsv = true;
    bool writeFlowXml = true;
    bool writeNetAnim = false;
    bool staggeredLaunch = true;
    uint32_t rtsCtsThreshold = 2346;
    uint64_t netAnimMaxPackets = 1000000000;
    double netAnimMobilityPollInterval = 0.5;
    uint32_t packetSize = 512;
    double packetRate = 100.0;
    double simTime = 1800.0;
    double serverStart = 1.0;
    double clientStart = 2.0;
    uint32_t dronesPerWave = 5;
    double launchInterval = 300.0;
    double areaSide = 500.0;
    double minSpeed = 5.0;
    double maxSpeed = 15.0;
    double pauseTime = 2.0;
    double communicationRange = 140.0;
    double rangeSampleInterval = 0.5;
    double pathLossExponent = 3.0;
    uint32_t rngRun = 1;
    std::string dataMode = "ErpOfdmRate12Mbps";
    std::string controlMode = "ErpOfdmRate6Mbps";
    std::string csvFile = "results/csv/fanet-csma-file1-results.csv";
    std::string milestoneCsvFile = "results/csv/fanet-csma-file1-milestones.csv";
    std::string flowXmlFile;
    std::string netAnimFile;
    std::string pcapPrefix = "results/pcap/fanet-csma-simulation";

    CommandLine cmd(__FILE__);
    cmd.AddValue("nNodes", "Total nodes in the ad-hoc WiFi network, including gateway node 0", nNodes);
    cmd.AddValue("mobile", "true = mobile FANET scenario, false = optional static baseline", mobile);
    cmd.AddValue("staggeredLaunch", "Launch drones in waves instead of starting all clients at once", staggeredLaunch);
    cmd.AddValue("rtsCtsThreshold", "RTS/CTS threshold in bytes; 2346 disables RTS/CTS for 512-byte packets", rtsCtsThreshold);
    cmd.AddValue("packetSize", "UDP payload size in bytes", packetSize);
    cmd.AddValue("packetRate", "Packets per second sent by each source node", packetRate);
    cmd.AddValue("simTime", "Simulation stop time in seconds", simTime);
    cmd.AddValue("serverStart", "UDP server start time in seconds", serverStart);
    cmd.AddValue("clientStart", "UDP client start time in seconds", clientStart);
    cmd.AddValue("dronesPerWave", "Number of drones launched at each staggered milestone", dronesPerWave);
    cmd.AddValue("launchInterval", "Seconds between staggered drone launch waves", launchInterval);
    cmd.AddValue("areaSide", "Side length of the square FANET area in meters", areaSide);
    cmd.AddValue("minSpeed", "Minimum RandomWaypoint speed for mobile FANET nodes", minSpeed);
    cmd.AddValue("maxSpeed", "Maximum RandomWaypoint speed for mobile FANET nodes", maxSpeed);
    cmd.AddValue("pauseTime", "RandomWaypoint pause time in seconds", pauseTime);
    cmd.AddValue("communicationRange", "Range used to count gateway link exits in the mobile extension", communicationRange);
    cmd.AddValue("rangeSampleInterval", "Interval in seconds for range-exit sampling", rangeSampleInterval);
    cmd.AddValue("pathLossExponent", "Log-distance path loss exponent", pathLossExponent);
    cmd.AddValue("rngRun", "ns-3 RNG run number for repeatable mobile scenarios", rngRun);
    cmd.AddValue("dataMode", "802.11g data mode", dataMode);
    cmd.AddValue("controlMode", "802.11g control mode", controlMode);
    cmd.AddValue("enablePcap", "Enable WiFi pcap traces", enablePcap);
    cmd.AddValue("writeCsv", "Append one summary row to the CSV result file", writeCsv);
    cmd.AddValue("writeMilestoneCsv", "Append interval rows at 300s milestones", writeMilestoneCsv);
    cmd.AddValue("writeFlowXml", "Write the FlowMonitor XML file", writeFlowXml);
    cmd.AddValue("writeNetAnim", "Write an optional NetAnim mobility/packet animation XML file", writeNetAnim);
    cmd.AddValue("netAnimMaxPackets", "Maximum packets per NetAnim trace file", netAnimMaxPackets);
    cmd.AddValue("netAnimMobilityPollInterval", "Seconds between NetAnim mobility samples", netAnimMobilityPollInterval);
    cmd.AddValue("csvFile", "CSV result file path", csvFile);
    cmd.AddValue("milestoneCsvFile", "CSV file path for staggered milestone interval rows", milestoneCsvFile);
    cmd.AddValue("flowXmlFile", "FlowMonitor XML output path; empty uses an automatic name", flowXmlFile);
    cmd.AddValue("netAnimFile", "NetAnim XML output path; empty uses an automatic name", netAnimFile);
    cmd.AddValue("pcapPrefix", "PCAP output prefix when enablePcap=true", pcapPrefix);
    cmd.Parse(argc, argv);

    if (nNodes < 2 || nNodes > 30)
    {
        std::cerr << "Error: Lab 5 requires nNodes from 2 to 30." << std::endl;
        return 1;
    }
    if (packetRate <= 0.0)
    {
        std::cerr << "Error: packetRate must be positive." << std::endl;
        return 1;
    }
    if (packetSize == 0)
    {
        std::cerr << "Error: packetSize must be positive." << std::endl;
        return 1;
    }
    if (serverStart < 0.0 || clientStart < 0.0)
    {
        std::cerr << "Error: serverStart and clientStart must not be negative." << std::endl;
        return 1;
    }
    if (simTime <= serverStart)
    {
        std::cerr << "Error: simTime must be larger than serverStart." << std::endl;
        return 1;
    }
    if (simTime <= clientStart)
    {
        std::cerr << "Error: simTime must be larger than clientStart." << std::endl;
        return 1;
    }
    if (serverStart > clientStart)
    {
        std::cerr << "Error: serverStart should be <= clientStart." << std::endl;
        return 1;
    }
    if (areaSide <= 0.0)
    {
        std::cerr << "Error: areaSide must be positive." << std::endl;
        return 1;
    }
    if (staggeredLaunch && dronesPerWave == 0)
    {
        std::cerr << "Error: dronesPerWave must be positive." << std::endl;
        return 1;
    }
    if (staggeredLaunch && launchInterval <= 0.0)
    {
        std::cerr << "Error: launchInterval must be positive." << std::endl;
        return 1;
    }
    if (communicationRange <= 0.0)
    {
        std::cerr << "Error: communicationRange must be positive." << std::endl;
        return 1;
    }
    if (rangeSampleInterval <= 0.0)
    {
        std::cerr << "Error: rangeSampleInterval must be positive." << std::endl;
        return 1;
    }
    if (netAnimMobilityPollInterval <= 0.0)
    {
        std::cerr << "Error: netAnimMobilityPollInterval must be positive." << std::endl;
        return 1;
    }
    if (pathLossExponent <= 0.0)
    {
        std::cerr << "Error: pathLossExponent must be positive." << std::endl;
        return 1;
    }
    if (minSpeed <= 0.0 || maxSpeed <= 0.0)
    {
        std::cerr << "Error: minSpeed and maxSpeed must be positive." << std::endl;
        return 1;
    }
    if (mobile && minSpeed > maxSpeed)
    {
        std::cerr << "Error: minSpeed must be <= maxSpeed." << std::endl;
        return 1;
    }
    if (pauseTime < 0.0)
    {
        std::cerr << "Error: pauseTime must not be negative." << std::endl;
        return 1;
    }

    RngSeedManager::SetRun(rngRun);

    LaunchConfig launchConfig;
    launchConfig.clientStart = clientStart;
    launchConfig.staggeredLaunch = staggeredLaunch;
    launchConfig.dronesPerWave = dronesPerWave;
    launchConfig.launchInterval = launchInterval;

    const uint32_t nClients = nNodes - 1;
    const bool rtsCtsDisabledForPayload = rtsCtsThreshold >= packetSize;
    const double offeredLoadMbps =
        static_cast<double>(nClients) * packetSize * 8.0 * packetRate / 1000000.0;

    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold",
                       UintegerValue(rtsCtsThreshold));
    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue(controlMode));

    NodeContainer nodes;
    nodes.Create(nNodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue(dataMode),
                                 "ControlMode",
                                 StringValue(controlMode),
                                 "RtsCtsThreshold",
                                 UintegerValue(rtsCtsThreshold));

    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::LogDistancePropagationLossModel",
                               "Exponent",
                               DoubleValue(pathLossExponent),
                               "ReferenceDistance",
                               DoubleValue(1.0),
                               "ReferenceLoss",
                               DoubleValue(40.046));

    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    if (mobile)
    {
        InstallMobileFanetMobility(nodes, areaSide, minSpeed, maxSpeed, pauseTime);
    }
    else
    {
        InstallStaticFanetMobility(nodes, areaSide);
    }

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);
    const Ipv4Address gatewayAddress = interfaces.GetAddress(0);

    const uint16_t gatewayPort = 9;
    UdpServerHelper server(gatewayPort);
    ApplicationContainer serverApps = server.Install(nodes.Get(0));
    serverApps.Start(Seconds(serverStart));
    serverApps.Stop(Seconds(simTime));

    ApplicationContainer clientApps;
    for (uint32_t i = 1; i < nNodes; ++i)
    {
        const double launchTime = GetClientLaunchTime(i,
                                                       clientStart,
                                                       staggeredLaunch,
                                                       dronesPerWave,
                                                       launchInterval);
        if (launchTime >= simTime)
        {
            continue;
        }

        const uint32_t maxPackets = std::ceil((simTime - launchTime) * packetRate);
        UdpClientHelper client(gatewayAddress, gatewayPort);
        client.SetAttribute("MaxPackets", UintegerValue(maxPackets));
        client.SetAttribute("Interval", TimeValue(Seconds(1.0 / packetRate)));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer app = client.Install(nodes.Get(i));
        app.Start(Seconds(launchTime));
        app.Stop(Seconds(simTime));
        clientApps.Add(app);
    }

    std::unique_ptr<AnimationInterface> animation;
    if (writeNetAnim)
    {
        if (netAnimFile.empty())
        {
            std::ostringstream name;
            name << "results/netanim/fanet-csma-n" << nNodes << '-' << OutputModeName(mobile)
                 << ".netanim.xml";
            netAnimFile = name.str();
        }
        EnsureParentDirectory(netAnimFile);
        animation = std::make_unique<AnimationInterface>(netAnimFile);
        animation->SetStopTime(Seconds(simTime));
        animation->SetMaxPktsPerTraceFile(netAnimMaxPackets);
        animation->SetMobilityPollInterval(Seconds(netAnimMobilityPollInterval));
        animation->UpdateNodeDescription(nodes.Get(0), "Gateway");
        animation->UpdateNodeColor(nodes.Get(0), 0, 120, 255);
        for (uint32_t i = 1; i < nNodes; ++i)
        {
            animation->UpdateNodeDescription(nodes.Get(i), "Drone " + std::to_string(i));
            animation->UpdateNodeColor(nodes.Get(i), 255, 128, 0);
        }
    }

    if (enablePcap)
    {
        EnsureParentDirectory(pcapPrefix);
        phy.EnablePcap(pcapPrefix, devices);
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());

    RangeTracker rangeTracker;
    InitializeRangeTracker(nodes, communicationRange, rangeTracker);
    RangeSampleContext rangeSampleContext;
    rangeSampleContext.nodes = nodes;
    rangeSampleContext.communicationRange = communicationRange;
    rangeSampleContext.interval = Seconds(rangeSampleInterval);
    rangeSampleContext.launch = launchConfig;
    rangeSampleContext.tracker = &rangeTracker;
    Simulator::Schedule(Seconds(0.0), &SampleGatewayRange, &rangeSampleContext);

    std::vector<MilestoneResult> milestoneResults;
    FlowSnapshot previousMilestoneFlow;
    RangeSnapshot previousMilestoneRange = GetRangeSnapshot(rangeTracker);
    double previousMilestoneTime = clientStart;
    MilestoneContext milestoneContext;
    milestoneContext.monitor = monitor;
    milestoneContext.classifier = classifier;
    milestoneContext.gatewayAddress = gatewayAddress;
    milestoneContext.gatewayPort = gatewayPort;
    milestoneContext.packetSize = packetSize;
    milestoneContext.packetRate = packetRate;
    milestoneContext.nClients = nClients;
    milestoneContext.launch = launchConfig;
    milestoneContext.rangeTracker = &rangeTracker;
    milestoneContext.previousFlow = &previousMilestoneFlow;
    milestoneContext.previousRange = &previousMilestoneRange;
    milestoneContext.previousTime = &previousMilestoneTime;
    milestoneContext.results = &milestoneResults;
    if (staggeredLaunch)
    {
        for (double milestone = launchInterval; milestone <= simTime + 1e-9; milestone += launchInterval)
        {
            if (milestone > clientStart)
            {
                Simulator::Schedule(Seconds(milestone), &RecordMilestone, &milestoneContext);
            }
        }
    }

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    const auto stats = monitor->GetFlowStats();

    uint64_t txPackets = 0;
    uint64_t rxPackets = 0;
    uint64_t rxBytes = 0;
    uint64_t rxPayloadBytes = 0;
    double delaySumSeconds = 0.0;
    uint32_t measuredFlows = 0;

    for (const auto& stat : stats)
    {
        const Ipv4FlowClassifier::FiveTuple tuple = classifier->FindFlow(stat.first);
        if (tuple.destinationAddress == gatewayAddress && tuple.destinationPort == gatewayPort &&
            tuple.protocol == 17)
        {
            txPackets += stat.second.txPackets;
            rxPackets += stat.second.rxPackets;
            rxBytes += stat.second.rxBytes;
            rxPayloadBytes += stat.second.rxPackets * packetSize;
            delaySumSeconds += stat.second.delaySum.GetSeconds();
            ++measuredFlows;
        }
    }

    const uint64_t lostPackets = txPackets >= rxPackets ? txPackets - rxPackets : 0;
    const double pdr = txPackets > 0 ? (static_cast<double>(rxPackets) / txPackets) * 100.0 : 0.0;
    const double lossRate =
        txPackets > 0 ? (static_cast<double>(lostPackets) / txPackets) * 100.0 : 0.0;
    const double throughputMbps =
        static_cast<double>(rxPayloadBytes) * 8.0 / simTime / 1000000.0;
    const double avgDelayMs =
        rxPackets > 0 ? (delaySumSeconds / static_cast<double>(rxPackets)) * 1000.0 : 0.0;
    const double linkTearingRate =
        simTime > 0.0 ? static_cast<double>(rangeTracker.exitEvents) / simTime : 0.0;
    const double outOfRangePercent =
        rangeTracker.activeNodeSamples > 0
            ? (static_cast<double>(rangeTracker.outOfRangeSamples) /
               static_cast<double>(rangeTracker.activeNodeSamples)) *
                  100.0
            : 0.0;
    const uint32_t activeClientsAtStop =
        CountActiveClients(nClients,
                           simTime,
                           launchConfig.clientStart,
                           launchConfig.staggeredLaunch,
                           launchConfig.dronesPerWave,
                           launchConfig.launchInterval);

    std::cout << "\n============================================================\n";
    std::cout << " FANET CSMA/CA WIFI AD-HOC - LAB 5 RESULT\n";
    std::cout << "============================================================\n";

    PrintSection("Scenario");
    PrintRow("Mode",
             OutputModeName(mobile) +
                 std::string(mobile ? " (FANET RandomWaypoint)" : " (static Lab 5 baseline)"));
    PrintRow("Nodes",
             std::to_string(nNodes) + " total = 1 gateway + " + std::to_string(nClients) +
                 " clients");
    PrintRow("Gateway", "Node 0, UDP server");
    PrintRow("Client nodes", "Node 1.." + std::to_string(nNodes - 1) + ", UDP clients");
    PrintRow("Routing", "one-hop UDP to gateway, no relay routing");
    PrintRow("Map size", FormatDouble(areaSide, 0) + " x " + FormatDouble(areaSide, 0), "m");
    PrintRow("Gateway position",
             "(" + FormatDouble(areaSide * 0.5, 1) + ", " + FormatDouble(areaSide * 0.5, 1) +
                 ", 0.0)");
    PrintRow("Mobility start", mobile ? "t=0 for all drones" : "static from t=0");

    PrintSection("WiFi and CSMA/CA");
    PrintRow("WiFi standard", "802.11g ad-hoc");
    PrintRow("Data mode", dataMode);
    PrintRow("Control mode", controlMode);
    PrintRow("RTS/CTS threshold", std::to_string(rtsCtsThreshold), "bytes");
    PrintRow("RTS/CTS status", rtsCtsDisabledForPayload ? "disabled for payload" : "enabled");
    PrintRow("Path loss exponent", FormatDouble(pathLossExponent, 2));

    PrintSection("Traffic");
    PrintRow("Packet size", std::to_string(packetSize), "bytes");
    PrintRow("Packet rate per client", FormatDouble(packetRate, 2), "pkt/s");
    PrintRow("Simulation time", FormatDouble(simTime, 2), "s");
    PrintRow("Server start", FormatDouble(serverStart, 2), "s");
    PrintRow("First client start", FormatDouble(clientStart, 2), "s");
    PrintRow("Launch model", staggeredLaunch ? "staggered launch" : "all clients together");
    PrintRow("Launch scope", staggeredLaunch ? "traffic + range accounting" : "traffic only");
    PrintRow("Drones per wave", std::to_string(dronesPerWave));
    PrintRow("Launch interval", FormatDouble(launchInterval, 2), "s");
    PrintRow("Active clients at stop", std::to_string(activeClientsAtStop));
    PrintRow("Max offered load", FormatDouble(offeredLoadMbps), "Mbps");

    PrintSection("Performance");
    PrintRow("Throughput", FormatDouble(throughputMbps), "Mbps");
    PrintRow("Throughput basis", "application payload bytes / simTime");
    PrintRow("PDR", FormatDouble(pdr), "%");
    PrintRow("Packet loss", FormatDouble(lossRate), "%");
    PrintRow("Average delay", FormatDouble(avgDelayMs), "ms");
    PrintRow("Gateway range exit events", std::to_string(rangeTracker.exitEvents));
    PrintRow("Link tearing rate", FormatDouble(linkTearingRate), "events/s");
    PrintRow("Link tearing basis", "gateway exits / simTime");
    PrintRow("Out-of-range samples", FormatDouble(outOfRangePercent), "%");

    if (!milestoneResults.empty())
    {
        PrintSection("Staggered Launch Milestones");
        for (const auto& result : milestoneResults)
        {
            std::ostringstream label;
            label << FormatDouble(result.intervalEnd, 0) << "s / "
                  << result.activeClients << " active clients";

            std::ostringstream value;
            value << "PDR " << FormatDouble(result.pdr, 2) << "%, Thr "
                  << FormatDouble(result.throughputMbps, 3) << " Mbps, Load "
                  << FormatDouble(result.offeredLoadMbps, 3) << " Mbps, Delay "
                  << FormatDouble(result.avgDelayMs, 2) << " ms, Tear "
                  << FormatDouble(result.linkTearingRate, 4) << "/s";
            PrintRow(label.str(), value.str());
        }
    }

    PrintSection("Raw counters");
    PrintRow("Measured flows", std::to_string(measuredFlows));
    PrintRow("Tx packets", std::to_string(txPackets));
    PrintRow("Rx packets", std::to_string(rxPackets));
    PrintRow("Rx bytes", std::to_string(rxBytes));
    PrintRow("Rx payload bytes", std::to_string(rxPayloadBytes));
    PrintRow("Lost packets", std::to_string(lostPackets));

    PrintSection("Output files");

    if (writeCsv)
    {
        EnsureParentDirectory(csvFile);
        const bool header = CsvNeedsHeader(csvFile);
        std::ofstream csv(csvFile, std::ios_base::app);
        if (!csv.is_open())
        {
            std::cerr << "Warning: cannot open CSV output file " << csvFile << std::endl;
        }
        else
        {
            if (header)
            {
                csv << "mode,nNodes,clients,rtsCtsThreshold,packetSize,packetRate,simTime,"
                    << "serverStart,clientStart,"
                    << "offeredLoadMbps,measuredFlows,txPackets,rxPackets,rxBytes,"
                    << "rxPayloadBytes,lostPackets,"
                    << "pdrPercent,lossRatePercent,throughputMbps,avgDelayMs,"
                    << "gatewayRangeExitEvents,linkTearingRatePerSecond,"
                    << "outOfRangeSamplePercent\n";
            }
            csv << OutputModeName(mobile) << ',' << nNodes << ',' << nClients << ','
                << rtsCtsThreshold << ',' << packetSize << ',' << packetRate << ',' << simTime
                << ',' << serverStart << ',' << clientStart << ',' << offeredLoadMbps << ','
                << measuredFlows << ',' << txPackets << ',' << rxPackets << ',' << rxBytes << ','
                << rxPayloadBytes << ',' << lostPackets << ',' << pdr << ',' << lossRate << ','
                << throughputMbps << ',' << avgDelayMs << ',' << rangeTracker.exitEvents << ','
                << linkTearingRate << ',' << outOfRangePercent << '\n';
            PrintRow("CSV summary", csvFile);
        }
    }
    else
    {
        PrintRow("CSV summary", "disabled");
    }

    if (writeMilestoneCsv && !milestoneResults.empty())
    {
        EnsureParentDirectory(milestoneCsvFile);
        const bool header = CsvNeedsHeader(milestoneCsvFile);
        std::ofstream csv(milestoneCsvFile, std::ios_base::app);
        if (!csv.is_open())
        {
            std::cerr << "Warning: cannot open milestone CSV output file " << milestoneCsvFile
                      << std::endl;
        }
        else
        {
            if (header)
            {
                csv << "mode,nNodes,rtsCtsThreshold,packetSize,packetRate,simTime,"
                    << "intervalStart,intervalEnd,activeClients,txPackets,rxPackets,rxBytes,"
                    << "rxPayloadBytes,lostPackets,"
                    << "pdrPercent,lossRatePercent,offeredLoadMbps,throughputMbps,avgDelayMs,"
                    << "gatewayRangeExitEvents,linkTearingRatePerSecond,"
                    << "outOfRangeSamplePercent\n";
            }
            for (const auto& result : milestoneResults)
            {
                csv << OutputModeName(mobile) << ',' << nNodes << ',' << rtsCtsThreshold << ','
                    << packetSize << ',' << packetRate << ',' << simTime << ','
                    << result.intervalStart << ',' << result.intervalEnd << ','
                    << result.activeClients << ',' << result.txPackets << ',' << result.rxPackets
                    << ',' << result.rxBytes << ',' << result.rxPayloadBytes << ','
                    << result.lostPackets << ',' << result.pdr << ',' << result.lossRate << ','
                    << result.offeredLoadMbps << ',' << result.throughputMbps << ','
                    << result.avgDelayMs << ',' << result.gatewayRangeExitEvents << ','
                    << result.linkTearingRate << ',' << result.outOfRangePercent << '\n';
            }
            PrintRow("Milestone CSV", milestoneCsvFile);
        }
    }
    else if (writeMilestoneCsv)
    {
        PrintRow("Milestone CSV", "not generated");
    }
    else
    {
        PrintRow("Milestone CSV", "disabled");
    }

    if (writeFlowXml)
    {
        if (flowXmlFile.empty())
        {
            std::ostringstream name;
            name << "results/flowmon/fanet-csma-n" << nNodes << '-' << OutputModeName(mobile)
                 << ".flowmon.xml";
            flowXmlFile = name.str();
        }
        EnsureParentDirectory(flowXmlFile);
        monitor->SerializeToXmlFile(flowXmlFile, true, true);
        PrintRow("FlowMonitor XML", flowXmlFile);
    }
    else
    {
        PrintRow("FlowMonitor XML", "disabled");
    }

    if (enablePcap)
    {
        PrintRow("PCAP prefix", pcapPrefix);
    }
    else
    {
        PrintRow("PCAP prefix", "disabled");
    }

    if (writeNetAnim)
    {
        PrintRow("NetAnim XML", netAnimFile);
    }
    else
    {
        PrintRow("NetAnim XML", "disabled");
    }

    std::cout << "============================================================\n";

    Simulator::Destroy();
    return 0;
}
