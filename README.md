# The Network Simulator, Version 3

[![codecov](https://codecov.io/gh/nsnam/ns-3-dev-git/branch/master/graph/badge.svg)](https://codecov.io/gh/nsnam/ns-3-dev-git/branch/master/)
[![Gitlab CI](https://gitlab.com/nsnam/ns-3-dev/badges/master/pipeline.svg)](https://gitlab.com/nsnam/ns-3-dev/-/pipelines)
[![Github CI](https://github.com/nsnam/ns-3-dev-git/actions/workflows/per_commit.yml/badge.svg)](https://github.com/nsnam/ns-3-dev-git/actions)

[![Latest Release](https://gitlab.com/nsnam/ns-3-dev/-/badges/release.svg)](https://gitlab.com/nsnam/ns-3-dev/-/releases)

## License

This software is licensed under the terms of the GNU General Public License v2.0 only (GPL-2.0-only).
See the LICENSE file for more details.

## Table of Contents

* [Overview](#overview-an-open-source-project)
* [Software overview](#software-overview)
* [ns-3 Architecture for Wireless Simulation](#ns-3-architecture-for-wireless-simulation)
* [Getting ns-3](#getting-ns-3)
* [Building ns-3](#building-ns-3)
* [Testing ns-3](#testing-ns-3)
* [Running ns-3](#running-ns-3)
* [ns-3 Documentation](#ns-3-documentation)
* [Working with the Development Version of ns-3](#working-with-the-development-version-of-ns-3)
* [Contributing to ns-3](#contributing-to-ns-3)
* [Reporting Issues](#reporting-issues)
* [Asking Questions](#asking-questions)
* [ns-3 App Store](#ns-3-app-store)

> **NOTE**: Much more substantial information about ns-3 can be found at
<https://www.nsnam.org>

## Overview: An Open Source Project

ns-3 is a free open source project aiming to build a discrete-event
network simulator targeted for simulation research and education.
This is a collaborative project; we hope that
the missing pieces of the models we have not yet implemented
will be contributed by the community in an open collaboration
process. If you would like to contribute to ns-3, please check
the [Contributing to ns-3](#contributing-to-ns-3) section below.

This README excerpts some details from a more extensive
tutorial that is maintained at:
<https://www.nsnam.org/documentation/latest/>

## Software overview

From a software perspective, ns-3 consists of a number of C++
libraries organized around different topics and technologies.
Programs that actually run simulations can be written in
either C++ or Python; the use of Python is enabled by
[runtime C++/Python bindings](https://cppyy.readthedocs.io/en/latest/).  Simulation programs will
typically link or import the ns `core` library and any additional
libraries that they need.  ns-3 requires a modern C++ compiler
installation (g++ or clang++) and the [CMake](https://cmake.org) build system.
Most ns-3 programs are single-threaded; there is some limited
support for parallelization using the [MPI](https://www.nsnam.org/docs/models/html/distributed.html) framework.
ns-3 can also run in a real-time emulation mode by binding to an
Ethernet device on the host machine and generating and consuming
packets on an actual network.  The ns-3 APIs are documented
using [Doxygen](https://www.doxygen.nl).

The code for the framework and the default models provided
by ns-3 is built as a set of libraries. The libraries maintained
by the open source project can be found in the `src` directory.
Users may extend ns-3 by adding libraries to the build;
third-party libraries can be found on the [ns-3 App Store](https://www.nsnam.org)
or elsewhere in public Git repositories, and are usually added to the `contrib` directory.

## ns-3 Architecture for Wireless Simulation

The ns-3 framework is organised as a collection of reusable building blocks that can be
composed to represent wireless network scenarios with differing levels of fidelity.  The
following sections summarise the main pieces you are likely to include when presenting the
architecture in a slide deck.

### High-level layering

ns-3 provides several foundational libraries that are used across all simulation models:

* **Simulation core (`src/core`, `src/simulator`):** Implements the discrete-event engine,
  real-time scheduler variants, event queues, logging, and command-line handling.
* **Network stack (`src/network`, `src/internet`):** Supplies common packet primitives,
  headers, sockets, routing, and internet protocols for IPv4/IPv6.
* **Mobility and propagation (`src/mobility`, `src/propagation`, `src/aodv`, etc.):** Models
  node movement, radio propagation loss, and fading effects that are re-used by multiple
  wireless technologies.
* **Wireless access technologies (`src/wifi`, `src/lte`, `src/nr`, `src/mesh`, `src/wimax`,
  ...):** Each module encapsulates PHY/MAC implementations, helper classes, and
  configuration defaults for a particular radio technology.

These libraries are compiled into modular shared libraries.  Simulation scripts—typically
placed in the `scratch/` directory for experimentation or `examples/` for curated samples—link
against whichever libraries they require.

### Key simulation concepts

* **Nodes (`ns3::Node`):** Abstract containers for protocol stacks and applications.  In
  wireless simulations, nodes usually hold one or more `NetDevice` instances (e.g., Wi-Fi
  radios) and mobility models.
* **NetDevices and Channels:** `NetDevice` objects (such as `WifiNetDevice`, `LteUeNetDevice`)
  model the MAC/PHY layers.  Devices communicate through `Channel` objects that represent
  shared mediums (e.g., `YansWifiChannel`, `LteSpectrumChannel`).
* **Helpers:** High-level configurators (e.g., `WifiHelper`, `LteHelper`) that encapsulate the
  boilerplate required to instantiate devices, channels, and PHY/MAC settings.
* **Attributes and Config Store:** ns-3 exposes almost every configuration parameter via an
  attribute system.  Attributes can be set programmatically (`Config::Set`), by command-line
  arguments, or via configuration files using the `ConfigStore` module.
* **Tracing:** Trace sources emit events (packet receptions, state changes) that can be
  connected to user-defined callbacks, pcap/file writers, or the statistics framework.

### Execution flow

1. **Scenario setup:** Create nodes, assign mobility models, and install protocol stacks and
   applications.
2. **Device installation:** Use helpers to install wireless net devices (e.g., Wi-Fi, LTE) and
   configure channel/propagation models.
3. **Attribute configuration:** Adjust PHY/MAC parameters (channel width, modulation, power
   levels) via attributes or helper setters.
4. **Tracing and logging:** Enable ASCII/pcap tracing or connect callbacks to trace sources.
5. **Run the simulator:** Call `Simulator::Run()` to execute events in timestamp order, then
   `Simulator::Destroy()` to clean up global state.

### Wireless module structure

Wireless modules share common design patterns:

* **PHY layer:** Models radio spectrum behaviour (transmitters, receivers, interference).  The
  Wi-Fi module, for example, provides `YansWifiPhy`, `SpectrumWifiPhy`, and a suite of
  propagation loss and error rate models.
* **MAC layer:** Implements medium access control logic (DCA/EDCA for Wi-Fi, MAC Scheduler for
  LTE).  MAC instances interact with PHY counterparts via service primitives.
* **Channel and propagation models:** Propagation loss models (Friis, LogDistance, Nakagami)
  plug into channels to calculate attenuation, while error rate and interference models decide
  packet reception outcomes.
* **Control plane managers:** Some technologies include higher-level managers (e.g., LTE’s
  `EpcHelper`, `LteRrcProtocolIdeal`) for mobility management and core network emulation.
* **Tracing hooks:** Each layer exports trace sources for PHY state, MAC queue events, and
  packet counters, enabling fine-grained instrumentation in wireless studies.

### Directory cheatsheet for presentations

| Directory | Purpose |
|-----------|---------|
| `src/core`, `src/simulator` | Discrete-event kernel and scheduling primitives. |
| `src/network` | Common packet structures, trace system, and address abstractions. |
| `src/mobility` | Mobility models and position allocators for moving nodes. |
| `src/propagation` | Propagation delay and loss models used by wireless channels. |
| `src/wifi` | IEEE 802.11 PHY/MAC models, helpers, and examples. |
| `src/lte`, `src/nr` | 3GPP LTE/NR radio stack, EPC integration, and schedulers. |
| `examples/wireless` | Curated example scripts for Wi-Fi, LTE, mmWave, and more. |
| `scratch/` | Workspace for custom experiments and prototypes. |

### Minimal Wi-Fi example

The following C++ snippet illustrates the typical structure of a Wi-Fi simulation script.  It
creates two nodes, installs Wi-Fi devices using the Yans helper stack, assigns IP addresses,
and schedules an application to generate traffic.

```cpp
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/wifi-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main (int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse (argc, argv);

  NodeContainer nodes;
  nodes.Create (2);

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (nodes);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy = YansWifiPhyHelper::Default ();
  phy.SetChannel (channel.Create ());

  WifiHelper wifi;
  wifi.SetStandard (WIFI_STANDARD_80211ac);
  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode", StringValue ("VhtMcs7"));

  WifiMacHelper mac;
  mac.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (Ssid ("ns3-wifi")));

  NetDeviceContainer devices = wifi.Install (phy, mac, nodes);

  InternetStackHelper stack;
  stack.Install (nodes);

  Ipv4AddressHelper address;
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer interfaces = address.Assign (devices);

  UdpEchoServerHelper echoServer (9);
  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (5));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Simulator::Stop (Seconds (10.0));
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
```

To compile and run a custom script stored in `scratch/wifi-quickstart.cc`, execute:

```shell
./ns3 configure --enable-examples
./ns3 build
./ns3 run wifi-quickstart
```

This example highlights the interplay between nodes, helpers, mobility models, and
applications.  You can extend it by swapping propagation models, enabling tracing (e.g.,
`phy.EnablePcapAll("wifi-quickstart")`), or integrating advanced modules such as LTE and NR to
illustrate cross-technology studies in your presentation.

## Getting ns-3

ns-3 can be obtained by either downloading a released source
archive, or by cloning the project's
[Git repository](https://gitlab.com/nsnam/ns-3-dev.git).

Starting with ns-3 release version 3.45, there are two versions
of source archives that are published with each release:

1. ns-3.##.tar.bz2
1. ns-allinone-3.##.tar.bz2

The first archive is simply a compressed archive of the same code
that one can obtain by checking out the release tagged code from
the ns-3-dev Git repository.  The second archive consists of
ns-3 plus additional contributed modules that are maintained outside
of the main ns-3 open source project but that have been reviewed
by maintainers and lightly tested for compatibility with the
release.  The contributed modules included in the `allinone` release
will change over time as new third-party libraries emerge while others
may lose compatibility with the ns-3 mainline (e.g., if they become
unmaintained).

## Building ns-3

As mentioned above, ns-3 uses the CMake build system, but
the project maintains a customized wrapper around CMake
called the `ns3` tool.  This tool provides a
[Waf-like](https://waf.io) API
to the underlying CMake build manager.
To build the set of default libraries and the example
programs included in this package, you need to use the
`ns3` tool. This tool provides a Waf-like API to the
underlying CMake build manager.
Detailed information on how to use `ns3` is included in the
[quick start guide](doc/installation/source/quick-start.rst).

Before building ns-3, you must configure it.
This step allows the configuration of the build options,
such as whether to enable the examples, tests and more.

To configure ns-3 with examples and tests enabled,
run the following command on the ns-3 main directory:

```shell
./ns3 configure --enable-examples --enable-tests
```

Then, build ns-3 by running the following command:

```shell
./ns3 build
```

By default, the build artifacts will be stored in the `build/` directory.

### Supported Platforms

The current codebase is expected to build and run on the
set of platforms listed in the [release notes](RELEASE_NOTES.md)
file.

Other platforms may or may not work: we welcome patches to
improve the portability of the code to these other platforms.

## Testing ns-3

ns-3 contains test suites to validate the models and detect regressions.
To run the test suite, run the following command on the ns-3 main directory:

```shell
./test.py
```

More information about ns-3 tests is available in the
[test framework](doc/manual/source/test-framework.rst) section of the manual.

## Running ns-3

On recent Linux systems, once you have built ns-3 (with examples
enabled), it should be easy to run the sample programs with the
following command, such as:

```shell
./ns3 run simple-global-routing
```

That program should generate a `simple-global-routing.tr` text
trace file and a set of `simple-global-routing-xx-xx.pcap` binary
PCAP trace files, which can be read by `tcpdump -n -tt -r filename.pcap`.
The program source can be found in the `examples/routing` directory.

## Running ns-3 from Python

If you do not plan to modify ns-3 upstream modules, you can get
a pre-built version of the ns-3 python bindings. It is recommended
to create a python virtual environment to isolate different application
packages from system-wide packages (installable via the OS package managers).

```shell
python3 -m venv ns3env
source ./ns3env/bin/activate
pip install ns3
```

If you do not have `pip`, check their documents
on [how to install it](https://pip.pypa.io/en/stable/installation/).

After installing the `ns3` package, you can then create your simulation python script.
Below is a trivial demo script to get you started.

```python
from ns import ns

ns.LogComponentEnable("Simulator", ns.LOG_LEVEL_ALL)

ns.Simulator.Stop(ns.Seconds(10))
ns.Simulator.Run()
ns.Simulator.Destroy()
```

The simulation will take a while to start, while the bindings are loaded.
The script above will print the logging messages for the called commands.

Use `help(ns)` to check the prototypes for all functions defined in the
ns3 namespace. To get more useful results, query specific classes of
interest and their functions e.g., `help(ns.Simulator)`.

Smart pointers `Ptr<>` can be differentiated from objects by checking if
`__deref__` is listed in `dir(variable)`. To dereference the pointer,
use `variable.__deref__()`.

Most ns-3 simulations are written in C++ and the documentation is
oriented towards C++ users. The ns-3 tutorial programs (`first.cc`,
`second.cc`, etc.) have Python equivalents, if you are looking for
some initial guidance on how to use the Python API. The Python
API may not be as full-featured as the C++ API, and an API guide
for what C++ APIs are supported or not from Python do not currently exist.
The project is looking for additional Python maintainers to improve
the support for future Python users.

## ns-3 Documentation

Once you have verified that your build of ns-3 works by running
the `simple-global-routing` example as outlined in the [running ns-3](#running-ns-3)
section, it is quite likely that you will want to get started on reading
some ns-3 documentation.

All of that documentation should always be available from
the ns-3 website: <https://www.nsnam.org/documentation/>.

This documentation includes:

* a tutorial
* a reference manual
* models in the ns-3 model library
* a wiki for user-contributed tips: <https://www.nsnam.org/wiki/>
* API documentation generated using doxygen: this is
  a reference manual, most likely not very well suited
  as introductory text:
  <https://www.nsnam.org/doxygen/index.html>

## Working with the Development Version of ns-3

If you want to download and use the development version of ns-3, you
need to use the tool `git`. A quick and dirty cheat sheet is included
in the manual, but reading through the Git
tutorials found in the Internet is usually a good idea if you are not
familiar with it.

If you have successfully installed Git, you can get
a copy of the development version with the following command:

```shell
git clone https://gitlab.com/nsnam/ns-3-dev.git
```

However, we recommend to follow the GitLab guidelines for starters,
that includes creating a GitLab account, forking the ns-3-dev project
under the new account's name, and then cloning the forked repository.
You can find more information in the [manual](https://www.nsnam.org/docs/manual/html/working-with-git.html).

## Contributing to ns-3

The process of contributing to the ns-3 project varies with
the people involved, the amount of time they can invest
and the type of model they want to work on, but the current
process that the project tries to follow is described in the
[contributing code](https://www.nsnam.org/developers/contributing-code/)
website and in the [CONTRIBUTING.md](CONTRIBUTING.md) file.

## Reporting Issues

If you would like to report an issue, you can open a new issue in the
[GitLab issue tracker](https://gitlab.com/nsnam/ns-3-dev/-/issues).
Before creating a new issue, please check if the problem that you are facing
was already reported and contribute to the discussion, if necessary.

## Asking Questions

ns-3 has an official [ns-3-users message board](https://groups.google.com/g/ns-3-users)
where the community asks questions and share helpful advice.
Additionally, ns-3 has the [ns-3 Zulip chat](https://ns-3.zulipchat.com/), used to discuss
development issues and questions among maintainers and the community.

Please use the above resources to ask questions about ns-3, rather than creating issues.

## ns-3 App Store

The official [ns-3 App Store](https://apps.nsnam.org/) is a centralized directory
listing third-party modules for ns-3 available on the Internet.

More information on how to submit an ns-3 module to the ns-3 App Store is available
in the [ns-3 App Store documentation](https://www.nsnam.org/docs/contributing/html/external.html).
