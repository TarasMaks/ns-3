/*
 * Multi-node Ultra Wideband (UWB) spectrum example for ns-3.
 *
 * This scratch program builds on the single-link UWB example and extends it to
 * a small network of devices that coexist in a shared wideband channel.  Each
 * node instantiates the custom UwbSpectrumPhy implemented here, schedules
 * random bursts of UWB pulses, and listens for pulses from every other node
 * through the ns-3 spectrum framework.  The callback exposed by the PHY reports
 * detailed timing, power, and SNR information, allowing the example to emulate
 * time-of-flight based ranging among many neighbors.
 */

#include "ns3/core-module.h"
#include "ns3/isotropic-antenna-model.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/non-communicating-net-device.h"
#include "ns3/spectrum-module.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UwbMultipleNodesExample");

namespace ns3
{

struct UwbSpectrumSignalParameters : public SpectrumSignalParameters
{
    Ptr<SpectrumSignalParameters> Copy() const override;

    UwbSpectrumSignalParameters();
    UwbSpectrumSignalParameters(const UwbSpectrumSignalParameters& other);

    Ptr<Packet> packet; //!< Payload associated with the pulse
    uint32_t sequenceId; //!< Sequence number of this pulse
    Time txTime;         //!< Transmit timestamp set by the sender
    Time pulseWidth;     //!< Width of the transmitted pulse
};

UwbSpectrumSignalParameters::UwbSpectrumSignalParameters()
    : sequenceId(0)
{
    NS_LOG_FUNCTION(this);
}

UwbSpectrumSignalParameters::UwbSpectrumSignalParameters(const UwbSpectrumSignalParameters& other)
    : SpectrumSignalParameters(other),
      sequenceId(other.sequenceId),
      txTime(other.txTime),
      pulseWidth(other.pulseWidth)
{
    NS_LOG_FUNCTION(this << &other);
    if (other.packet)
    {
        packet = other.packet->Copy();
    }
}

Ptr<SpectrumSignalParameters>
UwbSpectrumSignalParameters::Copy() const
{
    NS_LOG_FUNCTION(this);
    return Create<UwbSpectrumSignalParameters>(*this);
}

/** Callback invoked when a pulse is detected. */
typedef Callback<void,
                 Ptr<const Packet>,
                 uint32_t,
                 uint32_t,
                 Time,
                 double,
                 double>
    UwbReceptionCallback;

/**
 * Spectrum-aware PHY that emits rectangular UWB pulses.
 */
class UwbSpectrumPhy : public SpectrumPhy
{
  public:
    UwbSpectrumPhy();
    ~UwbSpectrumPhy() override = default;

    static TypeId GetTypeId();

    // Overrides from SpectrumPhy
    void SetDevice(Ptr<NetDevice> device) override;
    Ptr<NetDevice> GetDevice() const override;
    void SetMobility(Ptr<MobilityModel> mobility) override;
    Ptr<MobilityModel> GetMobility() const override;
    void SetChannel(Ptr<SpectrumChannel> channel) override;
    Ptr<const SpectrumModel> GetRxSpectrumModel() const override;
    Ptr<Object> GetAntenna() const override;
    void StartRx(Ptr<SpectrumSignalParameters> params) override;

    /** Configure the transmit spectrum. */
    void SetTxPowerSpectralDensity(Ptr<SpectrumValue> psd);
    /** Optional noise power spectral density used for SNR estimation. */
    void SetNoisePowerSpectralDensity(Ptr<const SpectrumValue> noise);
    /**
     * Set the receive callback that reports pulse receptions with
     * timing and power information.
     */
    void SetReceiveCallback(UwbReceptionCallback cb);
    /** Set the pulse duration emitted by the transmitter. */
    void SetPulseDuration(Time duration);
    /** Return the currently configured pulse duration. */
    Time GetPulseDuration() const;
    /** Set the detection threshold in Joules. */
    void SetDetectionThreshold(double energyJ);
    /** Get the detection threshold in Joules. */
    double GetDetectionThreshold() const;
    /** Attach an antenna model to the PHY. */
    void SetAntenna(Ptr<AntennaModel> antenna);

    /** Emit a single UWB pulse carrying the provided packet payload. */
    void TransmitPulse(Ptr<Packet> packet);

  protected:
    void DoDispose() override;

  private:
    double CalculateReceivedSnr(double rxPowerW) const;

