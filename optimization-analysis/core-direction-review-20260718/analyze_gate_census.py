#!/usr/bin/env python3
"""Summarize SC-RVF whole-program gate classifications from one BenchExec log archive."""

from __future__ import annotations

import argparse
import csv
import json
import re
import zipfile
from collections import Counter
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("archive", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    rows: list[dict[str, str | int]] = []
    with zipfile.ZipFile(args.archive) as archive:
        for member in sorted(name for name in archive.namelist() if not name.endswith("/")):
            text = archive.read(member).decode("utf-8", "replace")
            gate_match = re.search(r"^SC RVF program gate: (.+)$", text, re.MULTILINE)
            complete_match = re.search(
                r"^Number of complete executions explored: ([0-9]+)$", text, re.MULTILINE
            )
            counters = dict(re.findall(r"([a-z][a-z0-9-]*)=([0-9]+)", text))
            rows.append(
                {
                    "task": Path(member).name.removesuffix(".log"),
                    "gate": gate_match.group(1) if gate_match else "not-recorded",
                    "complete_executions": int(complete_match.group(1)) if complete_match else -1,
                    "rvf_quotient_disabled_loads": int(
                        counters.get("rvf-quotient-disabled-loads", "0")
                    ),
                    "rvf_loads_attempted": int(counters.get("rvf-loads-attempted", "0")),
                }
            )

    args.output.mkdir(parents=True, exist_ok=True)
    with (args.output / "gate-tasks.tsv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]), delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)

    counts = Counter(str(row["gate"]) for row in rows)
    summary = {
        "logs": len(rows),
        "gate_counts": dict(counts.most_common()),
        "enabled_with_quotient_opportunity": sum(
            row["gate"] == "enabled" and row["rvf_quotient_disabled_loads"] > 0
            for row in rows
        ),
        "quotient_disabled_loads_on_enabled_tasks": sum(
            int(row["rvf_quotient_disabled_loads"])
            for row in rows
            if row["gate"] == "enabled"
        ),
    }
    (args.output / "gate-summary.json").write_text(
        json.dumps(summary, indent=2) + "\n", encoding="utf-8"
    )


if __name__ == "__main__":
    main()
