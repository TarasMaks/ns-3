#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/multi-model-spectrum-channel.h"
#include "ns3/propagation-delay-model.h"
#include "ns3/propagation-loss-model.h"
#include "ns3/spectrum-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/string.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/udp-server.h"
#include "ns3/uinteger.h"

using namespace ns3;

int
main(int argc, char* argv[])
{
    Time simulationTime{"30s"};
    std::string mcs{"EhtMcs1"};

    CommandLine cmd(__FILE__);
    cmd.AddValue("simulationTime", "Simulation time", simulationTime);
    cmd.AddValue("mcs", "EHT MCS", mcs);
    cmd.Parse(argc, argv);

    NodeContainer staNode;
    staNode.Create(1);
    NodeContainer apNode;
    apNode.Create(1);

    Ptr<MultiModelSpectrumChannel> channel = CreateObject<MultiModelSpectrumChannel>();
    channel->AddPropagationLossModel(CreateObject<LogDistancePropagationLossModel>());
    channel->SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());
    SpectrumWifiPhyHelper phy;
    phy.SetChannel(channel);

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211be);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue(mcs),
                                 "ControlMode",
                                 StringValue(mcs));
    WifiMacHelper mac;

    Ssid ssid = Ssid("ns3-80211be");
    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer staDevice = wifi.Install(phy, mac, staNode);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    positionAlloc->Add(Vector(0.0, 0.0, 0.0));
    positionAlloc->Add(Vector(5.0, 0.0, 0.0));
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNode);

    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNode);

    Ipv4AddressHelper address;
    address.SetBase("192.168.1.0", "255.255.255.0");
    Ipv4InterfaceContainer staIf = address.Assign(staDevice);
    Ipv4InterfaceContainer apIf = address.Assign(apDevice);

    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(staNode.Get(0));
    serverApp.Start(Seconds(0));
    serverApp.Stop(simulationTime + Seconds(1));

    UdpClientHelper client(staIf.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(4294967295U));
    client.SetAttribute("Interval", TimeValue(Time("0.0001")));
    client.SetAttribute("PacketSize", UintegerValue(1472));
    ApplicationContainer clientApp = client.Install(apNode.Get(0));
    clientApp.Start(Seconds(1));
    clientApp.Stop(simulationTime + Seconds(1));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(simulationTime + Seconds(1));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    auto stats = monitor->GetFlowStats();
    for (const auto& s : stats)
    {
        auto t = classifier->FindFlow(s.first);
        if (t.destinationAddress == staIf.GetAddress(0))
        {
            double throughput = s.second.rxBytes * 8.0 / (simulationTime.GetSeconds()) / 1e6;
            double meanDelay =
                s.second.rxPackets ? s.second.delaySum.GetSeconds() / s.second.rxPackets : 0.0;
            std::cout << "Throughput: " << throughput << " Mbps" << std::endl;
            std::cout << "Mean delay: " << meanDelay << " s" << std::endl;
        }
    }

    Simulator::Destroy();
    return 0;
}
