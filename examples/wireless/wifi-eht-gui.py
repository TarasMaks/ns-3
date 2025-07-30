import sys
import threading
import tkinter as tk
from tkinter import ttk
from tkinter.scrolledtext import ScrolledText

import matplotlib
matplotlib.use('TkAgg')
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
from matplotlib.figure import Figure

try:
    from ns import ns
except ModuleNotFoundError:
    ns = None

ap_pos = None
sta_pos = None

class TextLogger:
    def __init__(self, widget):
        self.widget = widget
    def write(self, msg):
        self.widget.insert(tk.END, msg)
        self.widget.see(tk.END)
    def flush(self):
        pass

def on_click(event):
    global ap_pos, sta_pos
    if event.xdata is None or event.ydata is None:
        return
    if ap_pos is None:
        ap_pos = (event.xdata, event.ydata)
        ax.scatter(*ap_pos, c='red', marker='s', label='AP')
    elif sta_pos is None:
        sta_pos = (event.xdata, event.ydata)
        ax.scatter(*sta_pos, c='blue', marker='o', label='STA')
    canvas.draw()


def run_simulation():
    log.insert(tk.END, 'Starting simulation\n')
    if ns is None:
        log.insert(tk.END, 'ns3 Python bindings not available.\n')
        return
    if ap_pos is None or sta_pos is None:
        log.insert(tk.END, 'Place AP and STA before running.\n')
        return
    mcs = mcs_var.get()
    bw = int(bw_var.get())
    sim_time = float(time_var.get())

    nodes = ns.NodeContainer()
    nodes.Create(2)

    wifi = ns.WifiHelper()
    wifi.SetStandard(ns.WIFI_STANDARD_80211be)
    wifi.SetRemoteStationManager('ns3::ConstantRateWifiManager',
                                 'DataMode', ns.StringValue(mcs),
                                 'ControlMode', ns.StringValue(mcs))

    ns.Config.Set('/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/ChannelSettings',
                  ns.StringValue(f"{{0, {bw}, BAND_6GHZ, 0}}"))

    channel = ns.YansWifiChannelHelper.Default()
    phy = ns.YansWifiPhyHelper()
    phy.SetChannel(channel.Create())

    mac = ns.WifiMacHelper()
    ssid = ns.Ssid('ns3-eht')
    mac.SetType('ns3::StaWifiMac', 'Ssid', ns.SsidValue(ssid))
    sta_dev = wifi.Install(phy, mac, ns.NodeContainer(nodes.Get(1)))
    mac.SetType('ns3::ApWifiMac', 'Ssid', ns.SsidValue(ssid))
    ap_dev = wifi.Install(phy, mac, ns.NodeContainer(nodes.Get(0)))

    mobility = ns.MobilityHelper()
    pos = ns.ListPositionAllocator()
    pos.Add(ns.Vector(ap_pos[0], ap_pos[1], 0))
    pos.Add(ns.Vector(sta_pos[0], sta_pos[1], 0))
    mobility.SetPositionAllocator(pos)
    mobility.SetMobilityModel('ns3::ConstantPositionMobilityModel')
    mobility.Install(nodes)

    stack = ns.InternetStackHelper()
    stack.Install(nodes)

    address = ns.Ipv4AddressHelper()
    address.SetBase(ns.Ipv4Address('10.1.1.0'), ns.Ipv4Mask('255.255.255.0'))
    interfaces = address.Assign(ns.NetDeviceContainer(ap_dev, sta_dev))

    server = ns.UdpEchoServerHelper(9)
    serverApps = server.Install(nodes.Get(0))
    serverApps.Start(ns.Seconds(0))
    serverApps.Stop(ns.Seconds(sim_time))

    addr = interfaces.GetAddress(0).ConvertTo()
    client = ns.UdpEchoClientHelper(addr, 9)
    client.SetAttribute('MaxPackets', ns.UintegerValue(1))
    client.SetAttribute('Interval', ns.TimeValue(ns.Seconds(1)))
    client.SetAttribute('PacketSize', ns.UintegerValue(1024))
    clientApps = client.Install(nodes.Get(1))
    clientApps.Start(ns.Seconds(1))
    clientApps.Stop(ns.Seconds(sim_time))

    ns.Simulator.Stop(ns.Seconds(sim_time))
    ns.Simulator.Run()
    ns.Simulator.Destroy()
    log.insert(tk.END, 'Simulation finished\n')


def start_sim():
    thread = threading.Thread(target=run_simulation)
    thread.start()

root = tk.Tk()
root.title('ns-3 EHT WiFi GUI')

fig = Figure(figsize=(5, 4))
ax = fig.add_subplot(111)
ax.set_xlim(0, 10)
ax.set_ylim(0, 10)
ax.set_title('Click to place AP and STA')
canvas = FigureCanvasTkAgg(fig, master=root)
canvas.mpl_connect('button_press_event', on_click)
canvas.draw()
canvas.get_tk_widget().pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

control = tk.Frame(root)
control.pack(side=tk.RIGHT, fill=tk.Y)

mcs_var = tk.StringVar(value='EhtMcs0')
mc_options = [f'EhtMcs{i}' for i in range(0, 14)]
ttk.Label(control, text='EHT MCS').pack()
mc_menu = ttk.OptionMenu(control, mcs_var, mc_options[0], *mc_options)
mc_menu.pack()

bw_var = tk.StringVar(value='20')
ttk.Label(control, text='Bandwidth (MHz)').pack()
bw_menu = ttk.OptionMenu(control, bw_var, '20', '20', '40', '80', '160', '320')
bw_menu.pack()

time_var = tk.DoubleVar(value=10)
ttk.Label(control, text='Simulation Time (s)').pack()
time_slider = tk.Scale(control, from_=1, to=60, orient=tk.HORIZONTAL, variable=time_var)
time_slider.pack()

run_btn = ttk.Button(control, text='Run', command=start_sim)
run_btn.pack(pady=5)

log = ScrolledText(control, height=15, width=40)
log.pack()

sys.stdout = TextLogger(log)
sys.stderr = TextLogger(log)

root.mainloop()
