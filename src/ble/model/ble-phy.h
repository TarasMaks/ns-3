/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef BLE_PHY_H
#define BLE_PHY_H

#include "ble-propagation-loss-model.h"

#include "ns3/data-rate.h"
#include "ns3/event-id.h"
#include "ns3/mobility-model.h"
#include "ns3/net-device.h"
#include "ns3/non-communicating-net-device.h"
#include "ns3/packet.h"
#include "ns3/random-variable-stream.h"
#include "ns3/spectrum-signal-parameters.h"
#include "ns3/spectrum-interference.h"
#include "ns3/spectrum-phy.h"
#include "ns3/spectrum-value.h"
#include "ns3/traced-callback.h"

namespace ns3
{

class SpectrumErrorModel;

struct BleSpectrumSignalParameters : public SpectrumSignalParameters
{
    BleSpectrumSignalParameters();
    BleSpectrumSignalParameters(const BleSpectrumSignalParameters& p);

    Ptr<SpectrumSignalParameters> Copy() const override;

    Ptr<Packet> packet;   //!< Packet being transmitted
    uint8_t channelIndex; //!< BLE channel index
};

/**
 * \ingroup ble
 * \brief Minimal BLE PHY built on SpectrumPhy.
 */
class BlePhy : public SpectrumPhy
{
  public:
    static TypeId GetTypeId();

    BlePhy();
    ~BlePhy() override;

    // Inherited from SpectrumPhy
    void SetChannel(Ptr<SpectrumChannel> c) override;
    void SetMobility(Ptr<MobilityModel> m) override;
    void SetDevice(Ptr<NetDevice> d) override;
    Ptr<MobilityModel> GetMobility() const override;
    Ptr<NetDevice> GetDevice() const override;
    Ptr<const SpectrumModel> GetRxSpectrumModel() const override;
    Ptr<Object> GetAntenna() const override;
    void StartRx(Ptr<SpectrumSignalParameters> params) override;

    /**
     * Start a BLE transmission.
     * @param p packet to send
     * @return true if transmission started
     */
    bool StartTx(Ptr<Packet> p);

    /**
     * Setters for callbacks to observe packet receptions.
     */
    void SetRxSuccessCallback(Callback<void, Ptr<Packet>> cb);
    void SetRxErrorCallback(Callback<void, Ptr<Packet>> cb);

    /**
     * Force regeneration of PSDs (e.g., after attribute changes).
     */
    void RecomputeSpectrumModel();

  private:
    enum State
    {
        IDLE,
        TX,
        RX
    };

    void DoDispose() override;
    void EndTx();
    void EndRx();

    Ptr<const SpectrumModel> CreateBleSpectrumModel() const;
    Ptr<SpectrumValue> CreateTxPsd() const;
    Ptr<SpectrumValue> CreateNoisePsd() const;
    double GetCenterFrequencyHz() const;

    void NotifySignal(Ptr<const SpectrumValue> spd, Time duration);

    Ptr<SpectrumChannel> m_channel;
    Ptr<MobilityModel> m_mobility;
    Ptr<NetDevice> m_device;
    Ptr<AntennaModel> m_antenna;
    Ptr<const SpectrumModel> m_rxSpectrumModel;
    Ptr<SpectrumInterference> m_interference;
    Ptr<SpectrumErrorModel> m_errorModel;

    Ptr<Packet> m_currentRxPacket;
    EventId m_endRxEvent;
    EventId m_endTxEvent;
    State m_state;

    // Attributes
    uint8_t m_channelIndex;
    double m_channelBandwidth;
    uint32_t m_subBands;
    double m_txPowerDbm;
    double m_noiseFigureDb;
    DataRate m_dataRate;
    double m_sinrThresholdDb;

    TracedCallback<Ptr<const Packet>> m_rxOkTrace;
    TracedCallback<Ptr<const Packet>> m_rxErrorTrace;

    Callback<void, Ptr<Packet>> m_rxSuccessCallback;
    Callback<void, Ptr<Packet>> m_rxErrorCallback;
};

} // namespace ns3

#endif /* BLE_PHY_H */