    Ptr<NetDevice> m_device;                      //!< Owning NetDevice
    Ptr<MobilityModel> m_mobility;                //!< Mobility model
    Ptr<SpectrumChannel> m_channel;               //!< Connected channel
    Ptr<AntennaModel> m_antenna;                  //!< Antenna model
    Ptr<SpectrumValue> m_txPsd;                   //!< Transmit PSD
    Ptr<const SpectrumValue> m_noisePsd;          //!< Optional noise PSD
    Ptr<const SpectrumModel> m_rxSpectrumModel;   //!< Expected receive model
    Time m_pulseDuration;                         //!< Pulse width
    double m_detectionThreshold;                  //!< Energy threshold [J]
    UwbReceptionCallback m_rxCallback;            //!< Reception callback
    uint32_t m_nextSequenceId;                    //!< Sequence counter
};

NS_OBJECT_ENSURE_REGISTERED(UwbSpectrumPhy);

UwbSpectrumPhy::UwbSpectrumPhy()
    : m_pulseDuration(NanoSeconds(2)),
      m_detectionThreshold(1e-19),
      m_nextSequenceId(0)
{
    NS_LOG_FUNCTION(this);
}

TypeId
UwbSpectrumPhy::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::UwbSpectrumPhy")
            .SetParent<SpectrumPhy>()
            .SetGroupName("Spectrum")
            .AddConstructor<UwbSpectrumPhy>()
            .AddAttribute("PulseDuration",
                          "Duration of the transmitted UWB pulse.",
                          TimeValue(NanoSeconds(2)),
                          MakeTimeAccessor(&UwbSpectrumPhy::SetPulseDuration,
                                           &UwbSpectrumPhy::GetPulseDuration),
                          MakeTimeChecker(Seconds(0)))
            .AddAttribute("DetectionThreshold",
                          "Minimum received energy required to trigger the callback (Joules).",
                          DoubleValue(1e-19),
                          MakeDoubleAccessor(&UwbSpectrumPhy::SetDetectionThreshold,
                                             &UwbSpectrumPhy::GetDetectionThreshold),
                          MakeDoubleChecker<double>(0.0));
    return tid;
}

void
UwbSpectrumPhy::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_rxCallback = UwbReceptionCallback();
    m_txPsd = nullptr;
    m_noisePsd = nullptr;
    m_antenna = nullptr;
    m_channel = nullptr;
    m_mobility = nullptr;
    m_device = nullptr;
    SpectrumPhy::DoDispose();
}

void
UwbSpectrumPhy::SetDevice(Ptr<NetDevice> device)
{
    NS_LOG_FUNCTION(this << device);
    m_device = device;
}

Ptr<NetDevice>
UwbSpectrumPhy::GetDevice() const
{
    return m_device;
}

void
UwbSpectrumPhy::SetMobility(Ptr<MobilityModel> mobility)
{
    NS_LOG_FUNCTION(this << mobility);
    m_mobility = mobility;
}

Ptr<MobilityModel>
UwbSpectrumPhy::GetMobility() const
{
    return m_mobility;
}

void
UwbSpectrumPhy::SetChannel(Ptr<SpectrumChannel> channel)
{
    NS_LOG_FUNCTION(this << channel);
    m_channel = channel;
}

Ptr<const SpectrumModel>
UwbSpectrumPhy::GetRxSpectrumModel() const
{
    return m_rxSpectrumModel;
}

Ptr<Object>
UwbSpectrumPhy::GetAntenna() const
{
    return m_antenna;
}

void
UwbSpectrumPhy::SetAntenna(Ptr<AntennaModel> antenna)
{
    NS_LOG_FUNCTION(this << antenna);
    m_antenna = antenna;
}

void
UwbSpectrumPhy::SetTxPowerSpectralDensity(Ptr<SpectrumValue> psd)
{
    NS_LOG_FUNCTION(this << psd);
    m_txPsd = psd;
    if (psd)
    {
        m_rxSpectrumModel = psd->GetSpectrumModel();
    }
}

void
UwbSpectrumPhy::SetNoisePowerSpectralDensity(Ptr<const SpectrumValue> noise)
{
    NS_LOG_FUNCTION(this << noise);
    m_noisePsd = noise;
}

void
UwbSpectrumPhy::SetReceiveCallback(UwbReceptionCallback cb)
{
    m_rxCallback = cb;
}

void
UwbSpectrumPhy::SetPulseDuration(Time duration)
{
    NS_LOG_FUNCTION(this << duration);
    m_pulseDuration = duration;
}

