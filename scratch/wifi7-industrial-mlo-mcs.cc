/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Example usage:
 *   ./ns3 run "scratch/wifi7-industrial-mlo-mcs --nSta=20 --simTime=15s --ehtMcs=EhtMcs7"
 *   ./ns3 run "scratch/wifi7-industrial-mlo-mcs --nSta=50 --simTime=30s --ehtMcs=EhtMcs9"
 *
 * This example demonstrates a simple Wi-Fi 7 network with one access
 * point and multiple stations.  It intentionally avoids experimental
 * multi-link operation and other unstable features.
*/

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-flow-classifier.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wifi7IndustrialMloMcs");

int main(int argc, char* argv[])
{
    uint32_t nSta = 10;        // default number of stations
    Time simTime = Seconds(10); // default simulation time
    std::string ehtMcs = "EhtMcs9"; // default EHT MCS

    CommandLine cmd(__FILE__);
    cmd.AddValue("nSta", "Number of station nodes", nSta);
    cmd.AddValue("simTime", "Simulation duration", simTime);
    cmd.AddValue("ehtMcs", "EHT MCS (e.g., EhtMcs7)", ehtMcs);
    cmd.Parse(argc, argv);

    if (nSta == 0 || simTime <= Seconds(0))
    {
        NS_LOG_UNCOND("Error: nSta must be greater than 0 and simTime must be positive");
        return 1;
    }

    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(nSta);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211be);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue(ehtMcs),
                                 "ControlMode",
                                 StringValue(ehtMcs));

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi7-industrial");

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator>();
    pos->Add(Vector(0.0, 0.0, 0.0));
    for (uint32_t i = 0; i < nSta; ++i)
    {
        pos->Add(Vector(5.0 * (i + 1), 0.0, 0.0));
    }
    mobility.SetPositionAllocator(pos);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNodes);

    InternetStackHelper internet;
    internet.Install(apNode);
    internet.Install(staNodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer apIf = ipv4.Assign(apDevice);
    Ipv4InterfaceContainer staIf = ipv4.Assign(staDevices);

    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApps = server.Install(staNodes);
    serverApps.Start(Seconds(0.0));
    serverApps.Stop(simTime);

    for (uint32_t i = 0; i < nSta; ++i)
    {
        UdpClientHelper client(staIf.GetAddress(i), port);
        client.SetAttribute("MaxPackets", UintegerValue(0xffffffff));
        client.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
        client.SetAttribute("PacketSize", UintegerValue(1200));
        ApplicationContainer clientApp = client.Install(apNode.Get(0));
        // Start traffic after STAs have had time to associate
        clientApp.Start(Seconds(2.0));
        clientApp.Stop(simTime);
    }

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(simTime);
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (const auto& s : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(s.first);
        if (t.sourceAddress == apIf.GetAddress(0))
        {
            if (s.second.rxPackets > 0 &&
                s.second.timeLastRxPacket > s.second.timeFirstTxPacket)
            {
                double throughput = s.second.rxBytes * 8.0 /
                                      (s.second.timeLastRxPacket.GetSeconds() -
                                       s.second.timeFirstTxPacket.GetSeconds()) / 1e6;
                double delay = s.second.delaySum.GetSeconds() / s.second.rxPackets;
                std::cout << "Flow to " << t.destinationAddress
                          << " Throughput: " << throughput << " Mbit/s"
                          << " Avg Delay: " << delay * 1000 << " ms" << std::endl;
            }
            else
            {
                std::cout << "Flow to " << t.destinationAddress << " No packets received" << std::endl;
            }
        }
    }

    Simulator::Destroy();
    return 0;
}

