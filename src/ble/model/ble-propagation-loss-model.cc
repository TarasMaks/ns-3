/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ble-propagation-loss-model.h"

#include "ns3/double.h"
#include "ns3/log.h"
#include "ns3/mobility-model.h"
#include "ns3/spectrum-signal-parameters.h"

#include <algorithm>
#include <cmath>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("BlePropagationLossModel");

NS_OBJECT_ENSURE_REGISTERED(BlePropagationLossModel);

TypeId
BlePropagationLossModel::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::BlePropagationLossModel")
            .SetParent<SpectrumPropagationLossModel>()
            .SetGroupName("Ble")
            .AddConstructor<BlePropagationLossModel>()
            .AddAttribute("CenterFrequency", "Fallback center frequency in Hz when not provided by PSD.",
                          DoubleValue(2.4e9),
                          MakeDoubleAccessor(&BlePropagationLossModel::m_centerFrequencyHz),
                          MakeDoubleChecker<double>())
            .AddAttribute("ReferenceDistance", "Reference distance for the log-distance model (m)",
                          DoubleValue(1.0),
                          MakeDoubleAccessor(&BlePropagationLossModel::m_referenceDistance),
                          MakeDoubleChecker<double>(0.001))
            .AddAttribute("PathLossExponent", "Path loss exponent for the log-distance model",
                          DoubleValue(2.0),
                          MakeDoubleAccessor(&BlePropagationLossModel::m_pathLossExponent),
                          MakeDoubleChecker<double>())
            .AddAttribute("ShadowingSigma", "Log-normal shadowing sigma in dB (0 disables shadowing)",
                          DoubleValue(0.0),
                          MakeDoubleAccessor(&BlePropagationLossModel::m_shadowingSigmaDb),
                          MakeDoubleChecker<double>(0.0));
    return tid;
}

BlePropagationLossModel::BlePropagationLossModel()
    : m_centerFrequencyHz(2.4e9),
      m_referenceDistance(1.0),
      m_pathLossExponent(2.0),
      m_shadowingSigmaDb(0.0),
      m_shadowing(CreateObject<NormalRandomVariable>())
{
    NS_LOG_FUNCTION(this);
}

BlePropagationLossModel::~BlePropagationLossModel()
{
    NS_LOG_FUNCTION(this);
}

Ptr<SpectrumValue>
BlePropagationLossModel::DoCalcRxPowerSpectralDensity(Ptr<const SpectrumSignalParameters> params,
                                                      Ptr<const MobilityModel> a,
                                                      Ptr<const MobilityModel> b) const
{
    NS_LOG_FUNCTION(this << params << a << b);

    Ptr<const SpectrumValue> txPsd = params->psd;
    if (!txPsd)
    {
        return nullptr;
    }
    double distance = a->GetDistanceFrom(b);
    distance = std::max(distance, m_referenceDistance);

    double centerFrequency = m_centerFrequencyHz;
    if (txPsd && txPsd->GetSpectrumModel()->GetNumBands() > 0)
    {
        centerFrequency = txPsd->GetSpectrumModel()->Begin()->fc;
        auto model = txPsd->GetSpectrumModel();
        double sumFc = 0.0;
        uint32_t count = 0;
        for (auto it = model->Begin(); it != model->End(); ++it)
        {
            sumFc += it->fc;
            ++count;
        }
        if (count > 0)
        {
            centerFrequency = sumFc / count;
        }
    }

    double lambda = 299792458.0 / centerFrequency;
    double refLossDb = 20.0 * std::log10((4 * M_PI * m_referenceDistance) / lambda);
    double pathLossDb =
        refLossDb + 10.0 * m_pathLossExponent * std::log10(distance / m_referenceDistance);

    if (m_shadowingSigmaDb > 0.0)
    {
        pathLossDb += m_shadowing->GetValue(0.0, m_shadowingSigmaDb);
    }

    double pathGainLinear = std::pow(10.0, -pathLossDb / 10.0);
    Ptr<SpectrumValue> rxPsd = Create<SpectrumValue>(*txPsd);
    (*rxPsd) *= pathGainLinear;

    return rxPsd;
}

int64_t
BlePropagationLossModel::DoAssignStreams(int64_t stream)
{
    NS_LOG_FUNCTION(this << stream);
    m_shadowing->SetStream(stream);
    return 1;
}

} // namespace ns3
