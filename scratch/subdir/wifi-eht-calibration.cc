#include "ns3/command-line.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/mobility-helper.h"
#include "ns3/ssid.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/gnuplot.h"
#include "ns3/yans-error-rate-model.h"
#include <vector>
#include <sstream>
#include <fstream>
#include <cmath>
#include <string>

using namespace ns3;

namespace
{

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

double g_signalDbmAvg;
double g_noiseDbmAvg;
uint32_t g_samples;

void
MonitorSniffRx(Ptr<const Packet> /*packet*/,
               uint16_t /*channelFreqMhz*/,
               WifiTxVector /*txVector*/,
               MpduInfo /*aMpdu*/,
               SignalNoiseDbm signalNoise,
               uint16_t /*staId*/)
{
    g_samples++;
    g_signalDbmAvg += ((signalNoise.signal - g_signalDbmAvg) / g_samples);
    g_noiseDbmAvg += ((signalNoise.noise - g_noiseDbmAvg) / g_samples);
}

struct Result
{
    double throughputMbps{0};
    double snrDb{0};
};

Result
RunExperiment(double distance, const std::string& mcs, Time simulationTime)
{
    NodeContainer staNode;
    staNode.Create(1);
    NodeContainer apNode;
    apNode.Create(1);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

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
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNode);
    apNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));
    staNode.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(distance, 0.0, 0.0));

    InternetStackHelper stack;
    stack.Install(apNode);
    stack.Install(staNode);

    Ipv4AddressHelper address;
    address.SetBase("10.1.0.0", "255.255.255.0");
    Ipv4InterfaceContainer staIf = address.Assign(staDevice);
    Ipv4InterfaceContainer apIf = address.Assign(apDevice);

    uint16_t port = 9;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(staNode.Get(0));
    serverApp.Start(Seconds(0));
    serverApp.Stop(simulationTime + Seconds(1));

    UdpClientHelper client(staIf.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(0xffffffff));
    client.SetAttribute("Interval", TimeValue(Time("0.0001")));
    client.SetAttribute("PacketSize", UintegerValue(1472));
    ApplicationContainer clientApp = client.Install(apNode.Get(0));
    clientApp.Start(Seconds(1));
    clientApp.Stop(simulationTime + Seconds(1));

    Config::ConnectWithoutContext("/NodeList/0/DeviceList/*/Phy/MonitorSnifferRx",
                                  MakeCallback(&MonitorSniffRx));

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    g_signalDbmAvg = 0;
    g_noiseDbmAvg = 0;
    g_samples = 0;

    Simulator::Stop(simulationTime + Seconds(1));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    auto stats = monitor->GetFlowStats();
    double throughput = 0;
    for (const auto& s : stats)
    {
        auto t = classifier->FindFlow(s.first);
        if (t.destinationAddress == staIf.GetAddress(0))
        {
            throughput = s.second.rxBytes * 8.0 / simulationTime.GetSeconds() / 1e6;
            break;
        }
    }

    Simulator::Destroy();
    Config::Reset();

    Result r;
    r.throughputMbps = throughput;
    r.snrDb = g_signalDbmAvg - g_noiseDbmAvg;
    return r;
}

} // namespace

int
main(int argc, char* argv[])
{
    std::string distancesStr = "5,10,15";
    Time simulationTime{"10s"};
    CommandLine cmd(__FILE__);
    cmd.AddValue("distances", "Comma separated list of distances (m)", distancesStr);
    cmd.AddValue("simulationTime", "Simulation time", simulationTime);
    cmd.Parse(argc, argv);

    auto distances = ParseDoubles(distancesStr);
    if (distances.empty())
    {
        NS_LOG_UNCOND("No distances provided");
        return 1;
    }

    const std::vector<std::string> mcsStrings = {
        "EhtMcs0",  "EhtMcs1",  "EhtMcs2",  "EhtMcs3",  "EhtMcs4",
        "EhtMcs5",  "EhtMcs6",  "EhtMcs7",  "EhtMcs8",  "EhtMcs9",
        "EhtMcs10", "EhtMcs11", "EhtMcs12", "EhtMcs13"};

    Gnuplot throughputPlot("eht-throughput-vs-distance.eps");
    throughputPlot.SetLegend("Distance (m)", "Throughput (Mb/s)");
    Gnuplot sinrPlot("eht-sinr-vs-distance.eps");
    sinrPlot.SetLegend("Distance (m)", "SINR (dB)");
    Gnuplot berPlot("eht-ber-vs-snr.eps");
    berPlot.SetLegend("SINR (dB)", "BER");

    std::vector<double> sinrVsDistance(distances.size(), 0.0);

    Ptr<YansErrorRateModel> errModel = CreateObject<YansErrorRateModel>();

    for (std::size_t m = 0; m < mcsStrings.size(); ++m)
    {
        Gnuplot2dDataset throughputDataset(mcsStrings[m]);
        Gnuplot2dDataset berDataset(mcsStrings[m]);
        WifiTxVector txVector;
        txVector.SetMode(mcsStrings[m]);
        WifiMode mode(mcsStrings[m]);

        for (std::size_t d = 0; d < distances.size(); ++d)
        {
            Result r = RunExperiment(distances[d], mcsStrings[m], simulationTime);
            throughputDataset.Add(distances[d], r.throughputMbps);

            if (m == 0)
            {
                sinrVsDistance[d] = r.snrDb;
            }

            double snrLinear = std::pow(10.0, r.snrDb / 10.0);
            double ber = 1.0 - errModel->GetChunkSuccessRate(mode, txVector, snrLinear, 1);
            berDataset.Add(r.snrDb, ber);
        }
        throughputPlot.AddDataset(throughputDataset);
        berPlot.AddDataset(berDataset);
    }

    Gnuplot2dDataset sinrDataset("SINR");
    for (std::size_t d = 0; d < distances.size(); ++d)
    {
        sinrDataset.Add(distances[d], sinrVsDistance[d]);
    }
    sinrPlot.AddDataset(sinrDataset);

    std::ofstream throughputFile("eht-throughput-vs-distance.plt");
    throughputPlot.GenerateOutput(throughputFile);
    throughputFile.close();

    std::ofstream sinrFile("eht-sinr-vs-distance.plt");
    sinrPlot.GenerateOutput(sinrFile);
    sinrFile.close();

    std::ofstream berFile("eht-ber-vs-snr.plt");
    berPlot.GenerateOutput(berFile);
    berFile.close();

    return 0;
}

