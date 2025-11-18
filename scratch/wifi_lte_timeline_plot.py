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

STATE_VALUES: Dict[str, float] = {
    "PhyTxBegin": 1.0,
    "PhyRxBegin": 1.0,
    "PhyRxDrop": -1.0,
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


def build_step_series(events: List[Dict[str, float]]) -> Dict[int, Dict[str, List[float]]]:
    grouped: Dict[int, List[Dict[str, float]]] = {}
    for entry in events:
        if entry["event"] not in STATE_VALUES:
            continue
        grouped.setdefault(entry["flow_id"], []).append(entry)

    if not grouped:
        raise ValueError("No Wi-Fi PHY events found in the supplied log")

    step_data: Dict[int, Dict[str, List[float]]] = {}
    for flow_id, flow_events in grouped.items():
        flow_events.sort(key=lambda e: e["time"])
        times: List[float] = []
        values: List[float] = []

        current_state = 0.0
        start_time = flow_events[0]["time"]
        times.append(start_time)
        values.append(current_state)

        for entry in flow_events:
            time_s = entry["time"]
            times.append(time_s)
            values.append(current_state)

            current_state = STATE_VALUES[entry["event"]]
            times.append(time_s)
            values.append(current_state)

        total_span = max(flow_events[-1]["time"] - start_time, 1e-5)
        tail = total_span * 0.05
        times.append(flow_events[-1]["time"] + tail)
        values.append(current_state)

        step_data[flow_id] = {"times": times, "values": values}

    return step_data


def plot_timeline(events: Iterable[Dict[str, float]], output: pathlib.Path, show: bool = False) -> None:
    events = list(events)
    step_data = build_step_series(events)
    flows = sorted(step_data)

    fig, ax = plt.subplots(figsize=(10.0, 4.0))
    for flow_id in flows:
        series = step_data[flow_id]
        ax.step(
            series["times"],
            series["values"],
            where="post",
            label=f"Flow {flow_id}",
            linewidth=1.5,
        )

        drop_times = [e["time"] for e in events if e["flow_id"] == flow_id and e["event"] == "PhyRxDrop"]
        if drop_times:
            ax.scatter(drop_times, [-1.0] * len(drop_times), color=EVENT_COLORS["PhyRxDrop"], marker="x", label=None)

    ax.set_xlabel("Time (s)")
    ax.set_ylabel("State")
    ax.set_ylim(-1.5, 1.5)
    ax.set_yticks([-1.0, 0.0, 1.0])
    ax.set_yticklabels(["Drop", "Idle", "TX"])
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

