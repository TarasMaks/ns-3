/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ble-phy.h"

#include "ns3/antenna-model.h"
#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/isotropic-antenna-model.h"
#include "ns3/log.h"
#include "ns3/non-communicating-net-device.h"
#include "ns3/simulator.h"
#include "ns3/spectrum-channel.h"
#include "ns3/spectrum-error-model.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("BlePhy");

namespace
{
class BleThresholdErrorModel : public SpectrumErrorModel
{
  public:
    static TypeId GetTypeId();

    BleThresholdErrorModel();

    void SetThresholdDb(double thresholdDb);

    // SpectrumErrorModel overrides
    void StartRx(Ptr<const Packet> p) override;
    void EvaluateChunk(const SpectrumValue& sinr, Time duration) override;
    bool IsRxCorrect() override;

  private:
    double m_thresholdDb;
    double m_minSinr;
    Ptr<const Packet> m_packet;
};

BleThresholdErrorModel::BleThresholdErrorModel()
    : m_thresholdDb(6.0),
      m_minSinr(std::numeric_limits<double>::infinity())
{
}

TypeId
BleThresholdErrorModel::GetTypeId()
{
    static TypeId tid = TypeId("ns3::BleThresholdErrorModel")
                            .SetParent<SpectrumErrorModel>()
                            .SetGroupName("Ble")
                            .AddConstructor<BleThresholdErrorModel>();
    return tid;
}

void
BleThresholdErrorModel::SetThresholdDb(double thresholdDb)
{
    m_thresholdDb = thresholdDb;
}

void
BleThresholdErrorModel::StartRx(Ptr<const Packet> p)
{
    m_packet = p;
    m_minSinr = std::numeric_limits<double>::infinity();
}

void
BleThresholdErrorModel::EvaluateChunk(const SpectrumValue& sinr, Time duration)
{
    static_cast<void>(duration);
    double minChunk = *std::min_element(sinr.ConstValuesBegin(), sinr.ConstValuesEnd());
    m_minSinr = std::min(m_minSinr, minChunk);
}

bool
BleThresholdErrorModel::IsRxCorrect()
{
    if (m_minSinr == std::numeric_limits<double>::infinity())
    {
        return false;
    }
    double sinrDb = 10.0 * std::log10(m_minSinr);
    return sinrDb >= m_thresholdDb;
}
} // namespace

BleSpectrumSignalParameters::BleSpectrumSignalParameters()
    : packet(nullptr),
      channelIndex(37)
{
}

BleSpectrumSignalParameters::BleSpectrumSignalParameters(const BleSpectrumSignalParameters& p)
    : SpectrumSignalParameters(p),
      packet(p.packet ? p.packet->Copy() : nullptr),
      channelIndex(p.channelIndex)
{
}

Ptr<SpectrumSignalParameters>
BleSpectrumSignalParameters::Copy() const
{
    return Create<BleSpectrumSignalParameters>(*this);
}

NS_OBJECT_ENSURE_REGISTERED(BlePhy);

TypeId
BlePhy::GetTypeId()
{
    static TypeId tid = TypeId("ns3::BlePhy")
                            .SetParent<SpectrumPhy>()
                            .SetGroupName("Ble")
                            .AddConstructor<BlePhy>()
                            .AddAttribute("ChannelIndex", "BLE channel index (0-39)",
                                          UintegerValue(37),
                                          MakeUintegerAccessor(&BlePhy::m_channelIndex),
                                          MakeUintegerChecker<uint8_t>(0, 39))
                            .AddAttribute("ChannelBandwidth", "BLE channel bandwidth in Hz",
                                          DoubleValue(2e6),
                                          MakeDoubleAccessor(&BlePhy::m_channelBandwidth),
                                          MakeDoubleChecker<double>(1e3))
                            .AddAttribute("SubBands", "Number of spectrum sub-bands used to model 2 MHz",
                                          UintegerValue(4),
                                          MakeUintegerAccessor(&BlePhy::m_subBands),
                                          MakeUintegerChecker<uint32_t>(1))
                            .AddAttribute("TxPower", "Transmit power in dBm",
                                          DoubleValue(0.0),
                                          MakeDoubleAccessor(&BlePhy::m_txPowerDbm),
                                          MakeDoubleChecker<double>())
                            .AddAttribute("NoiseFigure", "Receiver noise figure in dB",
                                          DoubleValue(5.0),
                                          MakeDoubleAccessor(&BlePhy::m_noiseFigureDb),
                                          MakeDoubleChecker<double>())
                            .AddAttribute("DataRate", "BLE PHY data rate",
                                          DataRateValue(DataRate("1000000bps")),
                                          MakeDataRateAccessor(&BlePhy::m_dataRate),
                                          MakeDataRateChecker())
                            .AddAttribute("SinrThreshold", "Threshold SINR in dB for successful reception",
                                          DoubleValue(6.0),
                                          MakeDoubleAccessor(&BlePhy::m_sinrThresholdDb),
                                          MakeDoubleChecker<double>())
                            .AddTraceSource("RxOk", "A packet was successfully received",
                                             MakeTraceSourceAccessor(&BlePhy::m_rxOkTrace),
                                             "ns3::Packet::TracedCallback")
                            .AddTraceSource("RxError", "A packet was received with error",
                                             MakeTraceSourceAccessor(&BlePhy::m_rxErrorTrace),
                                             "ns3::Packet::TracedCallback");
    return tid;
}

