/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef BLE_PROPAGATION_LOSS_MODEL_H
#define BLE_PROPAGATION_LOSS_MODEL_H

#include "ns3/random-variable-stream.h"
#include "ns3/spectrum-propagation-loss-model.h"

namespace ns3
{

/**
 * \ingroup ble
 * \brief Log-distance propagation model tailored for BLE spectrum signals.
 */
class BlePropagationLossModel : public SpectrumPropagationLossModel
{
  public:
    static TypeId GetTypeId();

    BlePropagationLossModel();
    ~BlePropagationLossModel() override;

  private:
    Ptr<SpectrumValue> DoCalcRxPowerSpectralDensity(
        Ptr<const SpectrumSignalParameters> params,
        Ptr<const MobilityModel> a,
        Ptr<const MobilityModel> b) const override;

    int64_t DoAssignStreams(int64_t stream) override;

    double m_centerFrequencyHz;    //!< Default frequency when spectrum model is unavailable
    double m_referenceDistance;    //!< Reference distance for path loss calculation
    double m_pathLossExponent;     //!< Path loss exponent
    double m_shadowingSigmaDb;     //!< Shadowing sigma in dB
    Ptr<NormalRandomVariable> m_shadowing; //!< Shadowing random variable
};

} // namespace ns3

#endif /* BLE_PROPAGATION_LOSS_MODEL_H */
