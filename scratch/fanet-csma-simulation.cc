
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace ns3;

namespace
{

std::string
RvUniform(double minValue, double maxValue)
{
    std::ostringstream rv;
    rv << "ns3::UniformRandomVariable[Min=" << minValue << "|Max=" << maxValue << "]";
    return rv.str();
}

std::string
RvConstant(double value)
{
    std::ostringstream rv;
    rv << "ns3::ConstantRandomVariable[Constant=" << value << "]";
    return rv.str();
}

std::string
Format(double value, uint32_t precision = 6)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

std::string ModeName(bool mobile) { return mobile ? "mobile" : "static"; }

void PrintRow(const std::string& key, const std::string& value, const std::string& unit = "")
{
    std::cout << "  " << std::left << std::setw(28) << key << " : " << std::setw(18) << value << unit << '\n';
}

void PrintSection(const std::string& title) { std::cout << "\n[" << title << "]\n"; }

void
EnsureParentDirectory(const std::string& filePath)
{
    std::filesystem::path path(filePath);
    if (path.has_parent_path()) { std::filesystem::create_directories(path.parent_path()); }
}

bool NeedsHeader(const std::string& filePath)
{
    std::ifstream file(filePath);
    return !file.good() || file.peek() == std::ifstream::traits_type::eof();
}

double
LaunchTime(uint32_t clientIndex, double clientStart, bool staggeredLaunch, uint32_t dronesPerWave, double launchInterval)
{
    if (!staggeredLaunch) { return clientStart; }
    const uint32_t wave = (clientIndex - 1) / dronesPerWave;
    return clientStart + static_cast<double>(wave) * launchInterval;
}

void
InstallGateway(NodeContainer nodes, double areaSide)
{
    NodeContainer gateway;
    gateway.Add(nodes.Get(0));

    Ptr<ListPositionAllocator> position = CreateObject<ListPositionAllocator>();
    position->Add(Vector(areaSide * 0.5, areaSide * 0.5, 0.0));

    MobilityHelper mobility;
    mobility.SetPositionAllocator(position);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gateway);
}

NodeContainer
GetDrones(NodeContainer nodes)
{
    NodeContainer drones;
    for (uint32_t i = 1; i < nodes.GetN(); ++i) { drones.Add(nodes.Get(i)); }
    return drones;
}

void
InstallStaticDrones(NodeContainer nodes, double areaSide)
{
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X",
                                  StringValue(RvUniform(0.0, areaSide)),
                                  "Y",
                                  StringValue(RvUniform(0.0, areaSide)));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(GetDrones(nodes));
}

void
InstallMobileDrones(NodeContainer nodes, double areaSide, double minSpeed, double maxSpeed, double pauseTime)
{
    ObjectFactory allocatorFactory;
    allocatorFactory.SetTypeId("ns3::RandomRectanglePositionAllocator");
    allocatorFactory.Set("X", StringValue(RvUniform(0.0, areaSide)));
    allocatorFactory.Set("Y", StringValue(RvUniform(0.0, areaSide)));
    Ptr<PositionAllocator> waypointAllocator =
        allocatorFactory.Create()->GetObject<PositionAllocator>();

    MobilityHelper mobility;
    mobility.SetPositionAllocator(waypointAllocator);
    mobility.SetMobilityModel("ns3::RandomWaypointMobilityModel", "Speed",
                              StringValue(RvUniform(minSpeed, maxSpeed)), "Pause",
                              StringValue(RvConstant(pauseTime)), "PositionAllocator",
                              PointerValue(waypointAllocator));
    mobility.Install(GetDrones(nodes));
}

struct RangeState
{
    std::vector<bool> seen;
    std::vector<bool> inRange;
    uint64_t exitEvents{0};
    uint64_t activeSamples{0};
    uint64_t outOfRangeSamples{0};
};