Time
UwbSpectrumPhy::GetPulseDuration() const
{
    return m_pulseDuration;
}

double
UwbSpectrumPhy::GetDetectionThreshold() const
{
    return m_detectionThreshold;
}

void
UwbSpectrumPhy::SetDetectionThreshold(double energyJ)
{
    NS_LOG_FUNCTION(this << energyJ);
    m_detectionThreshold = energyJ;
}

void
UwbSpectrumPhy::TransmitPulse(Ptr<Packet> packet)
{
    NS_LOG_FUNCTION(this << packet);
    if (!m_channel || !m_txPsd)
    {
        NS_LOG_WARN("UWB PHY missing channel or transmit spectrum configuration");
        return;
    }

    Ptr<UwbSpectrumSignalParameters> params = Create<UwbSpectrumSignalParameters>();
    params->duration = m_pulseDuration;
    params->psd = m_txPsd;
    params->txPhy = GetObject<SpectrumPhy>();
    params->txAntenna = m_antenna;
    params->pulseWidth = m_pulseDuration;
    params->txTime = Simulator::Now();
    params->sequenceId = m_nextSequenceId++;
    if (packet)
    {
        params->packet = packet->Copy();
    }

    NS_LOG_INFO("Transmitting pulse seq=" << params->sequenceId << " at t=" << params->txTime);
    m_channel->StartTx(params);
}

double
UwbSpectrumPhy::CalculateReceivedSnr(double rxPowerW) const
{
    if (!m_noisePsd)
    {
        return std::numeric_limits<double>::infinity();
    }
    NS_ASSERT(m_noisePsd->GetSpectrumModel());
    double noisePowerW = Integral(*m_noisePsd);
    if (noisePowerW <= 0.0)
    {
        return std::numeric_limits<double>::infinity();
    }
    return rxPowerW / noisePowerW;
}

void
UwbSpectrumPhy::StartRx(Ptr<SpectrumSignalParameters> params)
{
    NS_LOG_FUNCTION(this << params);
    Ptr<UwbSpectrumSignalParameters> uwbParams =
        DynamicCast<UwbSpectrumSignalParameters>(params);
    if (!uwbParams)
    {
        NS_LOG_DEBUG("Received signal of unknown type; ignoring");
        return;
    }

    double rxPowerW = Integral(*uwbParams->psd);
    double energyJ = rxPowerW * uwbParams->duration.GetSeconds();

    NS_LOG_INFO("RX pulse seq=" << uwbParams->sequenceId << " power=" << rxPowerW << "W, energy="
                                << energyJ << "J");

    if (energyJ < m_detectionThreshold)
    {
        NS_LOG_DEBUG("Pulse below detection threshold");
        return;
    }

    uint32_t txNodeId = std::numeric_limits<uint32_t>::max();
    if (uwbParams->txPhy)
    {
        Ptr<UwbSpectrumPhy> txUwb = DynamicCast<UwbSpectrumPhy>(uwbParams->txPhy);
        if (txUwb)
        {
            Ptr<NetDevice> txDevice = txUwb->GetDevice();
            if (txDevice && txDevice->GetNode())
            {
                txNodeId = txDevice->GetNode()->GetId();
            }
        }
    }

    Time propagationDelay = Simulator::Now() - uwbParams->txTime;
    double snr = CalculateReceivedSnr(rxPowerW);

    if (!m_rxCallback.IsNull())
    {
        double rxPowerDbm = (rxPowerW > 0.0) ? 10.0 * std::log10(rxPowerW) + 30.0
                                             : -std::numeric_limits<double>::infinity();
        double snrDb = std::isfinite(snr) ? 10.0 * std::log10(snr)
                                          : std::numeric_limits<double>::infinity();
        m_rxCallback(uwbParams->packet,
                     uwbParams->sequenceId,
                     txNodeId,
                     propagationDelay,
                     rxPowerDbm,
                     snrDb);
    }
}

/**
 * Create a wideband transmit power spectral density covering the
 * provided bandwidth around the center frequency.
 */
