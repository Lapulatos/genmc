#!/usr/bin/env python3
"""Summarize annotated-read SC-RVF admission and activation over all actual tasks."""

from __future__ import annotations

import argparse
import json
import re
import zipfile
from collections import Counter
from pathlib import Path


COUNTERS = (
    "rvf-loads-attempted",
    "rvf-loads-reduced",
    "rvf-annotated-groups-rejected",
    "rvf-owned-read-revisits-suppressed",
    "rvf-fail-open",
)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("archive", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    gates: Counter[str] = Counter()
    totals: Counter[str] = Counter()
    active = Counter()
    logs = 0
    with zipfile.ZipFile(args.archive) as archive:
        for member in archive.namelist():
            if member.endswith("/"):
                continue
            logs += 1
            text = archive.read(member).decode("utf-8", "replace")
            match = re.search(r"^SC RVF program gate: (.+)$", text, re.MULTILINE)
            gates[match.group(1) if match else "not-recorded"] += 1
            values = {key: int(value) for key, value in re.findall(
                r"([a-z][a-z0-9-]*)=([0-9]+)", text
            )}
            for counter in COUNTERS:
                value = values.get(counter, 0)
                totals[counter] += value
                if value:
                    active[counter] += 1

    result = {
        "logs": logs,
        "gate_counts": dict(gates.most_common()),
        "counter_totals": {counter: totals[counter] for counter in COUNTERS},
        "tasks_with_nonzero_counter": {counter: active[counter] for counter in COUNTERS},
        "retain_gate": totals["rvf-loads-reduced"] > 0 and totals["rvf-fail-open"] == 0,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
