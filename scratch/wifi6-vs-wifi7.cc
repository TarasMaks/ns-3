#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

namespace {
std::vector<double>
ParseDoubles(const std::string& str)
{
    std::vector<double> vals;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, ','))
    {
        std::stringstream ts(token);
        double v;
        ts >> v;
        if (!ts.fail())
        {
            vals.push_back(v);
        }
    }
    return vals;
}

std::vector<uint8_t>
ParseMcs(const std::string& str)
{
    std::vector<uint8_t> vals;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, ','))
    {
        std::stringstream ts(token);
        int v;
        ts >> v;
        if (!ts.fail() && v >= 0)
        {
            vals.push_back(static_cast<uint8_t>(v));
        }
    }
    return vals;
}

struct Result
{
    double throughputMbps{0};
    double delayMs{0};
};

Result
RunExperiment(WifiStandard standard, double distance, uint8_t mcs)
{
    NodeContainer staNode;
    staNode.Create(1);
    NodeContainer apNode;
    apNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy = YansWifiPhyHelper::Default();
    phy.SetChannel(channel.Create());

    WifiHelper wifi;
    wifi.SetStandard(standard);
    std::string modeStr;
    if (standard == WIFI_STANDARD_80211be)
    {
        modeStr = "EhtMcs" + std::to_string(mcs);
    }
    else
    {
        modeStr = "HeMcs" + std::to_string(mcs);
    }
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue(modeStr),
                                 "ControlMode",
                                 StringValue(modeStr));

    WifiMacHelper mac;
    Ssid ssid = Ssid("wifi");

    mac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer staDev = wifi.Install(phy, mac, staNode);

    mac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDev = wifi.Install(phy, mac, apNode);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNode);
    apNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));
    staNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(distance, 0.0, 0.0));

    InternetStackHelper internet;
    internet.Install(apNode);
    internet.Install(staNode);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.0.0", "255.255.255.0");
    Ipv4InterfaceContainer apIf = ipv4.Assign(apDev);
    Ipv4InterfaceContainer staIf = ipv4.Assign(staDev);

    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(apNode.Get(0));
    serverApp.Start(Seconds(0));
    serverApp.Stop(Seconds(5));

    UdpClientHelper client(apIf.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(0xffffffff));
    client.SetAttribute("Interval", TimeValue(MicroSeconds(100)));
    client.SetAttribute("PacketSize", UintegerValue(1200));
    ApplicationContainer clientApp = client.Install(staNode.Get(0));
    clientApp.Start(Seconds(1));
    clientApp.Stop(Seconds(5));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(5));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    Result result;
    for (const auto& s : stats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(s.first);
        if (t.destinationAddress == apIf.GetAddress(0))
        {
            if (s.second.rxPackets > 0 &&
                s.second.timeLastRxPacket > s.second.timeFirstTxPacket)
            {
                result.throughputMbps = s.second.rxBytes * 8.0 /
                                      (s.second.timeLastRxPacket.GetSeconds() -
                                       s.second.timeFirstTxPacket.GetSeconds()) / 1e6;
                result.delayMs = s.second.delaySum.GetSeconds() / s.second.rxPackets * 1000;
            }
        }
    }

    Simulator::Destroy();
    return result;
}
} // namespace

int
main(int argc, char* argv[])
{
    std::string distancesStr = "1,5,10";
    std::string mcsStr = "0,5,11";

    CommandLine cmd(__FILE__);
    cmd.AddValue("distances", "Comma separated list of distances (m)", distancesStr);
    cmd.AddValue("mcs", "Comma separated list of MCS indexes", mcsStr);
    cmd.Parse(argc, argv);

    auto distances = ParseDoubles(distancesStr);
    auto mcsValues = ParseMcs(mcsStr);

    if (distances.empty() || mcsValues.empty())
    {
        NS_LOG_UNCOND("No distances or MCS values provided");
        return 1;
    }

    for (WifiStandard standard : {WIFI_STANDARD_80211ax, WIFI_STANDARD_80211be})
    {
        std::string stdName = standard == WIFI_STANDARD_80211be ? "Wi-Fi 7" : "Wi-Fi 6";
        for (double d : distances)
        {
            for (uint8_t mcs : mcsValues)
            {
                Result r = RunExperiment(standard, d, mcs);
                std::cout << stdName << " distance=" << d << "m MCS=" << unsigned(mcs)
                          << " throughput=" << r.throughputMbps << " Mbit/s"
                          << " delay=" << r.delayMs << " ms" << std::endl;
            }
        }
    }

    return 0;
}

