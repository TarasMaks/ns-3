#!/usr/bin/env python3
"""Generate a timeline plot for the wifi-lte-traffic example."""
from __future__ import annotations

import argparse
import csv
import pathlib
from typing import Dict, Iterable, List

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402  (import after backend selection)

EVENT_COLORS: Dict[str, str] = {
    "PhyTxBegin": "tab:blue",
    "PhyRxBegin": "tab:green",
    "PhyRxDrop": "tab:red",
}

EVENT_MARKERS: Dict[str, str] = {
    "PhyTxBegin": "o",
    "PhyRxBegin": "s",
    "PhyRxDrop": "x",
}

EVENT_OFFSETS: Dict[str, float] = {
    "PhyTxBegin": -0.2,
    "PhyRxBegin": 0.0,
    "PhyRxDrop": 0.2,
}


def load_events(path: pathlib.Path) -> List[Dict[str, float]]:
    events: List[Dict[str, float]] = []
    with path.open(newline="") as fh:
        reader = csv.DictReader(fh)
        for row in reader:
            try:
                time_s = float(row.get("time_s", ""))
                flow_id = int(float(row.get("flow_id", "0")))
            except ValueError:
                continue

            event = row.get("event", "")
            if not event:
                continue

            node_id = int(float(row.get("node_id", "-1")))
            device_id = int(float(row.get("device_id", "-1")))
            packet_size = int(float(row.get("packet_size_bytes", "0")))
            detail = row.get("detail", "")

            events.append(
                {
                    "time": time_s,
                    "event": event,
                    "flow_id": flow_id,
                    "node_id": node_id,
                    "device_id": device_id,
                    "packet_size": packet_size,
                    "detail": detail,
                }
            )
    return events


def plot_timeline(events: Iterable[Dict[str, float]], output: pathlib.Path, show: bool = False) -> None:
    filtered = [e for e in events if e["event"] in EVENT_COLORS]
    if not filtered:
        raise ValueError("No Wi-Fi PHY events found in the supplied log")

    flows = sorted({e["flow_id"] for e in filtered})
    if not flows:
        raise ValueError("No flow identifiers were found; ensure flow tagging is enabled")

    y_positions = {flow_id: idx for idx, flow_id in enumerate(flows)}

    fig_height = max(2.5, 1.5 * len(flows))
    fig, ax = plt.subplots(figsize=(10.0, fig_height))

    legend_added = set()
    for entry in filtered:
        event = entry["event"]
        flow_id = entry["flow_id"]
        time_s = entry["time"]
        y_base = y_positions[flow_id]
        y_value = y_base + EVENT_OFFSETS.get(event, 0.0)
        label = event if event not in legend_added else None
        ax.scatter(
            [time_s],
            [y_value],
            color=EVENT_COLORS[event],
            marker=EVENT_MARKERS[event],
            label=label,
            linewidths=1.5,
        )

        if event == "PhyRxDrop" and entry.get("detail"):
            ax.annotate(
                entry["detail"],
                (time_s, y_value),
                textcoords="offset points",
                xytext=(0, 6),
                ha="center",
                fontsize=8,
                rotation=45,
            )

        legend_added.add(event)

    ax.set_xlabel("Time (s)")
    ax.set_ylabel("Traffic flow")
    ax.set_yticks([y_positions[f] for f in flows])
    ax.set_yticklabels([f"Flow {f}" for f in flows])
    ax.grid(True, axis="x", linestyle="--", alpha=0.4)
    ax.legend(loc="upper right")
    fig.tight_layout()

    if show:
        plt.show()
    else:
        fig.savefig(output, dpi=200)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--log",
        type=pathlib.Path,
        default=pathlib.Path("wifi-lte-traffic-log.csv"),
        help="Path to the CSV log emitted by wifi-lte-traffic example",
    )
    parser.add_argument(
        "--output",
        type=pathlib.Path,
        default=pathlib.Path("wifi_lte_timeline.png"),
        help="Destination image file for the timeline plot",
    )
    parser.add_argument(
        "--show",
        action="store_true",
        help="Display the plot interactively instead of saving it",
    )
    args = parser.parse_args()

    if not args.log.exists():
        raise SystemExit(f"Log file '{args.log}' does not exist")

    events = load_events(args.log)
    plot_timeline(events, args.output, show=args.show)


if __name__ == "__main__":
    main()