Ptr<SpectrumValue>
CreateUwbPowerSpectralDensity(double centerHz,
                              double bandwidthHz,
                              double totalPowerDbm,
                              double resolutionHz)
{
    NS_ABORT_MSG_IF(bandwidthHz <= 0.0, "Bandwidth must be positive");
    NS_ABORT_MSG_IF(resolutionHz <= 0.0, "Resolution must be positive");

    uint32_t numBands = std::max(1u, static_cast<uint32_t>(std::ceil(bandwidthHz / resolutionHz)));
    double startFreq = centerHz - bandwidthHz / 2.0;

    Bands bands;
    double fl = startFreq;
    for (uint32_t i = 0; i < numBands; ++i)
    {
        BandInfo band;
        band.fl = fl;
        band.fc = fl + resolutionHz / 2.0;
        fl += resolutionHz;
        band.fh = fl;
        bands.push_back(band);
    }

    Ptr<SpectrumModel> model = Create<SpectrumModel>(bands);
    Ptr<SpectrumValue> psd = Create<SpectrumValue>(model);
    double powerW = std::pow(10.0, (totalPowerDbm - 30.0) / 10.0);
    double density = powerW / bandwidthHz;
    for (auto it = psd->ValuesBegin(); it != psd->ValuesEnd(); ++it)
    {
        *it = density;
    }
    return psd;
}

/** Create a flat thermal noise PSD for the given spectrum model. */
Ptr<SpectrumValue>
CreateThermalNoisePsd(Ptr<const SpectrumModel> model, double noiseFigureDb)
{
    Ptr<SpectrumValue> noise = Create<SpectrumValue>(model);
    const double kTDbmPerHz = -174.0;
    double noiseWPerHz = std::pow(10.0, (kTDbmPerHz - 30.0) / 10.0);
    double noiseFigureLinear = std::pow(10.0, noiseFigureDb / 10.0);
    double density = noiseWPerHz * noiseFigureLinear;
    for (auto it = noise->ValuesBegin(); it != noise->ValuesEnd(); ++it)
    {
        *it = density;
    }
    return noise;
}

/** Helper to schedule a single pulse emission. */
void
SendSinglePulse(Ptr<UwbSpectrumPhy> phy, uint32_t payloadSize)
{
    Ptr<Packet> packet = Create<Packet>(payloadSize);
    phy->TransmitPulse(packet);
}

/** Schedule a burst of pulses separated by a fixed spacing. */
Time
SchedulePulseBurst(Ptr<UwbSpectrumPhy> phy,
                   uint32_t pulseCount,
                   Time startTime,
                   Time spacing,
                   uint32_t payloadSize)
{
    Time t = startTime;
    Time last = startTime;
    for (uint32_t i = 0; i < pulseCount; ++i)
    {
        Simulator::Schedule(t, &SendSinglePulse, phy, payloadSize);
        last = t;
        t += spacing;
    }
    return last;
}

struct UwbNodeContext
{
    Ptr<Node> node;
    Ptr<UwbSpectrumPhy> phy;
    std::string label;
};

} // namespace ns3

using NodeLabelMap = std::map<uint32_t, std::string>;

/** Print reception results with node context. */
void
OnUwbPulse(const NodeLabelMap* labelMap,
           const std::string& receiverLabel,
           uint32_t receiverId,
           Ptr<const Packet> packet,
           uint32_t sequenceId,
           uint32_t transmitterId,
           Time propagationDelay,
           double rxPowerDbm,
           double snrDb)
{
    if (transmitterId == receiverId)
    {
        return; // Ignore self receptions if they arise.
    }

    std::string txLabel;
    if (transmitterId == std::numeric_limits<uint32_t>::max())
    {
        txLabel = "unknown";
    }
    else if (labelMap)
    {
        auto it = labelMap->find(transmitterId);
        if (it != labelMap->end())
        {
            txLabel = it->second;
        }
    }
    if (txLabel.empty())
    {
        txLabel = "Node" + std::to_string(transmitterId);
    }

    std::ostringstream oss;
    oss.setf(std::ios::fixed, std::ios::floatfield);
    oss << "t=" << std::setprecision(9) << Simulator::Now().GetSeconds() << " s "
        << receiverLabel << "(id=" << receiverId << ") received seq=" << sequenceId << " from "
        << txLabel;
    if (transmitterId != std::numeric_limits<uint32_t>::max())
    {
        oss << "(id=" << transmitterId << ")";
    }

    double flightNs = propagationDelay.GetNanoSeconds();
    double distance = propagationDelay.GetSeconds() * 299792458.0;
    oss << " flight=" << std::setprecision(3) << flightNs << " ns";
    oss << " distance≈" << std::setprecision(2) << distance << " m";
    oss << " Prx=" << std::setprecision(2) << rxPowerDbm << " dBm";
    if (std::isfinite(snrDb))
    {
        oss << " SNR=" << std::setprecision(2) << snrDb << " dB";
    }
    if (packet)
    {
        oss << " payload=" << packet->GetSize() << " B";
    }

    std::cout << oss.str() << std::endl;
}