BlePhy::BlePhy()
    : m_state(IDLE),
      m_channelIndex(37),
      m_channelBandwidth(2e6),
      m_subBands(4),
      m_txPowerDbm(0.0),
      m_noiseFigureDb(5.0),
      m_dataRate(DataRate("1000000bps")),
      m_sinrThresholdDb(6.0)
{
    NS_LOG_FUNCTION(this);
    m_antenna = CreateObject<IsotropicAntennaModel>();
    m_interference = CreateObject<SpectrumInterference>();
    auto errorModel = CreateObject<BleThresholdErrorModel>();
    errorModel->SetThresholdDb(m_sinrThresholdDb);
    m_errorModel = errorModel;
    m_interference->SetErrorModel(m_errorModel);
    RecomputeSpectrumModel();
}

BlePhy::~BlePhy()
{
    NS_LOG_FUNCTION(this);
}

void
BlePhy::DoDispose()
{
    NS_LOG_FUNCTION(this);
    m_channel = nullptr;
    m_mobility = nullptr;
    m_device = nullptr;
    m_interference = nullptr;
    m_errorModel = nullptr;
    m_currentRxPacket = nullptr;
    SpectrumPhy::DoDispose();
}

void
BlePhy::SetChannel(Ptr<SpectrumChannel> c)
{
    NS_LOG_FUNCTION(this << c);
    m_channel = c;
    if (m_channel)
    {
        m_channel->AddRx(this);
    }
}

void
BlePhy::SetMobility(Ptr<MobilityModel> m)
{
    m_mobility = m;
}

void
BlePhy::SetDevice(Ptr<NetDevice> d)
{
    m_device = d;
}

Ptr<MobilityModel>
BlePhy::GetMobility() const
{
    return m_mobility;
}

Ptr<NetDevice>
BlePhy::GetDevice() const
{
    return m_device;
}

Ptr<const SpectrumModel>
BlePhy::GetRxSpectrumModel() const
{
    return m_rxSpectrumModel;
}

Ptr<Object>
BlePhy::GetAntenna() const
{
    return m_antenna;
}

void
BlePhy::NotifySignal(Ptr<const SpectrumValue> spd, Time duration)
{
    m_interference->AddSignal(spd, duration);
}

void
BlePhy::StartRx(Ptr<SpectrumSignalParameters> params)
{
    NS_LOG_FUNCTION(this << params);
    Ptr<BleSpectrumSignalParameters> bleParams = DynamicCast<BleSpectrumSignalParameters>(params);
    if (!bleParams)
    {
        NS_LOG_INFO("Non-BLE signal ignored");
        return;
    }

    Ptr<const SpectrumValue> rxPsd = params->psd;
    NotifySignal(rxPsd, params->duration);

    if (m_state == TX)
    {
        return;
    }

    if (m_state == RX)
    {
        return;
    }

    m_state = RX;
    m_currentRxPacket = bleParams->packet->Copy();
    m_interference->StartRx(m_currentRxPacket, rxPsd);
    m_endRxEvent = Simulator::Schedule(params->duration, &BlePhy::EndRx, this);
}

