
/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * fanet-csma.cc
 *
 * FANET CSMA/CA Performance Evaluation (WITHOUT RTS/CTS)
 * -------------------------------------------------------
 * Evaluates the saturation point and performance collapse of the
 * CSMA/CA protocol when the RTS/CTS protection mechanism is completely
 * disabled in a Flying Ad-hoc Network (FANET) environment.
 *
 * All performance degradation is strictly due to MAC-layer collisions
 * (hidden node problem & queue congestion). No physical-layer fading,
 * link outage, or multi-hop routing effects are involved.
 *


#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"
#include <iomanip>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FanetCsmaEval");

int
main (int argc, char *argv[])
{
  // ===================================================================
  // I. COMMAND-LINE ARGUMENTS
  // ===================================================================
  uint32_t nUavs = 4;      // Default: 4 UAVs (Total 5 nodes = 1 Sink + 4 UAV)
  bool enableRts = false;   // Default: RTS/CTS disabled
  uint32_t rngSeed = RngSeedManager::GetSeed ();
  uint64_t rngRun = RngSeedManager::GetRun ();

  CommandLine cmd (__FILE__);
  cmd.AddValue ("nUavs", "Number of UAV nodes (total nodes = 1 Sink + nUavs)", nUavs);
  cmd.AddValue ("enableRts", "Enable RTS/CTS (true/false)", enableRts);
  cmd.AddValue ("RngSeed", "Global RNG seed", rngSeed);
  cmd.AddValue ("RngRun", "Global RNG run/substream index", rngRun);
  cmd.Parse (argc, argv);

  RngSeedManager::SetSeed (rngSeed);
  RngSeedManager::SetRun (rngRun);

  uint32_t totalNodes = 1 + nUavs; // Node 0 = Sink, Node 1..N = UAVs
  double simTime = 50.0;           // Simulation time in seconds

  std::cout << "\n========================================" << std::endl;
  std::cout << " FANET CSMA/CA Evaluation" << std::endl;
  std::cout << "========================================" << std::endl;
  std::cout << " Total Nodes : " << totalNodes
            << " (1 Sink + " << nUavs << " UAVs)" << std::endl;
  std::cout << " RTS/CTS     : " << (enableRts ? "ENABLED" : "DISABLED") << std::endl;
  std::cout << " Sim Time    : " << simTime << " s" << std::endl;
  std::cout << "========================================\n" << std::endl;

  // ===================================================================
  // II. NODE CREATION
  // ===================================================================
  NodeContainer allNodes;
  allNodes.Create (totalNodes);

  // Separate sink (Node 0) and UAV nodes (Node 1..N)
  Ptr<Node> sinkNode = allNodes.Get (0);

  NodeContainer uavNodes;
  for (uint32_t i = 1; i < totalNodes; ++i)
    {
      uavNodes.Add (allNodes.Get (i));
    }

  // ===================================================================
  // III. WIFI SETUP (802.11b Ad-Hoc, RTS/CTS DISABLED)
  // ===================================================================

  // RTS/CTS threshold: 0 = always use RTS/CTS, 999999 = never use RTS/CTS
  if (enableRts)
    {
      Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold",
                          StringValue ("0"));
    }
  else
    {
      Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold",
                          StringValue ("999999"));
    }

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211b);

  // Use the default rate control (ArfWifiManager for 802.11b)
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  // PHY layer - use default YansWifi channel (no special propagation loss)
  YansWifiPhyHelper wifiPhy;
  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
  wifiPhy.SetChannel (wifiChannel.Create ());

  // MAC layer - Ad-hoc mode (no AP, no infrastructure)
  WifiMacHelper wifiMac;
  wifiMac.SetType ("ns3::AdhocWifiMac");

  // Install WiFi on all nodes
  NetDeviceContainer devices = wifi.Install (wifiPhy, wifiMac, allNodes);

  // ===================================================================
  // IV. INTERNET STACK (No routing protocol - single-hop direct comm)
  // ===================================================================
  InternetStackHelper internet;
  internet.Install (allNodes);

  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = ipv4.Assign (devices);

  // ===================================================================
  // V. MOBILITY SETUP
  // ===================================================================

  // --- Sink Node (Node 0): Fixed at the center of the 100m x 100m area ---
  MobilityHelper sinkMobility;
  Ptr<ListPositionAllocator> sinkPos = CreateObject<ListPositionAllocator> ();
  sinkPos->Add (Vector (50.0, 50.0, 0.0));
  sinkMobility.SetPositionAllocator (sinkPos);
  sinkMobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  sinkMobility.Install (sinkNode);

  // --- UAV Nodes (Node 1..N): RandomWalk2d within 100m x 100m box ---
  MobilityHelper uavMobility;
  uavMobility.SetPositionAllocator (
      "ns3::RandomRectanglePositionAllocator",
      "X", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"),
      "Y", StringValue ("ns3::UniformRandomVariable[Min=0.0|Max=100.0]"));

  uavMobility.SetMobilityModel (
      "ns3::RandomWalk2dMobilityModel",
      "Bounds", RectangleValue (Rectangle (0.0, 100.0, 0.0, 100.0)),
      "Speed", StringValue ("ns3::UniformRandomVariable[Min=4.0|Max=7.0]"));

  uavMobility.Install (uavNodes);

  // ===================================================================
  // VI. APPLICATION SETUP (UDP Traffic: All UAVs -> Sink)
  // ===================================================================
  uint16_t serverPort = 9;

  // --- UdpServer on Sink (Node 0) ---
  UdpServerHelper udpServer (serverPort);
  ApplicationContainer serverApp = udpServer.Install (sinkNode);
  serverApp.Start (Seconds (0.0));
  serverApp.Stop (Seconds (simTime));

  // --- UdpClient on each UAV, targeting the Sink's IP address ---
  Ipv4Address sinkAddr = interfaces.GetAddress (0);

  UdpClientHelper udpClient (sinkAddr, serverPort);
  udpClient.SetAttribute ("MaxPackets", UintegerValue (4294967295u)); // Max uint32
  udpClient.SetAttribute ("Interval", TimeValue (Seconds (0.01)));   // 100 pkts/s
  udpClient.SetAttribute ("PacketSize", UintegerValue (512));         // 512 bytes

  ApplicationContainer clientApps = udpClient.Install (uavNodes);
  clientApps.Start (Seconds (1.0)); // Start slightly after server
  clientApps.Stop (Seconds (simTime));

  // ===================================================================
  // VII. FLOW MONITOR SETUP
  // ===================================================================
  FlowMonitorHelper flowMonHelper;
  Ptr<FlowMonitor> flowMonitor = flowMonHelper.InstallAll ();

  // ===================================================================
  // VIII. RUN SIMULATION
  // ===================================================================
  Simulator::Stop (Seconds (simTime));
  Simulator::Run ();

  // ===================================================================
  // IX. COLLECT & PRINT METRICS FROM FLOWMONITOR
  // ===================================================================
  flowMonitor->CheckForLostPackets ();

  Ptr<Ipv4FlowClassifier> classifier =
      DynamicCast<Ipv4FlowClassifier> (flowMonHelper.GetClassifier ());

  FlowMonitor::FlowStatsContainer stats = flowMonitor->GetFlowStats ();

  uint64_t totalTxPackets = 0;
  uint64_t totalRxPackets = 0;
  uint64_t totalRxBytes   = 0;
  double   totalDelaySum  = 0.0; // Sum of delays in seconds

  for (auto it = stats.begin (); it != stats.end (); ++it)
    {
      Ipv4FlowClassifier::FiveTuple tuple = classifier->FindFlow (it->first);

      // Only consider flows directed to the Sink node's IP address
      if (tuple.destinationAddress == sinkAddr)
        {
          totalTxPackets += it->second.txPackets;
          totalRxPackets += it->second.rxPackets;
          totalRxBytes   += it->second.rxBytes;
          totalDelaySum  += it->second.delaySum.GetSeconds ();
        }
    }

  // --- Calculate Metrics ---
  double throughputMbps = 0.0;
  double pdrPercent     = 0.0;
  double avgDelayMs     = 0.0;

  if (totalRxPackets > 0)
    {
      // Throughput (Mbps) = (Total Rx Bytes * 8) / (Sim Time * 10^6)
      throughputMbps = (totalRxBytes * 8.0) / (simTime * 1000000.0);

      // PDR (%) = (Rx Packets / Tx Packets) * 100
      pdrPercent = (static_cast<double> (totalRxPackets) /
                    static_cast<double> (totalTxPackets)) * 100.0;

      // Average E2E Delay (ms) = (Total Delay in seconds / Rx Packets) * 1000
      avgDelayMs = (totalDelaySum / static_cast<double> (totalRxPackets)) * 1000.0;
    }
  else
    {
      // Edge case: No packets received (extreme collisions)
      throughputMbps = 0.0;
      pdrPercent     = 0.0;
      avgDelayMs     = 0.0;
    }

  // --- Print Results ---
  std::cout << "\n╔══════════════════════════════════════════════╗" << std::endl;
  std::cout << "║       SIMULATION RESULTS (FlowMonitor)       ║" << std::endl;
  std::cout << "╠══════════════════════════════════════════════╣" << std::endl;
  std::cout << "║  Nodes         : " << totalNodes
            << " (1 Sink + " << nUavs << " UAVs)" << std::endl;
  std::cout << "║  RTS/CTS       : " << (enableRts ? "ENABLED" : "DISABLED") << std::endl;
  std::cout << "║  Tx Packets    : " << totalTxPackets << std::endl;
  std::cout << "║  Rx Packets    : " << totalRxPackets << std::endl;
  std::cout << "║  Rx Bytes      : " << totalRxBytes << std::endl;
  std::cout << "╠══════════════════════════════════════════════╣" << std::endl;
  std::cout << std::fixed << std::setprecision (4);
  std::cout << "║  THROUGHPUT    : " << throughputMbps << " Mbps" << std::endl;
  std::cout << "║  PDR           : " << pdrPercent << " %" << std::endl;
  std::cout << "║  AVG DELAY     : " << avgDelayMs << " ms" << std::endl;
  std::cout << "╚══════════════════════════════════════════════╝" << std::endl;
  std::cout << std::endl;

  // ===================================================================
  // X. CLEANUP
  // ===================================================================
  Simulator::Destroy ();

  return 0;
}
