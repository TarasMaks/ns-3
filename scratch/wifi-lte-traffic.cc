#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/socket.h"
#include "ns3/tag.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/wifi-mac-header.h"
#include "ns3/wifi-phy-common.h"
#include "ns3/wifi-phy.h"
#include "ns3/phy-entity.h"

#include <fstream>
#include <iomanip>
#include <limits>
#include <ostream>
#include <sstream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WifiLteTrafficExample");

class TrafficFlowTag : public Tag
{
public:
  TrafficFlowTag() = default;
  static TypeId GetTypeId();
  TypeId GetInstanceTypeId() const override;
  void Serialize(TagBuffer i) const override;
  void Deserialize(TagBuffer i) override;
  uint32_t GetSerializedSize() const override;
  void Print(std::ostream& os) const override;
  void SetFlowId(uint8_t flowId);
  uint8_t GetFlowId() const;

private:
  uint8_t m_flowId{0};
};

NS_OBJECT_ENSURE_REGISTERED(TrafficFlowTag);

TypeId
TrafficFlowTag::GetTypeId()
{
  static TypeId tid = TypeId("ns3::TrafficFlowTag")
                          .SetParent<Tag>()
                          .SetGroupName("Network")
                          .AddConstructor<TrafficFlowTag>();
  return tid;
}

TypeId
TrafficFlowTag::GetInstanceTypeId() const
{
  return GetTypeId();
}

void
TrafficFlowTag::Serialize(TagBuffer i) const
{
  i.WriteU8(m_flowId);
}

void
TrafficFlowTag::Deserialize(TagBuffer i)
{
  m_flowId = i.ReadU8();
}

uint32_t
TrafficFlowTag::GetSerializedSize() const
{
  return 1;
}

void
TrafficFlowTag::Print(std::ostream& os) const
{
  os << "flowId=" << static_cast<uint32_t>(m_flowId);
}

void
TrafficFlowTag::SetFlowId(uint8_t flowId)
{
  m_flowId = flowId;
}

uint8_t
TrafficFlowTag::GetFlowId() const
{
  return m_flowId;
}

static std::ofstream g_timelineLog;

namespace
{

uint32_t
ExtractIdFromContext(const std::string& context, const std::string& token)
{
  const auto tokenWithSlashes = std::string("/") + token + "/";
  auto pos = context.find(tokenWithSlashes);
  if (pos == std::string::npos)
  {
    return std::numeric_limits<uint32_t>::max();
  }
  pos += tokenWithSlashes.size();
  auto end = context.find('/', pos);
  auto idString = context.substr(pos, end - pos);
  return static_cast<uint32_t>(std::stoul(idString));
}

bool
PacketBelongsToTaggedFlow(Ptr<const Packet> packet, uint8_t& flowId)
{
  TrafficFlowTag flowTag;
  if (!packet->PeekPacketTag(flowTag))
  {
    return false;
  }

  Ptr<Packet> copy = packet->Copy();
  WifiMacHeader hdr;
  if (!copy->PeekHeader(hdr) || !hdr.IsData())
  {
    return false;
  }

  flowId = flowTag.GetFlowId();
  return true;
}

void
LogPacketEvent(const std::string& event,
               const std::string& context,
               Ptr<const Packet> packet,
               const std::string& detail = "")
{
  if (!g_timelineLog.is_open())
  {
    return;
  }

  uint8_t flowId = 0;
  if (!PacketBelongsToTaggedFlow(packet, flowId))
  {
    return;
  }

  const auto nodeId = ExtractIdFromContext(context, "NodeList");
  const auto deviceId = ExtractIdFromContext(context, "DeviceList");

  g_timelineLog << Simulator::Now().GetSeconds() << ',' << event << ',' << nodeId << ','
                << deviceId << ',' << static_cast<uint32_t>(flowId) << ',' << packet->GetSize()
                << ',' << detail << '\n';
}

void
WifiPhyTxBeginTrace(std::string context, Ptr<const Packet> packet, double txPowerW)
{
  std::ostringstream oss;
  oss << txPowerW;
  LogPacketEvent("PhyTxBegin", context, packet, oss.str());
}

void
WifiPhyRxBeginTrace(std::string context,
                    Ptr<const Packet> packet,
                    RxPowerWattPerChannelBand rxPowersW)
{
  std::ostringstream oss;
  if (!rxPowersW.empty())
  {
    oss << rxPowersW.begin()->second;
  }
  LogPacketEvent("PhyRxBegin", context, packet, oss.str());
}

void
WifiPhyRxDropTrace(std::string context, Ptr<const Packet> packet, WifiPhyRxfailureReason reason)
{
  std::ostringstream oss;
  oss << reason;
  LogPacketEvent("PhyRxDrop", context, packet, oss.str());
}

} // namespace

class LteVoipApplication : public Application
{
public:
  LteVoipApplication();
  void Setup(Address peer, uint32_t pktSize, Time interval, uint8_t flowId);

private:
  virtual void StartApplication() override;
  virtual void StopApplication() override;
  void SendPacket();

  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_pktSize{160};
  Time m_interval{MilliSeconds(20)};
  EventId m_sendEvent;
  uint8_t m_flowId{0};
};

LteVoipApplication::LteVoipApplication() {}

void
LteVoipApplication::Setup(Address peer, uint32_t pktSize, Time interval, uint8_t flowId)
{
  m_peer = peer;
  m_pktSize = pktSize;
  m_interval = interval;
  m_flowId = flowId;
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
  if (m_sendEvent.IsPending())
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
  TrafficFlowTag flowTag;
  flowTag.SetFlowId(m_flowId);
  p->AddPacketTag(flowTag);
  m_socket->Send(p);
  m_sendEvent = Simulator::Schedule(m_interval, &LteVoipApplication::SendPacket, this);
}

int
main(int argc, char *argv[])
{
  Time::SetResolution(Time::NS);

  std::string timelineLogFilename = "wifi-lte-traffic-log.csv";

  CommandLine cmd(__FILE__);
  cmd.AddValue("timelineLog", "CSV file capturing per-packet timeline information",
               timelineLogFilename);
  cmd.Parse(argc, argv);

  g_timelineLog.open(timelineLogFilename, std::ios::out | std::ios::trunc);
  if (!g_timelineLog.is_open())
  {
    NS_FATAL_ERROR("Unable to open timeline log file: " << timelineLogFilename);
  }
  g_timelineLog.setf(std::ios::fixed, std::ios::floatfield);
  g_timelineLog << std::setprecision(9);
  g_timelineLog << "time_s,event,node_id,device_id,flow_id,packet_size_bytes,detail" << '\n';

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

  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyTxBegin",
                  MakeCallback(&WifiPhyTxBeginTrace));
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxBegin",
                  MakeCallback(&WifiPhyRxBeginTrace));
  Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxDrop",
                  MakeCallback(&WifiPhyRxDropTrace));

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
                  MilliSeconds(20), 1);
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
                   MilliSeconds(20), 2);
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

  if (g_timelineLog.is_open())
  {
    g_timelineLog.close();
  }

  return 0;
}