void
BlePhy::EndRx()
{
    NS_LOG_FUNCTION(this);
    bool ok = m_interference->EndRx();
    Ptr<Packet> delivered = m_currentRxPacket;
    m_currentRxPacket = nullptr;
    m_state = IDLE;

    if (!delivered)
    {
        return;
    }

    if (ok)
    {
        m_rxOkTrace(delivered);
        if (!m_rxSuccessCallback.IsNull())
        {
            m_rxSuccessCallback(delivered);
        }
    }
    else
    {
        m_rxErrorTrace(delivered);
        if (!m_rxErrorCallback.IsNull())
        {
            m_rxErrorCallback(delivered);
        }
    }
}

void
BlePhy::EndTx()
{
    NS_LOG_FUNCTION(this);
    m_state = IDLE;
}

bool
BlePhy::StartTx(Ptr<Packet> p)
{
    NS_LOG_FUNCTION(this << p);
    if (m_state != IDLE)
    {
        return false;
    }
    if (!m_channel)
    {
        return false;
    }

    if (!m_rxSpectrumModel)
    {
        RecomputeSpectrumModel();
    }

    Ptr<SpectrumValue> txPsd = CreateTxPsd();
    Ptr<BleSpectrumSignalParameters> params = Create<BleSpectrumSignalParameters>();
    params->psd = txPsd;
    params->txPhy = this;
    params->txAntenna = m_antenna;
    params->duration = Seconds(static_cast<double>(p->GetSize() * 8) / m_dataRate.GetBitRate());
    params->packet = p->Copy();
    params->channelIndex = m_channelIndex;

    m_state = TX;
    NotifySignal(txPsd, params->duration);
    m_channel->StartTx(params);
    m_endTxEvent = Simulator::Schedule(params->duration, &BlePhy::EndTx, this);
    return true;
}

Ptr<const SpectrumModel>
BlePhy::CreateBleSpectrumModel() const
{
    double center = GetCenterFrequencyHz();
    double bandWidth = m_channelBandwidth;
    double subBandWidth = bandWidth / static_cast<double>(m_subBands);
    std::vector<double> centers;
    centers.reserve(m_subBands);
    double firstCenter = center - (bandWidth / 2.0) + (subBandWidth / 2.0);
    for (uint32_t i = 0; i < m_subBands; ++i)
    {
        centers.push_back(firstCenter + i * subBandWidth);
    }
    return Create<const SpectrumModel>(centers);
}

Ptr<SpectrumValue>
BlePhy::CreateTxPsd() const
{
    Ptr<SpectrumValue> txPsd = Create<SpectrumValue>(m_rxSpectrumModel);
    double txPowerW = std::pow(10.0, (m_txPowerDbm - 30.0) / 10.0);
    double density = txPowerW / m_channelBandwidth;
    (*txPsd) = density;
    return txPsd;
}

Ptr<SpectrumValue>
BlePhy::CreateNoisePsd() const
{
    Ptr<SpectrumValue> noise = Create<SpectrumValue>(m_rxSpectrumModel);
    double noiseDbmPerHz = -174.0 + m_noiseFigureDb;
    double noiseWPerHz = std::pow(10.0, (noiseDbmPerHz - 30.0) / 10.0);
    (*noise) = noiseWPerHz;
    return noise;
}

double
BlePhy::GetCenterFrequencyHz() const
{
    if (m_channelIndex == 37)
    {
        return 2402000000.0;
    }
    if (m_channelIndex == 38)
    {
        return 2426000000.0;
    }
    if (m_channelIndex == 39)
    {
        return 2480000000.0;
    }
    return 2404000000.0 + static_cast<double>(m_channelIndex) * 2000000.0;
}

void
BlePhy::RecomputeSpectrumModel()
{
    m_rxSpectrumModel = CreateBleSpectrumModel();
    m_interference->SetNoisePowerSpectralDensity(CreateNoisePsd());
    auto errorModel = DynamicCast<BleThresholdErrorModel>(m_errorModel);
    if (errorModel)
    {
        errorModel->SetThresholdDb(m_sinrThresholdDb);
    }
}

void
BlePhy::SetRxSuccessCallback(Callback<void, Ptr<Packet>> cb)
{
    m_rxSuccessCallback = cb;
}

void
BlePhy::SetRxErrorCallback(Callback<void, Ptr<Packet>> cb)
{
    m_rxErrorCallback = cb;
}

} // namespace ns3