struct RangeContext : public SimpleRefCount<RangeContext>
{
    NodeContainer nodes;
    RangeState state;
    double communicationRange{140.0};
    double sampleInterval{0.5};
    double simTime{1800.0};
    double clientStart{2.0};
    bool staggeredLaunch{true};
    uint32_t dronesPerWave{5};
    double launchInterval{300.0};
};

double
DistanceToGateway(NodeContainer nodes, uint32_t nodeIndex)
{
    Ptr<MobilityModel> gateway = nodes.Get(0)->GetObject<MobilityModel>();
    Ptr<MobilityModel> drone = nodes.Get(nodeIndex)->GetObject<MobilityModel>();
    return CalculateDistance(gateway->GetPosition(), drone->GetPosition());
}

void
PollGatewayRange(Ptr<RangeContext> context)
{
    const double now = Simulator::Now().GetSeconds();
    for (uint32_t i = 1; i < context->nodes.GetN(); ++i)
    {
        const double start = LaunchTime(i, context->clientStart, context->staggeredLaunch,
                                        context->dronesPerWave, context->launchInterval);
        if (start > now) { continue; }

        const bool current = DistanceToGateway(context->nodes, i) <= context->communicationRange;
        if (!context->state.seen[i]) { context->state.seen[i] = true; }
        else if (context->state.inRange[i] && !current)
        {
            ++context->state.exitEvents;
        }
        context->state.inRange[i] = current;
        ++context->state.activeSamples;
        if (!current) { ++context->state.outOfRangeSamples; }
    }

    if (now + context->sampleInterval <= context->simTime + 1e-9)
    {
        Simulator::Schedule(Seconds(context->sampleInterval), &PollGatewayRange, context);
    }
}

} // namespace

