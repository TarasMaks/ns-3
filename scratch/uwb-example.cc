/*
 * Ultra Wideband (UWB) spectrum example for ns-3.
 *
 * This scratch program demonstrates how to build a minimal custom
 * spectrum-aware PHY to model short UWB pulses using the ns-3 spectrum
 * framework.  Two nodes exchange a burst of pulses across a
 * SingleModelSpectrumChannel with Friis propagation loss.  The custom
 * UwbSpectrumPhy class defined in this file extends SpectrumPhy, emits
 * wideband pulses, and reports reception events with timing and power
 * information to showcase ranging-style measurements that are typical
 * of UWB systems.
 */

#include "ns3/core-module.h"
#include "ns3/isotropic-antenna-model.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/non-communicating-net-device.h"
#include "ns3/spectrum-module.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UwbExample");

namespace ns3
{

/**
 * Custom signal parameters that travel over the SpectrumChannel.
 *
 * They extend SpectrumSignalParameters to carry the payload packet,
 * the pulse duration, transmit timestamp, and a sequence identifier so
 * receivers can compute time-of-flight and differentiate pulses.
 */
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
typedef Callback<void, Ptr<const Packet>, uint32_t, Time, double, double> UwbReceptionCallback;

/**
 * Simple spectrum-aware PHY that emits rectangular UWB pulses.
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

    Time propagationDelay = Simulator::Now() - uwbParams->txTime;
    double snr = CalculateReceivedSnr(rxPowerW);

    if (!m_rxCallback.IsNull())
    {
        double rxPowerDbm = (rxPowerW > 0.0) ? 10.0 * std::log10(rxPowerW) + 30.0
                                             : -std::numeric_limits<double>::infinity();
        double snrDb = std::isfinite(snr) ? 10.0 * std::log10(snr)
                                          : std::numeric_limits<double>::infinity();
        m_rxCallback(uwbParams->packet, uwbParams->sequenceId, propagationDelay, rxPowerDbm, snrDb);
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
void
SchedulePulseBurst(Ptr<UwbSpectrumPhy> phy,
                   uint32_t pulseCount,
                   Time startTime,
                   Time spacing,
                   uint32_t payloadSize)
{
    Time t = startTime;
    for (uint32_t i = 0; i < pulseCount; ++i)
    {
        Simulator::Schedule(t, &SendSinglePulse, phy, payloadSize);
        t += spacing;
    }
}

} // namespace ns3

/** Print reception results. */
void
OnUwbPulse(Ptr<const Packet> packet,
           uint32_t sequenceId,
           Time propagationDelay,
           double rxPowerDbm,
           double snrDb)
{
    std::ostringstream oss;
    oss.setf(std::ios::fixed, std::ios::floatfield);
    const double c = 299792458.0; // speed of light [m/s]
    double distance = propagationDelay.GetSeconds() * c;
    oss << "t=" << std::setprecision(9) << Simulator::Now().GetSeconds() << " s"
        << " seq=" << sequenceId
        << " flight=" << std::setprecision(3) << propagationDelay.GetNanoSeconds() << " ns"
        << " distance≈" << std::setprecision(2) << distance << " m"
        << " Prx=" << std::setprecision(2) << rxPowerDbm << " dBm";
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
    double distance = 5.0;              // meters
    double centerFrequency = 6.5e9;     // Hz
    double bandwidth = 500e6;           // Hz
    double toneResolution = 10e6;       // Hz
    double txPowerDbm = 0.0;            // dBm
    double noiseFigureDb = 6.0;         // dB
    uint32_t pulseCount = 5;            // pulses in burst
    uint32_t payloadSize = 2;           // bytes
    Time pulseDuration = NanoSeconds(2);
    Time pulseSpacing = NanoSeconds(200);
    Time burstStart = MicroSeconds(1);
    bool verbose = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("distance", "Separation between transmitter and receiver (m)", distance);
    cmd.AddValue("centerFrequency", "Center frequency of the UWB emission (Hz)", centerFrequency);
    cmd.AddValue("bandwidth", "Occupied bandwidth of the pulse (Hz)", bandwidth);
    cmd.AddValue("toneResolution",
                 "Width of each spectral sample used to discretize the pulse (Hz)",
                 toneResolution);
    cmd.AddValue("txPowerDbm", "Total transmit power contained in a pulse (dBm)", txPowerDbm);
    cmd.AddValue("noiseFigureDb", "Receiver noise figure (dB)", noiseFigureDb);
    cmd.AddValue("pulseCount", "Number of pulses to emit in the burst", pulseCount);
    cmd.AddValue("payloadSize", "Payload bytes attached to each pulse", payloadSize);
    cmd.AddValue("pulseDuration", "Pulse duration", pulseDuration);
    cmd.AddValue("pulseSpacing", "Spacing between consecutive pulses", pulseSpacing);
    cmd.AddValue("burstStart", "Time when the first pulse is transmitted", burstStart);
    cmd.AddValue("verbose", "Enable detailed logging", verbose);
    cmd.Parse(argc, argv);

    if (pulseCount == 0)
    {
        pulseCount = 1;
    }
    if (toneResolution > bandwidth)
    {
        toneResolution = bandwidth;
    }

    if (verbose)
    {
        LogComponentEnable("UwbExample", LOG_LEVEL_INFO);
        LogComponentEnable("SingleModelSpectrumChannel", LOG_LEVEL_INFO);
    }

    NodeContainer nodes;
    nodes.Create(2);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);
    nodes.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));
    nodes.Get(1)->GetObject<MobilityModel>()->SetPosition(Vector(distance, 0.0, 0.0));

    Ptr<SingleModelSpectrumChannel> channel = CreateObject<SingleModelSpectrumChannel>();
    channel->SetPropagationDelayModel(CreateObject<ConstantSpeedPropagationDelayModel>());
    Ptr<FriisSpectrumPropagationLossModel> loss = CreateObject<FriisSpectrumPropagationLossModel>();
    channel->AddSpectrumPropagationLossModel(loss);

    Ptr<UwbSpectrumPhy> txPhy = CreateObject<UwbSpectrumPhy>();
    Ptr<UwbSpectrumPhy> rxPhy = CreateObject<UwbSpectrumPhy>();

    Ptr<NonCommunicatingNetDevice> txDevice = CreateObject<NonCommunicatingNetDevice>();
    Ptr<NonCommunicatingNetDevice> rxDevice = CreateObject<NonCommunicatingNetDevice>();

    txDevice->SetNode(nodes.Get(0));
    nodes.Get(0)->AddDevice(txDevice);
    txDevice->SetPhy(txPhy);
    txDevice->SetChannel(channel);

    rxDevice->SetNode(nodes.Get(1));
    nodes.Get(1)->AddDevice(rxDevice);
    rxDevice->SetPhy(rxPhy);
    rxDevice->SetChannel(channel);

    txPhy->SetDevice(txDevice);
    rxPhy->SetDevice(rxDevice);
    txPhy->SetMobility(nodes.Get(0)->GetObject<MobilityModel>());
    rxPhy->SetMobility(nodes.Get(1)->GetObject<MobilityModel>());
    txPhy->SetChannel(channel);
    rxPhy->SetChannel(channel);

    Ptr<IsotropicAntennaModel> txAntenna = CreateObject<IsotropicAntennaModel>();
    Ptr<IsotropicAntennaModel> rxAntenna = CreateObject<IsotropicAntennaModel>();
    txPhy->SetAntenna(txAntenna);
    rxPhy->SetAntenna(rxAntenna);

    channel->AddRx(txPhy);
    channel->AddRx(rxPhy);

    Ptr<SpectrumValue> txPsd =
        CreateUwbPowerSpectralDensity(centerFrequency, bandwidth, txPowerDbm, toneResolution);
    txPhy->SetTxPowerSpectralDensity(txPsd);
    txPhy->SetPulseDuration(pulseDuration);

    Ptr<SpectrumValue> noise = CreateThermalNoisePsd(txPsd->GetSpectrumModel(), noiseFigureDb);
    rxPhy->SetNoisePowerSpectralDensity(noise);
    rxPhy->SetPulseDuration(pulseDuration);
    rxPhy->SetReceiveCallback(MakeCallback(&OnUwbPulse));

    SchedulePulseBurst(txPhy, pulseCount, burstStart, pulseSpacing, payloadSize);

    uint32_t spacingCount = (pulseCount > 0) ? (pulseCount - 1) : 0;
    Time lastPulseOffset = Seconds(spacingCount * pulseSpacing.GetSeconds());
    Simulator::Stop(burstStart + lastPulseOffset + MilliSeconds(1));

    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
