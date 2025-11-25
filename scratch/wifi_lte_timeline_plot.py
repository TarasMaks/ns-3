#!/usr/bin/env python3
"""Generate a timeline plot for the wifi-lte-traffic example."""
from __future__ import annotations

import argparse
import csv
import pathlib
from typing import Dict, Iterable, List, Optional

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

# Minimum spacing to separate back-to-idle transitions from the next event
TRANSIENT_EPS = 1e-6


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


def build_step_series(
    events: List[Dict[str, float]],
    pulse_width: float,
) -> Dict[int, Dict[str, List[float]]]:
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

        for idx, entry in enumerate(flow_events):
            time_s = entry["time"]
            next_time: Optional[float] = None
            if idx + 1 < len(flow_events):
                next_time = flow_events[idx + 1]["time"]

            # Hold the previous state until this event time
            times.append(time_s)
            values.append(current_state)

            event = entry["event"]
            event_state = STATE_VALUES[event]
            current_state = event_state
            times.append(time_s)
            values.append(current_state)

            # Decide how long to keep the pulse high (or negative on drop)
            desired_end: Optional[float] = None
            if pulse_width > 0.0:
                desired_end = time_s + pulse_width
            if next_time is not None:
                candidate = max(time_s, next_time - TRANSIENT_EPS)
                if desired_end is None or candidate < desired_end:
                    desired_end = candidate
            if desired_end is None:
                desired_end = time_s + max(pulse_width, 1e-5)

            # Ensure we do not go backwards in time
            desired_end = max(desired_end, time_s + TRANSIENT_EPS)

            current_state = 0.0
            times.append(desired_end)
            values.append(current_state)

        total_span = max(flow_events[-1]["time"] - start_time, 1e-5)
        tail = total_span * 0.05
        times.append(flow_events[-1]["time"] + tail)
        values.append(current_state)

        step_data[flow_id] = {"times": times, "values": values}

    return step_data


def plot_timeline(
    events: Iterable[Dict[str, float]],
    output: pathlib.Path,
    pulse_width: float,
    show: bool = False,
) -> None:
    events = list(events)
    step_data = build_step_series(events, pulse_width=pulse_width)
    flows = sorted(step_data)

    fig, ax = plt.subplots(figsize=(10.0, 4.0))
    drop_label_shown = False
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
            label = "Drop" if not drop_label_shown else None
            ax.scatter(
                drop_times,
                [-1.0] * len(drop_times),
                color=EVENT_COLORS["PhyRxDrop"],
                marker="x",
                label=label,
            )
            drop_label_shown = True

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
        "--pulse-width",
        type=float,
        default=0.002,
        metavar="SECONDS",
        help="Duration each packet burst stays high in the step plot (0 to extend until the next event)",
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
    plot_timeline(events, args.output, pulse_width=args.pulse_width, show=args.show)


if __name__ == "__main__":
    main()

