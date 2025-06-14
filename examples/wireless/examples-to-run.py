#! /usr/bin/env python3

# A list of C++ examples to run in order to ensure that they remain
# buildable and runnable over time.  Each tuple in the list contains
#
#     (example_name, do_run, do_valgrind_run).
#
# See test.py for more information.
cpp_examples = [
    ("mixed-wired-wireless", "True", "True"),
    ("wifi-multirate --totalTime=0.3s --rateManager=ns3::AarfcdWifiManager", "True", "True"),
    ("wifi-multirate --totalTime=0.3s --rateManager=ns3::AmrrWifiManager", "True", "False"),
    ("wifi-multirate --totalTime=0.3s --rateManager=ns3::CaraWifiManager", "True", "False"),
    ("wifi-multirate --totalTime=0.3s --rateManager=ns3::IdealWifiManager", "True", "False"),
    ("wifi-multirate --totalTime=0.3s --rateManager=ns3::MinstrelWifiManager", "True", "False"),
    ("wifi-multirate --totalTime=0.3s --rateManager=ns3::OnoeWifiManager", "True", "False"),
    ("wifi-multirate --totalTime=0.3s --rateManager=ns3::RraaWifiManager", "True", "False"),
    ("wifi-adhoc", "False", "True"),  # Takes too long to run
    ("wifi-ap --verbose=0", "True", "True"),  # Don't let it spew to stdout
    ("wifi-clear-channel-cmu", "False", "True"),  # Requires specific hardware
    ("wifi-simple-adhoc", "True", "True"),
    ("wifi-simple-adhoc-grid", "True", "True"),
    ("wifi-simple-infra", "True", "True"),
    ("wifi-simple-interference", "True", "True"),
    ("wifi-phy-interference", "True", "True"),
    ("wifi-ble-interference", "True", "True"),
    ("wifi-wired-bridging", "True", "True"),
    ("wifi-sleep", "True", "True"),
    ("wifi-blockack", "True", "True"),
    ("wifi-timing-attributes --simulationTime=1s", "True", "True"),
    (
        "wifi-power-adaptation-distance --manager=ns3::ParfWifiManager --outputFileName=parf --steps=5 --stepsSize=10",
        "True",
        "True",
    ),
    (
        "wifi-power-adaptation-distance --manager=ns3::AparfWifiManager --outputFileName=aparf --steps=5 --stepsSize=10",
        "True",
        "False",
    ),
    (
        "wifi-power-adaptation-distance --manager=ns3::RrpaaWifiManager --outputFileName=rrpaa --steps=5 --stepsSize=10",
        "True",
        "False",
    ),
    (
        "wifi-rate-adaptation-distance --standard=802.11a --staManager=ns3::MinstrelWifiManager --apManager=ns3::MinstrelWifiManager --outputFileName=minstrel --stepsSize=50 --stepsTime=0.1",
        "True",
        "False",
    ),
    (
        "wifi-rate-adaptation-distance --standard=802.11a --staManager=ns3::MinstrelWifiManager --apManager=ns3::MinstrelWifiManager --outputFileName=minstrel --stepsSize=50 --stepsTime=0.1 --STA1_x=-200",
        "True",
        "False",
    ),
    (
        "wifi-rate-adaptation-distance --staManager=ns3::MinstrelHtWifiManager --apManager=ns3::MinstrelHtWifiManager --outputFileName=minstrelHt --shortGuardInterval=true --channelWidth=40 --stepsSize=50 --stepsTime=0.1",
        "True",
        "False",
    ),
    ("wifi-power-adaptation-interference --simuTime=5", "True", "False"),
    ("wifi-dsss-validation", "True", "True"),
    ("wifi-ofdm-validation", "True", "True"),
    ("wifi-ofdm-ht-validation", "True", "True"),
    ("wifi-ofdm-vht-validation", "True", "True"),
    ("wifi-ofdm-he-validation", "True", "True"),
    ("wifi-error-models-comparison", "True", "True"),
    ("wifi-80211n-mimo --simulationTime=0.1s --step=10", "True", "True"),
    (
        "wifi-ht-network --simulationTime=0.2s --frequency=5 --useRts=0 --minExpectedThroughput=5 --maxExpectedThroughput=135",
        "True",
        "True",
    ),
    (
        "wifi-ht-network --simulationTime=0.2s --frequency=5 --useRts=1 --minExpectedThroughput=5 --maxExpectedThroughput=132",
        "True",
        "True",
    ),
    (
        "wifi-ht-network --simulationTime=0.2s --frequency=2.4 --useRts=0 --minExpectedThroughput=5 --maxExpectedThroughput=132",
        "True",
        "True",
    ),
    (
        "wifi-ht-network --simulationTime=0.2s --frequency=2.4 --useRts=1 --minExpectedThroughput=5 --maxExpectedThroughput=129",
        "True",
        "True",
    ),
    (
        "wifi-vht-network --simulationTime=0.2s --useRts=0 --minExpectedThroughput=5 --maxExpectedThroughput=620",
        "True",
        "True",
    ),
    (
        "wifi-vht-network --simulationTime=0.2s --useRts=1 --minExpectedThroughput=5 --maxExpectedThroughput=557",
        "True",
        "True",
    ),
    (
        "wifi-vht-network --simulationTime=0.2s --useRts=0 --use80Plus80=1 --minExpectedThroughput=5 --maxExpectedThroughput=620",
        "True",
        "True",
    ),
    (
        "wifi-he-network --simulationTime=0.25s --frequency=5 --useRts=0 --minExpectedThroughput=6 --maxExpectedThroughput=844",
        "True",
        "True",
    ),
    (
        "wifi-he-network --simulationTime=0.25s --frequency=5 --useRts=0 --use80Plus80=1 --minExpectedThroughput=6 --maxExpectedThroughput=844",
        "True",
        "True",
    ),
    (
        "wifi-he-network --simulationTime=0.3s --frequency=5 --useRts=0 --useExtendedBlockAck=1 --minExpectedThroughput=6 --maxExpectedThroughput=1033",
        "True",
        "True",
    ),
    (
        "wifi-he-network --simulationTime=0.3s --frequency=5 --useRts=1 --minExpectedThroughput=6 --maxExpectedThroughput=745",
        "True",
        "True",
    ),
    (
        "wifi-he-network --simulationTime=0.25s --frequency=2.4 --useRts=0 --minExpectedThroughput=6 --maxExpectedThroughput=238",
        "True",
        "True",
    ),
    (
        "wifi-he-network --simulationTime=0.3s --frequency=2.4 --useRts=1 --minExpectedThroughput=6 --maxExpectedThroughput=223",
        "True",
        "True",
    ),
    (
        "wifi-he-network --simulationTime=0.3s --udp=0 --downlink=1 --useRts=0 --nStations=4 --dlAckType=ACK-SU-FORMAT --enableUlOfdma=1 --enableBsrp=0 --mcs=4 --minExpectedThroughput=20 --maxExpectedThroughput=212",
        "True",
        "True",
    ),
    (
        "wifi-he-network --simulationTime=0.3s --frequency=2.4 --udp=0 --downlink=1 --useRts=1 --nStations=5 --dlAckType=MU-BAR --enableUlOfdma=1 --enableBsrp=1 --mcs=5 --minExpectedThroughput=21 --maxExpectedThroughput=56",
        "True",
        "True",
    ),
    (
        "wifi-he-network --simulationTime=0.3s --udp=0 --downlink=1 --useRts=0 --nStations=5 --dlAckType=AGGR-MU-BAR --enableUlOfdma=1 --enableBsrp=0 --mcs=6 --muSchedAccessReqInterval=50ms --minExpectedThroughput=31 --maxExpectedThroughput=290",
        "True",
        "True",
    ),
    (
        "wifi-he-network --simulationTime=0.3s --udp=1 --downlink=0 --useRts=1 --nStations=5 --dlAckType=AGGR-MU-BAR --enableUlOfdma=1 --enableBsrp=1 --mcs=5 --muSchedAccessReqInterval=50ms --minExpectedThroughput=46 --maxExpectedThroughput=327",
        "True",
        "True",
    ),
    (
        "wifi-eht-network --simulationTime=0.1s --frequency=5 --useRts=0 --minExpectedThroughput=6 --maxExpectedThroughput=760",
        "True",
        "True",
    ),
    (
        "wifi-eht-network --simulationTime=0.1s --frequency=5 --useRts=0 --use80Plus80=1 --minExpectedThroughput=6 --maxExpectedThroughput=760",
        "True",
        "True",
    ),
    (
        "wifi-eht-network --simulationTime=0.1s --frequency=5 --useRts=0 --mpduBufferSize=1024 --frequency2=6 --minExpectedThroughput=7 --maxExpectedThroughput=1444",
        "True",
        "True",
    ),
    (
        "wifi-eht-network --simulationTime=0.1s --frequency=6 --useRts=1 --minExpectedThroughput=5 --maxExpectedThroughput=800",
        "True",
        "True",
    ),
    (
        "wifi-eht-network --simulationTime=0.1s --frequency=2.4 --useRts=0 --mpduBufferSize=512 --frequency2=5 --minExpectedThroughput=7 --maxExpectedThroughput=512",
        "True",
        "True",
    ),
    (
        "wifi-eht-network --simulationTime=0.1s --frequency=2.4 --useRts=1 --minExpectedThroughput=5 --maxExpectedThroughput=240",
        "True",
        "True",
    ),
    (
        "wifi-eht-network --simulationTime=0.23s --udp=0 --downlink=1 --useRts=0 --nStations=4 --dlAckType=ACK-SU-FORMAT --enableUlOfdma=1 --enableBsrp=0 --mcs=6 --frequency2=6 --minExpectedThroughput=35 --maxExpectedThroughput=404",
        "True",
        "True",
    ),
    (
        "wifi-eht-network --simulationTime=0.25s --frequency=2.4 --udp=0 --downlink=1 --useRts=0 --nStations=5 --dlAckType=MU-BAR --enableUlOfdma=1 --enableBsrp=1 --mcs=5 --frequency2=5 --mpduBufferSize=1024 --minExpectedThroughput=50 --maxExpectedThroughput=120",
        "True",
        "True",
    ),
    (
        "wifi-eht-network --simulationTime=0.3s --udp=0 --downlink=1 --useRts=1 --nStations=5 --dlAckType=AGGR-MU-BAR --enableUlOfdma=1 --enableBsrp=0 --mcs=6 --muSchedAccessReqInterval=50ms --frequency2=2.4 --minExpectedThroughput=50 --maxExpectedThroughput=140",
        "True",
        "True",
    ),
    (
        "wifi-eht-network --simulationTime=0.25s --udp=0 --downlink=0 --useRts=0 --nStations=4 --dlAckType=AGGR-MU-BAR --enableUlOfdma=1 --enableBsrp=1 --mpduBufferSize=1024 --mcs=8 --muSchedAccessReqInterval=45ms --frequency2=6 --minExpectedThroughput=50 --maxExpectedThroughput=550 --RngRun=3",
        "True",
        "True",
    ),
    (
        "wifi-eht-network --simulationTime=0.2s --frequency=2.4 --frequency2=5 --guardInterval=1600 --udp=0 --downlink=1 --useRts=0 --mpduBufferSize=512 --emlsrLinks=0,1 --emlsrPaddingDelay=32 --emlsrTransitionDelay=32 --channelSwitchDelay=32us --emlsrAuxSwitch=True --emlsrAuxTxCapable=True --minExpectedThroughput=5 --maxExpectedThroughput=200",
        "True",
        "True",
    ),
    (
        "wifi-eht-network --simulationTime=0.2s --frequency=2.4 --frequency2=5 --guardInterval=1600 --udp=0 --downlink=1 --useRts=1 --mpduBufferSize=512 --emlsrLinks=0,1 --emlsrPaddingDelay=64 --emlsrTransitionDelay=64 --channelSwitchDelay=64us --emlsrMgrTypeId=ns3::AdvancedEmlsrManager --emlsrAuxSwitch=False --emlsrAuxTxCapable=True --minExpectedThroughput=5 --maxExpectedThroughput=190",
        "True",
        "True",
    ),
    (
        "wifi-eht-network --simulationTime=0.2s --frequency=2.4 --frequency2=5 --guardInterval=1600 --udp=0 --downlink=0 --useRts=0 --mpduBufferSize=512 --emlsrLinks=0,1 --emlsrPaddingDelay=0 --emlsrTransitionDelay=0 --channelSwitchDelay=1ns --emlsrMgrTypeId=ns3::AdvancedEmlsrManager --emlsrAuxSwitch=False --emlsrAuxTxCapable=False --minExpectedThroughput=5 --maxExpectedThroughput=40 --RngRun=9",
        "True",
        "True",
    ),
    (
        "wifi-eht-network --simulationTime=0.3s --frequency=2.4 --frequency2=5 --frequency3=6 --guardInterval=1600 --udp=0 --downlink=1 --useRts=0 --mpduBufferSize=512 --emlsrLinks=0,1,2 --emlsrPaddingDelay=32 --emlsrTransitionDelay=32 --channelSwitchDelay=32us --emlsrAuxSwitch=True --emlsrAuxTxCapable=True --nStations=4 --dlAckType=AGGR-MU-BAR --enableUlOfdma=1 --enableBsrp=0 --mcs=0,3,5,9,10 --minExpectedThroughput=8 --maxExpectedThroughput=300",
        "True",
        "True",
    ),
    (
        "wifi-eht-network --simulationTime=0.3s --frequency=2.4 --frequency2=5 --frequency3=6 --guardInterval=1600 --udp=0 --downlink=0 --useRts=1 --mpduBufferSize=512 --emlsrLinks=0,1,2 --emlsrPaddingDelay=64 --emlsrTransitionDelay=64 --channelSwitchDelay=64us --emlsrAuxSwitch=False --emlsrAuxTxCapable=True --nStations=4 --dlAckType=MU-BAR --enableUlOfdma=1 --enableBsrp=1 --mcs=1,4,8,11,13 --minExpectedThroughput=10 --maxExpectedThroughput=260",
        "True",
        "True",
    ),
    (
        "wifi-eht-network --simulationTime=0.3s --frequency=2.4 --frequency2=5 --frequency3=6 --guardInterval=1600 --udp=0 --downlink=0 --useRts=1 --mpduBufferSize=512 --emlsrLinks=0,1,2 --emlsrPaddingDelay=0 --emlsrTransitionDelay=0 --channelSwitchDelay=1ns --emlsrMgrTypeId=ns3::AdvancedEmlsrManager --emlsrAuxSwitch=False --emlsrAuxTxCapable=False --nStations=4 --dlAckType=ACK-SU-FORMAT --enableUlOfdma=1 --enableBsrp=1 --mcs=1,5,8,11 --minExpectedThroughput=8 --maxExpectedThroughput=288",
        "True",
        "True",
    ),
    (
        "wifi-simple-ht-hidden-stations --simulationTime=1s --enableRts=0 --nMpdus=32 --minExpectedThroughput=59 --maxExpectedThroughput=60",
        "True",
        "True",
    ),
    (
        "wifi-simple-ht-hidden-stations --simulationTime=1s --enableRts=1 --nMpdus=32 --minExpectedThroughput=57 --maxExpectedThroughput=58",
        "True",
        "True",
    ),
    ("wifi-mixed-network --simulationTime=1s", "True", "True"),
    ("wifi-aggregation --simulationTime=1s --verifyResults=1", "True", "True"),
    ("wifi-txop-aggregation --simulationTime=1s --verifyResults=1", "True", "True"),
    ("wifi-80211e-txop --simulationTime=1s --verifyResults=1", "True", "True"),
    (
        "wifi-multi-tos --simulationTime=1s --nWifi=16 --useRts=1 --useShortGuardInterval=1",
        "True",
        "True",
    ),
    ("wifi-tcp", "True", "True"),
    ("wifi-hidden-terminal --wifiManager=Arf", "True", "True"),
    ("wifi-hidden-terminal --wifiManager=Aarf", "True", "True"),
    ("wifi-hidden-terminal --wifiManager=Aarfcd", "True", "True"),
    ("wifi-hidden-terminal --wifiManager=Onoe", "True", "True"),
    ("wifi-hidden-terminal --wifiManager=Amrr", "True", "True"),
    ("wifi-hidden-terminal --wifiManager=Minstrel", "True", "True"),
    ("wifi-hidden-terminal --wifiManager=Cara", "True", "True"),
    ("wifi-hidden-terminal --wifiManager=Rraa", "True", "True"),
    ("wifi-hidden-terminal --wifiManager=Rrpaa", "True", "True"),
    (
        "wifi-spectrum-per-example --distance=52 --index=3 --wifiType=ns3::SpectrumWifiPhy --simulationTime=1s",
        "True",
        "True",
    ),
    (
        "wifi-spectrum-per-example --distance=24 --index=31 --wifiType=ns3::YansWifiPhy --simulationTime=1s",
        "True",
        "False",
    ),
    (
        "wifi-spectrum-per-interference --distance=24 --index=31 --simulationTime=1s --waveformPower=0.1",
        "True",
        "True",
    ),
    ("wifi-spectrum-saturation-example --simulationTime=1s --index=63", "True", "True"),
    (
        "wifi-backward-compatibility --apVersion=80211a --staVersion=80211n_5GHZ --simulationTime=1s",
        "True",
        "True",
    ),
    (
        "wifi-backward-compatibility --apVersion=80211a --staVersion=80211n_5GHZ --apRaa=Ideal --staRaa=Ideal --simulationTime=1s",
        "True",
        "False",
    ),
    (
        "wifi-backward-compatibility --apVersion=80211a --staVersion=80211ac --simulationTime=1s",
        "True",
        "False",
    ),
    (
        "wifi-backward-compatibility --apVersion=80211a --staVersion=80211ac --apRaa=Ideal --staRaa=Ideal --simulationTime=1s",
        "True",
        "False",
    ),
    (
        "wifi-multicast --minExpectedPackets=10",
        "True",
        "True",
    ),
    (
        "wifi-multicast --gcrRetransmissionPolicy=GcrUr --minExpectedPackets=10",
        "True",
        "True",
    ),
    (
        "wifi-multicast --gcrRetransmissionPolicy=GcrUr --multicastFrameErrorRate=0.2 --minExpectedPackets=10",
        "True",
        "True",
    ),
    (
        "wifi-multicast --gcrRetransmissionPolicy=GcrUr --maxAmpduLength=65535 --maxPackets=0 --nStations=4 --dataRate=50Mbps --gcrProtection=Rts-Cts --rtsThreshold=0 --simulationTime=1 --minExpectedThroughput=35 --maxExpectedThroughput=40",
        "True",
        "True",
    ),
    (
        "wifi-multicast --gcrRetransmissionPolicy=GcrUr --maxAmpduLength=65535 --maxPackets=0 --nStations=4 --dataRate=50Mbps --gcrProtection=Cts-To-Self --simulationTime=1 --minExpectedThroughput=40 --maxExpectedThroughput=45",
        "True",
        "True",
    ),
    (
        "wifi-multicast --gcrRetransmissionPolicy=GcrBlockAck --minExpectedPackets=10",
        "True",
        "True",
    ),
    (
        "wifi-multicast --gcrRetransmissionPolicy=GcrBlockAck --multicastFrameErrorRate=0.2 --minExpectedPackets=10",
        "True",
        "True",
    ),
    (
        "wifi-multicast --gcrRetransmissionPolicy=GcrBlockAck --maxAmpduLength=65535 --maxPackets=0 --nStations=4 --dataRate=100Mbps --gcrProtection=Rts-Cts --rtsThreshold=0 --simulationTime=1s --minExpectedThroughput=100 --maxExpectedThroughput=100",
        "True",
        "True",
    ),
    (
        "wifi-multicast --gcrRetransmissionPolicy=GcrBlockAck --maxAmpduLength=65535 --maxPackets=0 --nStations=4 --dataRate=100Mbps --gcrProtection=Cts-To-Self --simulationTime=1s --minExpectedThroughput=100 --maxExpectedThroughput=100",
        "True",
        "True",
    ),
    ("new-wifi-multiple-devices", "True", "False"),
]

# A list of Python examples to run in order to ensure that they remain
# runnable over time.  Each tuple in the list contains
#
#     (example_name, do_run).
#
# See test.py for more information.
python_examples = [
    ("wifi-ap.py", "True"),
    ("mixed-wired-wireless.py", "True"),
]