int
main(int argc, char* argv[])
{
    uint32_t numNodes = 4;
    uint32_t burstsPerNode = 3;
    uint32_t pulsesPerBurst = 3;
    uint32_t payloadSize = 2;
    double deploymentRadius = 5.0;
    double centerFrequency = 6.5e9;
    double bandwidth = 500e6;
    double toneResolution = 10e6;
    double txPowerDbm = 0.0;
    double noiseFigureDb = 6.0;
    Time pulseDuration = NanoSeconds(2);
    Time pulseSpacing = NanoSeconds(200);
    Time firstBurstEarliest = MicroSeconds(1);
    Time firstBurstLatest = MicroSeconds(80);
    Time minInterBurst = MicroSeconds(50);
    Time maxInterBurst = MicroSeconds(200);
    uint32_t randomStream = 1;
    bool verbose = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("numNodes", "Number of UWB devices in the simulation", numNodes);
    cmd.AddValue("burstsPerNode", "How many bursts each node transmits", burstsPerNode);
    cmd.AddValue("pulsesPerBurst", "Number of pulses within each burst", pulsesPerBurst);
    cmd.AddValue("payloadSize", "Payload bytes attached to each pulse", payloadSize);
    cmd.AddValue("deploymentRadius", "Radius (m) of the circular deployment area", deploymentRadius);
    cmd.AddValue("centerFrequency", "Center frequency of the UWB emission (Hz)", centerFrequency);
    cmd.AddValue("bandwidth", "Occupied bandwidth of each pulse (Hz)", bandwidth);
    cmd.AddValue("toneResolution",
                 "Width of each spectral sample used to discretize the pulse (Hz)",
                 toneResolution);
    cmd.AddValue("txPowerDbm", "Total transmit power contained in a pulse (dBm)", txPowerDbm);
    cmd.AddValue("noiseFigureDb", "Receiver noise figure (dB)", noiseFigureDb);
    cmd.AddValue("pulseDuration", "Pulse duration", pulseDuration);
    cmd.AddValue("pulseSpacing", "Spacing between consecutive pulses", pulseSpacing);
    cmd.AddValue("firstBurstEarliest",
                 "Earliest possible start time for the first burst from any node",
                 firstBurstEarliest);
    cmd.AddValue("firstBurstLatest",
                 "Latest possible start time for the first burst from any node",
                 firstBurstLatest);
    cmd.AddValue("minInterBurst",
                 "Minimum gap between consecutive bursts from the same node",
                 minInterBurst);
    cmd.AddValue("maxInterBurst",
                 "Maximum gap between consecutive bursts from the same node",
                 maxInterBurst);
    cmd.AddValue("randomStream", "Stream index used for random burst scheduling", randomStream);
    cmd.AddValue("verbose", "Enable detailed logging", verbose);
    cmd.Parse(argc, argv);

    NS_ABORT_MSG_IF(numNodes < 2, "At least two UWB nodes are required");
    if (burstsPerNode == 0)
    {
        burstsPerNode = 1;
    }
    if (pulsesPerBurst == 0)
    {
        pulsesPerBurst = 1;
    }
    if (toneResolution > bandwidth)
    {
        toneResolution = bandwidth;
    }
    if (firstBurstLatest < firstBurstEarliest)
    {
        firstBurstLatest = firstBurstEarliest;
    }
    if (maxInterBurst < minInterBurst)
    {
        maxInterBurst = minInterBurst;
    }

    if (verbose)
    {
        LogComponentEnable("UwbMultipleNodesExample", LOG_LEVEL_INFO);
        LogComponentEnable("SingleModelSpectrumChannel", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(numNodes);

    Ptr<ListPositionAllocator> positions = CreateObject<ListPositionAllocator>();
    const double pi = 3.14159265358979323846;
    for (uint32_t i = 0; i < numNodes; ++i)
    {
        double angle = (numNodes > 1) ? (2.0 * pi * static_cast<double>(i) / numNodes) : 0.0;
        double x = deploymentRadius * std::cos(angle);
        double y = deploymentRadius * std::sin(angle);
        positions->Add(Vector(x, y, 0.0));
    }

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.SetPositionAllocator(positions);
    mobility.Install(nodes);

    Ptr<SingleModelSpectrumChannel> channel = CreateObject<SingleModelSpectrumChannel>();
    channel->SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());
    Ptr<FriisSpectrumPropagationLossModel> loss = CreateObject<FriisSpectrumPropagationLossModel>();
    channel->AddSpectrumPropagationLossModel(loss);

    Ptr<SpectrumValue> txPsd =
        CreateUwbPowerSpectralDensity(centerFrequency, bandwidth, txPowerDbm, toneResolution);
    Ptr<SpectrumValue> noise = CreateThermalNoisePsd(txPsd->GetSpectrumModel(), noiseFigureDb);

    std::vector<UwbNodeContext> contexts;
    contexts.reserve(numNodes);
    NodeLabelMap labelMap;

    for (uint32_t i = 0; i < numNodes; ++i)
    {
        Ptr<Node> node = nodes.Get(i);

        Ptr<UwbSpectrumPhy> phy = CreateObject<UwbSpectrumPhy>();
        Ptr<NonCommunicatingNetDevice> device = CreateObject<NonCommunicatingNetDevice>();
        device->SetNode(node);
        node->AddDevice(device);
        device->SetPhy(phy);
        device->SetChannel(channel);

        phy->SetDevice(device);
        phy->SetMobility(node->GetObject<MobilityModel>());
        phy->SetChannel(channel);
        Ptr<IsotropicAntennaModel> antenna = CreateObject<IsotropicAntennaModel>();
        phy->SetAntenna(antenna);
        phy->SetTxPowerSpectralDensity(txPsd);
        phy->SetPulseDuration(pulseDuration);
        phy->SetNoisePowerSpectralDensity(noise);
        channel->AddRx(phy);

        std::string label = "Node" + std::to_string(node->GetId());
        labelMap[node->GetId()] = label;
        phy->SetReceiveCallback(MakeBoundCallback(&OnUwbPulse, &labelMap, label, node->GetId()));

        contexts.push_back({node, phy, label});
    }

    std::cout << "Configured " << numNodes << " UWB nodes, "
              << burstsPerNode << " bursts/node and "
              << pulsesPerBurst << " pulses/burst." << std::endl;

    std::cout << "Node deployment:" << std::endl;
    for (const auto& ctx : contexts)
    {
        Vector pos = ctx.node->GetObject<MobilityModel>()->GetPosition();
        std::ostringstream line;
        line.setf(std::ios::fixed, std::ios::floatfield);
        line << "  " << ctx.label << " (id=" << ctx.node->GetId() << ") at ("
             << std::setprecision(2) << pos.x << ", " << std::setprecision(2) << pos.y << ", "
             << std::setprecision(2) << pos.z << ") m";
        std::cout << line.str() << std::endl;
    }

    Ptr<UniformRandomVariable> firstStartRng = CreateObject<UniformRandomVariable>();
    firstStartRng->SetAttribute("Min", DoubleValue(firstBurstEarliest.GetSeconds()));
    firstStartRng->SetAttribute("Max", DoubleValue(firstBurstLatest.GetSeconds()));
    Ptr<UniformRandomVariable> gapRng = CreateObject<UniformRandomVariable>();
    gapRng->SetAttribute("Min", DoubleValue(minInterBurst.GetSeconds()));
    gapRng->SetAttribute("Max", DoubleValue(maxInterBurst.GetSeconds()));
    firstStartRng->SetStream(randomStream);
    gapRng->SetStream(randomStream + 1);

    Time latestTransmission = Seconds(0);
    for (const auto& ctx : contexts)
    {
        Time nextBurstStart = Seconds(firstStartRng->GetValue());
        for (uint32_t burst = 0; burst < burstsPerNode; ++burst)
        {
            Time lastPulseTime =
                SchedulePulseBurst(ctx.phy, pulsesPerBurst, nextBurstStart, pulseSpacing, payloadSize);
            if (lastPulseTime > latestTransmission)
            {
                latestTransmission = lastPulseTime;
            }
            if (burst + 1 < burstsPerNode)
            {
                nextBurstStart += Seconds(gapRng->GetValue());
            }
        }
    }

    Time guardTime = pulseDuration + MicroSeconds(200);
    Simulator::Stop(latestTransmission + guardTime);

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
