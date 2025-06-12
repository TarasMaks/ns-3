/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/double.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/log.h"
#include "ns3/mobility-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/string.h"
#include "ns3/ssid.h"
#include "ns3/uinteger.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/yans-wifi-helper.h"

#include <fstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("NewWifiMultipleDevices");

int
main(int argc, char* argv[])
{
    double startPower1 = 1.0; // dBm
    double endPower1 = 5.0;   // dBm
    double startPower2 = 1.0; // dBm
    double endPower2 = 5.0;   // dBm
    double stepPower = 2.0;   // dB step
    std::string logFileName = "new-wifi-multiple-devices.log";
    Time simulationTime = Seconds(1.0);

    CommandLine cmd(__FILE__);
    cmd.AddValue("startPower1", "Starting TX power for transmitter 1 (dBm)", startPower1);
    cmd.AddValue("endPower1", "Ending TX power for transmitter 1 (dBm)", endPower1);
    cmd.AddValue("startPower2", "Starting TX power for transmitter 2 (dBm)", startPower2);
    cmd.AddValue("endPower2", "Ending TX power for transmitter 2 (dBm)", endPower2);
    cmd.AddValue("stepPower", "Step in TX power (dB)", stepPower);
    cmd.AddValue("logFile", "Output log file", logFileName);
    cmd.Parse(argc, argv);

    std::ofstream logFile(logFileName.c_str());
    logFile << "Ptx1(dBm)\tPtx2(dBm)\tThroughput1(Mbps)\tLost1\tThroughput2(Mbps)\tLost2"
            << std::endl;

    for (double p1 = startPower1; p1 <= endPower1; p1 += stepPower)
    {
        for (double p2 = startPower2; p2 <= endPower2; p2 += stepPower)
        {
            NodeContainer nodes;
            nodes.Create(4);

            YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
            YansWifiPhyHelper phy;
            phy.SetChannel(channel.Create());
            phy.Set("TxPowerLevels", UintegerValue(1));

            WifiHelper wifi;
            wifi.SetStandard(WIFI_STANDARD_80211a);
            wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                         "DataMode",
                                         StringValue("OfdmRate6Mbps"),
                                         "ControlMode",
                                         StringValue("OfdmRate6Mbps"));
            WifiMacHelper mac;
            mac.SetType("ns3::AdhocWifiMac");

            NetDeviceContainer devices;
            phy.Set("TxPowerStart", DoubleValue(p1));
            phy.Set("TxPowerEnd", DoubleValue(p1));
            devices.Add(wifi.Install(phy, mac, nodes.Get(0)));
            devices.Add(wifi.Install(phy, mac, nodes.Get(1)));
            phy.Set("TxPowerStart", DoubleValue(p2));
            phy.Set("TxPowerEnd", DoubleValue(p2));
            devices.Add(wifi.Install(phy, mac, nodes.Get(2)));
            devices.Add(wifi.Install(phy, mac, nodes.Get(3)));

            MobilityHelper mobility;
            Ptr<ListPositionAllocator> pos = CreateObject<ListPositionAllocator>();
            pos->Add(Vector(0.0, 0.0, 0.0));
            pos->Add(Vector(5.0, 0.0, 0.0));
            pos->Add(Vector(0.0, 5.0, 0.0));
            pos->Add(Vector(5.0, 5.0, 0.0));
            mobility.SetPositionAllocator(pos);
            mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
            mobility.Install(nodes);

            InternetStackHelper stack;
            stack.Install(nodes);

            Ipv4AddressHelper address;
            address.SetBase("10.1.1.0", "255.255.255.0");
            Ipv4InterfaceContainer i1 =
                address.Assign(NetDeviceContainer{devices.Get(0), devices.Get(1)});
            address.SetBase("10.1.2.0", "255.255.255.0");
            Ipv4InterfaceContainer i2 =
                address.Assign(NetDeviceContainer{devices.Get(2), devices.Get(3)});

            uint16_t port1 = 4000;
            uint16_t port2 = 5000;

            OnOffHelper onoff1("ns3::UdpSocketFactory", InetSocketAddress(i1.GetAddress(1), port1));
            onoff1.SetAttribute("DataRate", StringValue("5Mbps"));
            onoff1.SetAttribute("PacketSize", UintegerValue(1024));
            ApplicationContainer app1 = onoff1.Install(nodes.Get(0));
            app1.Start(Seconds(0.0));
            app1.Stop(simulationTime);
            PacketSinkHelper sink1("ns3::UdpSocketFactory",
                                   InetSocketAddress(Ipv4Address::GetAny(), port1));
            ApplicationContainer sinkApp1 = sink1.Install(nodes.Get(1));
            sinkApp1.Start(Seconds(0.0));
            sinkApp1.Stop(simulationTime);

            OnOffHelper onoff2("ns3::UdpSocketFactory", InetSocketAddress(i2.GetAddress(1), port2));
            onoff2.SetAttribute("DataRate", StringValue("5Mbps"));
            onoff2.SetAttribute("PacketSize", UintegerValue(1024));
            ApplicationContainer app2 = onoff2.Install(nodes.Get(2));
            app2.Start(Seconds(0.0));
            app2.Stop(simulationTime);
            PacketSinkHelper sink2("ns3::UdpSocketFactory",
                                   InetSocketAddress(Ipv4Address::GetAny(), port2));
            ApplicationContainer sinkApp2 = sink2.Install(nodes.Get(3));
            sinkApp2.Start(Seconds(0.0));
            sinkApp2.Stop(simulationTime);

            FlowMonitorHelper flowmon;
            Ptr<FlowMonitor> monitor = flowmon.InstallAll();

            Simulator::Stop(simulationTime);
            Simulator::Run();

            monitor->CheckForLostPackets();
            Ptr<Ipv4FlowClassifier> classifier =
                DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
            std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

            double tput1 = 0.0;
            double tput2 = 0.0;
            uint32_t lost1 = 0;
            uint32_t lost2 = 0;

            for (auto& s : stats)
            {
                Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(s.first);
                if (t.sourceAddress == i1.GetAddress(0) && t.destinationAddress == i1.GetAddress(1))
                {
                    tput1 = s.second.rxBytes * 8.0 / simulationTime.GetSeconds() / 1e6;
                    lost1 = s.second.lostPackets;
                }
                else if (t.sourceAddress == i2.GetAddress(0) &&
                         t.destinationAddress == i2.GetAddress(1))
                {
                    tput2 = s.second.rxBytes * 8.0 / simulationTime.GetSeconds() / 1e6;
                    lost2 = s.second.lostPackets;
                }
            }

            logFile << p1 << "\t" << p2 << "\t" << tput1 << "\t" << lost1 << "\t" << tput2 << "\t"
                    << lost2 << std::endl;

            Simulator::Destroy();
        }
    }
    logFile.close();

    return 0;
}