int
main(int argc, char* argv[])
{
    uint32_t nNodes = 30;
    bool mobile = true;
    bool staggeredLaunch = true;
    uint32_t rtsCtsThreshold = 2346;
    uint32_t packetSize = 512;
    uint32_t dronesPerWave = 5;
    uint32_t rngRun = 1;
    double packetRate = 100.0;
    double simTime = 1800.0;
    double serverStart = 1.0;
    double clientStart = 2.0;
    double launchInterval = 300.0;
    double areaSide = 500.0;
    double minSpeed = 5.0;
    double maxSpeed = 15.0;
    double pauseTime = 2.0;
    double communicationRange = 140.0;
    double rangeSampleInterval = 0.5;
    double pathLossExponent = 3.0;
    bool writeCsv = true;
    bool writeFlowXml = true;
    std::string dataMode = "ErpOfdmRate12Mbps";
    std::string controlMode = "ErpOfdmRate6Mbps";
    std::string csvFile = "results/csv/fanet-csma-compact-results.csv";
    std::string flowXmlFile;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nNodes", "Total nodes, node 0 is the gateway, valid range: 2..30", nNodes);
    cmd.AddValue("mobile", "true = FANET RandomWaypoint, false = static baseline", mobile);
    cmd.AddValue("staggeredLaunch", "Start drone traffic in launch waves", staggeredLaunch);
    cmd.AddValue("rtsCtsThreshold", "2346 disables RTS/CTS for 512-byte payloads; 0 enables it", rtsCtsThreshold);
    cmd.AddValue("packetSize", "UDP application payload size in bytes", packetSize);
    cmd.AddValue("packetRate", "Packets per second per active drone", packetRate);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("serverStart", "UDP server start time", serverStart);
    cmd.AddValue("clientStart", "First UDP client start time", clientStart);
    cmd.AddValue("dronesPerWave", "Number of drones per staggered launch wave", dronesPerWave);
    cmd.AddValue("launchInterval", "Seconds between launch waves", launchInterval);
    cmd.AddValue("areaSide", "Square FANET area side in meters", areaSide);
    cmd.AddValue("minSpeed", "Minimum RandomWaypoint speed in m/s", minSpeed);
    cmd.AddValue("maxSpeed", "Maximum RandomWaypoint speed in m/s", maxSpeed);
    cmd.AddValue("pauseTime", "RandomWaypoint pause time in seconds", pauseTime);
    cmd.AddValue("communicationRange", "Range used for gateway exit statistics", communicationRange);
    cmd.AddValue("rangeSampleInterval", "Range polling interval in seconds", rangeSampleInterval);
    cmd.AddValue("pathLossExponent", "LogDistance path loss exponent", pathLossExponent);
    cmd.AddValue("dataMode", "802.11g data mode", dataMode);
    cmd.AddValue("controlMode", "802.11g control mode", controlMode);
    cmd.AddValue("rngRun", "ns-3 RNG run number", rngRun);
    cmd.AddValue("writeCsv", "Append one summary row to CSV", writeCsv);
    cmd.AddValue("csvFile", "CSV summary path", csvFile);
    cmd.AddValue("writeFlowXml", "Write FlowMonitor XML", writeFlowXml);
    cmd.AddValue("flowXmlFile", "FlowMonitor XML path; empty uses automatic path", flowXmlFile);
    cmd.Parse(argc, argv);

    if (nNodes < 2 || nNodes > 30 || packetSize == 0 || packetRate <= 0.0 || simTime <= clientStart ||
        serverStart < 0.0 || clientStart < serverStart || dronesPerWave == 0 ||
        launchInterval <= 0.0 || areaSide <= 0.0 || communicationRange <= 0.0 ||
        rangeSampleInterval <= 0.0 || pathLossExponent <= 0.0 || (mobile && minSpeed > maxSpeed))
    {
        std::cerr << "Error: invalid Project Netsim FANET parameter set.\n";
        return 1;
    }

    RngSeedManager::SetRun(rngRun);
    Config::SetDefault("ns3::WifiRemoteStationManager::RtsCtsThreshold", UintegerValue(rtsCtsThreshold));
    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue(controlMode));

    const uint32_t nClients = nNodes - 1;
    const double maxOfferedLoadMbps =
        static_cast<double>(nClients) * packetSize * 8.0 * packetRate / 1000000.0;
    const bool rtsDisabledForPayload = rtsCtsThreshold >= packetSize;

    NodeContainer nodes;
    nodes.Create(nNodes);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211g);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue(dataMode),
                                 "ControlMode", StringValue(controlMode), "RtsCtsThreshold",
                                 UintegerValue(rtsCtsThreshold));

    YansWifiChannelHelper channel;
    channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    channel.AddPropagationLoss("ns3::LogDistancePropagationLossModel", "Exponent",
                               DoubleValue(pathLossExponent), "ReferenceDistance", DoubleValue(1.0),
                               "ReferenceLoss", DoubleValue(40.046));

    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

    WifiMacHelper mac;
    mac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer devices = wifi.Install(phy, mac, nodes);

    InstallGateway(nodes, areaSide);
    if (mobile)
    {
        InstallMobileDrones(nodes, areaSide, minSpeed, maxSpeed, pauseTime);
    }
    else
    {
        InstallStaticDrones(nodes, areaSide);
    }

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);
    const Ipv4Address gatewayAddress = interfaces.GetAddress(0);
    const uint16_t gatewayPort = 9;

    UdpServerHelper server(gatewayPort);
    ApplicationContainer serverApp = server.Install(nodes.Get(0));
    serverApp.Start(Seconds(serverStart));
    serverApp.Stop(Seconds(simTime));

    for (uint32_t i = 1; i < nNodes; ++i)
    {
        const double start = LaunchTime(i, clientStart, staggeredLaunch, dronesPerWave, launchInterval);
        if (start >= simTime) { continue; }
        UdpClientHelper client(gatewayAddress, gatewayPort);
        client.SetAttribute("MaxPackets",
                            UintegerValue(static_cast<uint32_t>(std::ceil((simTime - start) * packetRate))));
        client.SetAttribute("Interval", TimeValue(Seconds(1.0 / packetRate)));
        client.SetAttribute("PacketSize", UintegerValue(packetSize));
        ApplicationContainer app = client.Install(nodes.Get(i));
        app.Start(Seconds(start));
        app.Stop(Seconds(simTime));
    }

    Ptr<RangeContext> rangeContext = Create<RangeContext>();
    rangeContext->nodes = nodes;
    rangeContext->communicationRange = communicationRange;
    rangeContext->sampleInterval = rangeSampleInterval;
    rangeContext->simTime = simTime;
    rangeContext->clientStart = clientStart;
    rangeContext->staggeredLaunch = staggeredLaunch;
    rangeContext->dronesPerWave = dronesPerWave;
    rangeContext->launchInterval = launchInterval;
    rangeContext->state.seen.assign(nNodes, false);
    rangeContext->state.inRange.assign(nNodes, false);
    Simulator::Schedule(Seconds(rangeSampleInterval), &PollGatewayRange, rangeContext);

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    monitor->CheckForLostPackets();

    uint64_t txPackets = 0;
    uint64_t rxPackets = 0;
    uint64_t rxBytes = 0;
    double delaySumSeconds = 0.0;
    uint32_t measuredFlows = 0;

    for (const auto& item : monitor->GetFlowStats())
    {
        const Ipv4FlowClassifier::FiveTuple tuple = classifier->FindFlow(item.first);
        if (tuple.destinationAddress == gatewayAddress && tuple.destinationPort == gatewayPort &&
            tuple.protocol == 17)
        {
            txPackets += item.second.txPackets;
            rxPackets += item.second.rxPackets;
            rxBytes += item.second.rxBytes;
            delaySumSeconds += item.second.delaySum.GetSeconds();
            ++measuredFlows;
        }
    }

    const uint64_t rxPayloadBytes = rxPackets * packetSize;
    const uint64_t lostPackets = txPackets >= rxPackets ? txPackets - rxPackets : 0;
    const double pdr = txPackets > 0 ? 100.0 * static_cast<double>(rxPackets) / txPackets : 0.0;
    const double lossRate =
        txPackets > 0 ? 100.0 * static_cast<double>(lostPackets) / txPackets : 0.0;
    const double throughputMbps = static_cast<double>(rxPayloadBytes) * 8.0 / simTime / 1000000.0;
    const double avgDelayMs =
        rxPackets > 0 ? delaySumSeconds * 1000.0 / static_cast<double>(rxPackets) : 0.0;
    const double linkTearingRate =
        rangeContext->state.exitEvents / std::max(simTime, 1.0);
    const double outOfRangePercent =
        rangeContext->state.activeSamples > 0
            ? 100.0 * static_cast<double>(rangeContext->state.outOfRangeSamples) /
                  rangeContext->state.activeSamples
            : 0.0;
    const uint32_t launchedByTime = (static_cast<uint32_t>((simTime - clientStart) / launchInterval) + 1) * dronesPerWave;
    const uint32_t activeClientsAtStop =
        !staggeredLaunch || launchedByTime > nClients ? nClients : launchedByTime;

    std::cout << "\n============================================================\n";
    std::cout << " FANET CSMA/CA WIFI AD-HOC - LAB 5 RESULT\n";
    std::cout << "============================================================\n";

    PrintSection("Scenario");
    PrintRow("Mode", ModeName(mobile) + (mobile ? " (FANET RandomWaypoint)" : " (static Lab 5 baseline)"));
    PrintRow("Nodes", std::to_string(nNodes) + " total = 1 gateway + " + std::to_string(nClients) + " clients");
    PrintRow("Gateway", "Node 0, UDP server");
    PrintRow("Client nodes", "Node 1.." + std::to_string(nNodes - 1) + ", UDP clients");
    PrintRow("Routing", "one-hop UDP to gateway, no relay routing");
    PrintRow("Map size", Format(areaSide, 0) + " x " + Format(areaSide, 0), "m");
    PrintRow("Gateway position", "(" + Format(areaSide * 0.5, 1) + ", " + Format(areaSide * 0.5, 1) + ", 0.0)");
    PrintRow("Mobility start", mobile ? "t=0 for all drones" : "static from t=0");

    PrintSection("WiFi and CSMA/CA");
    PrintRow("WiFi standard", "802.11g ad-hoc");
    PrintRow("Data mode", dataMode);
    PrintRow("Control mode", controlMode);
    PrintRow("RTS/CTS threshold", std::to_string(rtsCtsThreshold), "bytes");
    PrintRow("RTS/CTS status", rtsDisabledForPayload ? "disabled for payload" : "enabled");
    PrintRow("Path loss exponent", Format(pathLossExponent, 2));

    PrintSection("Traffic");
    PrintRow("Packet size", std::to_string(packetSize), "bytes");
    PrintRow("Packet rate per client", Format(packetRate, 2), "pkt/s");
    PrintRow("Simulation time", Format(simTime, 2), "s");
    PrintRow("Server start", Format(serverStart, 2), "s");
    PrintRow("First client start", Format(clientStart, 2), "s");
    PrintRow("Launch model", staggeredLaunch ? "staggered launch" : "all clients together");
    PrintRow("Launch scope", staggeredLaunch ? "traffic + range accounting" : "traffic only");
    PrintRow("Drones per wave", std::to_string(dronesPerWave));
    PrintRow("Launch interval", Format(launchInterval, 2), "s");
    PrintRow("Active clients at stop", std::to_string(activeClientsAtStop));
    PrintRow("Max offered load", Format(maxOfferedLoadMbps), "Mbps");

    PrintSection("Performance");
    PrintRow("Throughput", Format(throughputMbps), "Mbps");
    PrintRow("Throughput basis", "application payload bytes / simTime");
    PrintRow("PDR", Format(pdr), "%");
    PrintRow("Packet loss", Format(lossRate), "%");
    PrintRow("Average delay", Format(avgDelayMs), "ms");
    PrintRow("Gateway range exit events", std::to_string(rangeContext->state.exitEvents));
    PrintRow("Link tearing rate", Format(linkTearingRate), "events/s");
    PrintRow("Link tearing basis", "gateway exits / simTime");
    PrintRow("Out-of-range samples", Format(outOfRangePercent), "%");

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
        const bool header = NeedsHeader(csvFile);
        std::ofstream csv(csvFile, std::ios_base::app);
        if (header)
        {
            csv << "mode,nNodes,clients,rtsCtsThreshold,packetSize,packetRate,simTime,serverStart,clientStart,offeredLoadMbps,measuredFlows,txPackets,rxPackets,rxBytes,rxPayloadBytes,lostPackets,pdrPercent,lossRatePercent,throughputMbps,avgDelayMs,gatewayRangeExitEvents,linkTearingRatePerSecond,outOfRangeSamplePercent\n";
        }
        csv << ModeName(mobile) << ',' << nNodes << ',' << nClients << ',' << rtsCtsThreshold
            << ',' << packetSize << ',' << packetRate << ',' << simTime << ',' << serverStart
            << ',' << clientStart << ',' << maxOfferedLoadMbps << ',' << measuredFlows << ','
            << txPackets << ',' << rxPackets << ',' << rxBytes << ',' << rxPayloadBytes << ','
            << lostPackets << ',' << pdr << ',' << lossRate << ',' << throughputMbps << ','
            << avgDelayMs << ',' << rangeContext->state.exitEvents << ',' << linkTearingRate
            << ',' << outOfRangePercent << '\n';
        PrintRow("CSV summary", csvFile);
    }
    else
    {
        PrintRow("CSV summary", "disabled");
    }

    if (writeFlowXml)
    {
        if (flowXmlFile.empty())
        {
            std::ostringstream name;
            name << "results/flowmon/fanet-csma-n" << nNodes << '-' << ModeName(mobile) << ".flowmon.xml";
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

    PrintRow("Milestone CSV", "disabled");
    PrintRow("PCAP prefix", "disabled");
    PrintRow("NetAnim XML", "disabled");
    std::cout << "============================================================\n";

    Simulator::Destroy();
    return 0;
}
