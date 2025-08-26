#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/socket.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiLteTrafficExample");

class LteVoipApplication : public Application
{
public:
  LteVoipApplication();
  void Setup(Address peer, uint32_t pktSize, Time interval);

private:
  virtual void StartApplication() override;
  virtual void StopApplication() override;
  void SendPacket();

  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_pktSize{160};
  Time m_interval{MilliSeconds(20)};
  EventId m_sendEvent;
};

LteVoipApplication::LteVoipApplication() {}

void
LteVoipApplication::Setup(Address peer, uint32_t pktSize, Time interval)
{
  m_peer = peer;
  m_pktSize = pktSize;
  m_interval = interval;
}

void
LteVoipApplication::StartApplication()
{
  m_socket = Socket::CreateSocket(GetNode(), UdpSocketFactory::GetTypeId());
  m_socket->Connect(m_peer);
  m_socket->SetIpTos(0xb8); // EF DSCP for voice
  SendPacket();
}

void
LteVoipApplication::StopApplication()
{
  if (m_sendEvent.IsRunning())
  {
    Simulator::Cancel(m_sendEvent);
  }
  if (m_socket)
  {
    m_socket->Close();
  }
}

void
LteVoipApplication::SendPacket()
{
  Ptr<Packet> p = Create<Packet>(m_pktSize);
  SocketPriorityTag prio;
  prio.SetPriority(6); // map to AC_VO
  p->AddPacketTag(prio);
  m_socket->Send(p);
  m_sendEvent = Simulator::Schedule(m_interval, &LteVoipApplication::SendPacket, this);
}

int
main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);

  NodeContainer staNode;
  staNode.Create(1);
  NodeContainer apNode;
  apNode.Create(1);

  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211be);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue("EhtMcs9"),
                               "ControlMode", StringValue("EhtMcs9"));

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy;
  phy.SetChannel(channel.Create());

  WifiMacHelper mac;
  Ssid ssid = Ssid("ns3-80211be");
  mac.SetType("ns3::StaWifiMac",
              "Ssid", SsidValue(ssid),
              "QosSupported", BooleanValue(true));
  NetDeviceContainer staDevice = wifi.Install(phy, mac, staNode);
  mac.SetType("ns3::ApWifiMac",
              "Ssid", SsidValue(ssid),
              "QosSupported", BooleanValue(true));
  NetDeviceContainer apDevice = wifi.Install(phy, mac, apNode);

  MobilityHelper mobility;
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(staNode);
  mobility.Install(apNode);

  InternetStackHelper stack;
  stack.Install(staNode);
  stack.Install(apNode);

  Ipv4AddressHelper address;
  address.SetBase("192.168.1.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterface = address.Assign(staDevice);
  Ipv4InterfaceContainer apInterface = address.Assign(apDevice);

  uint16_t port = 5000;
  PacketSinkHelper sinkHelper("ns3::UdpSocketFactory",
                              InetSocketAddress(Ipv4Address::GetAny(), port));
  ApplicationContainer sinkApp = sinkHelper.Install(staNode.Get(0));
  sinkApp.Start(Seconds(0.0));
  sinkApp.Stop(Seconds(10.0));

  Ptr<LteVoipApplication> voiceApp = CreateObject<LteVoipApplication>();
  voiceApp->Setup(InetSocketAddress(staInterface.GetAddress(0), port), 160,
                  MilliSeconds(20));
  apNode.Get(0)->AddApplication(voiceApp);
  voiceApp->SetStartTime(Seconds(1.0));
  voiceApp->SetStopTime(Seconds(10.0));

  // Optional uplink voice flow
  PacketSinkHelper sinkHelper2("ns3::UdpSocketFactory",
                               InetSocketAddress(Ipv4Address::GetAny(), port + 1));
  ApplicationContainer sinkApp2 = sinkHelper2.Install(apNode.Get(0));
  sinkApp2.Start(Seconds(0.0));
  sinkApp2.Stop(Seconds(10.0));

  Ptr<LteVoipApplication> voiceApp2 = CreateObject<LteVoipApplication>();
  voiceApp2->Setup(InetSocketAddress(apInterface.GetAddress(0), port + 1), 160,
                   MilliSeconds(20));
  staNode.Get(0)->AddApplication(voiceApp2);
  voiceApp2->SetStartTime(Seconds(1.0));
  voiceApp2->SetStopTime(Seconds(10.0));

  FlowMonitorHelper flowmonHelper;
  Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

  Simulator::Stop(Seconds(10.0));
  Simulator::Run();
  monitor->CheckForLostPackets();
  double simTime = Simulator::Now().GetSeconds();
  for (const auto& statsPair : monitor->GetFlowStats())
  {
      auto stats = statsPair.second;
      double throughputKbps = (stats.rxBytes * 8.0) / simTime / 1000.0;
      double avgDelayMs = stats.delaySum.GetSeconds() / stats.rxPackets * 1000.0;
      NS_LOG_UNCOND("Flow " << statsPair.first 
                   << ": throughput=" << throughputKbps << " kbps, "
                   << "latency=" << avgDelayMs << " ms");
  }
  Simulator::Destroy();

  return 0;
}

