/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Wifi7IndustrialMlo");

int main(int argc, char* argv[])
{
    uint32_t nSta = 10;
    Time simTime = Seconds(10);

    NodeContainer apNode;
    apNode.Create(1);
    NodeContainer staNodes;
    staNodes.Create(nSta);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211be);
    wifi.ConfigEhtOptions("EmlsrActivated", BooleanValue(true));
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue("EhtMcs13"),
                                 "ControlMode",
                                 StringValue("EhtMcs13"));

    SpectrumWifiPhyHelper phy(2);
    phy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    phy.Set("ChannelSwitchDelay", TimeValue(MicroSeconds(100)));

    Ptr<MultiModelSpectrumChannel> ch6 = CreateObject<MultiModelSpectrumChannel>();
    ch6->AddPropagationLossModel(CreateObject<LogDistancePropagationLossModel>());
    phy.AddChannel(ch6, WIFI_SPECTRUM_6_GHZ);
    phy.Set(0, "ChannelSettings", StringValue("{0, 320, BAND_6GHZ, 0}"));

    Ptr<MultiModelSpectrumChannel> ch5 = CreateObject<MultiModelSpectrumChannel>();
    ch5->AddPropagationLossModel(CreateObject<LogDistancePropagationLossModel>());
    phy.AddChannel(ch5, WIFI_SPECTRUM_5_GHZ);
    phy.Set(1, "ChannelSettings", StringValue("{0, 160, BAND_5GHZ, 0}"));

    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi7-industrial");

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    mac.SetEmlsrManager("ns3::DefaultEmlsrManager", "EmlsrLinkSet", StringValue("0,1"));
    NetDeviceContainer staDevices = wifi.Install(phy, mac, staNodes);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    mac.SetApEmlsrManager("ns3::DefaultApEmlsrManager");
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
        clientApp.Start(Seconds(1.0));
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
            double throughput = s.second.rxBytes * 8.0 /
                                  (s.second.timeLastRxPacket.GetSeconds() -
                                   s.second.timeFirstTxPacket.GetSeconds()) / 1e6;
            double delay = s.second.delaySum.GetSeconds() / s.second.rxPackets;
            std::cout << "Flow to " << t.destinationAddress
                      << " Throughput: " << throughput << " Mbit/s"
                      << " Avg Delay: " << delay * 1000 << " ms" << std::endl;
        }
    }

    Simulator::Destroy();
    return 0;
}

