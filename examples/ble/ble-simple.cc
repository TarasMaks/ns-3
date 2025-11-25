/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ns3/ble-phy.h"
#include "ns3/ble-propagation-loss-model.h"
#include "ns3/core-module.h"
#include "ns3/mobility-helper.h"
#include "ns3/multi-model-spectrum-channel.h"
#include "ns3/non-communicating-net-device.h"
#include "ns3/packet.h"

#include <array>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BleSimpleExample");

static void
LogRxOk(Ptr<Packet> p)
{
    NS_LOG_INFO("BLE packet received: " << p->GetSize() << " bytes");
}

static void
LogRxError(Ptr<Packet> p)
{
    NS_LOG_INFO("BLE packet error: " << p->GetSize() << " bytes");
}

int
main(int argc, char* argv[])
{
    double distance = 5.0;
    CommandLine cmd(__FILE__);
    cmd.AddValue("distance", "Distance between nodes", distance);
    cmd.Parse(argc, argv);

    NodeContainer nodes;
    nodes.Create(2);

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);
    nodes.Get(0)->GetObject<MobilityModel>()->SetPosition(Vector(0.0, 0.0, 0.0));
    nodes.Get(1)->GetObject<MobilityModel>()->SetPosition(Vector(distance, 0.0, 0.0));

    Ptr<MultiModelSpectrumChannel> channel = CreateObject<MultiModelSpectrumChannel>();
    Ptr<BlePropagationLossModel> loss = CreateObject<BlePropagationLossModel>();
    channel->AddSpectrumPropagationLossModel(loss);

    std::array<Ptr<BlePhy>, 2> phys;
    for (uint32_t i = 0; i < nodes.GetN(); ++i)
    {
        Ptr<NonCommunicatingNetDevice> dev = CreateObject<NonCommunicatingNetDevice>();
        dev->SetNode(nodes.Get(i));
        nodes.Get(i)->AddDevice(dev);

        phys[i] = CreateObject<BlePhy>();
        phys[i]->SetDevice(dev);
        phys[i]->SetMobility(nodes.Get(i)->GetObject<MobilityModel>());
        phys[i]->SetChannel(channel);
    }

    phys[1]->SetRxSuccessCallback(MakeCallback(&LogRxOk));
    phys[1]->SetRxErrorCallback(MakeCallback(&LogRxError));

    Simulator::Schedule(Seconds(1.0), &BlePhy::StartTx, phys[0], Create<Packet>(20));

    Simulator::Stop(Seconds(2.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
