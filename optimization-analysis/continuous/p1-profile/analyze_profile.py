#!/usr/bin/env python3
"""Aggregate opt-in CAAT statistics from BenchExec log archives."""

from __future__ import annotations

import argparse
import csv
import json
import math
import re
from pathlib import Path
from statistics import median
from zipfile import ZipFile


PREFIX = "CAT incremental statistics: "
PAIR_RE = re.compile(r"([a-z][a-z0-9-]*)=([0-9]+)")


def percentile(values: list[int], fraction: float) -> int:
    if not values:
        return 0
    ordered = sorted(values)
    return ordered[math.ceil(fraction * len(ordered)) - 1]


def read_rows(archives: list[Path]) -> tuple[list[dict[str, int | str]], int]:
    rows: list[dict[str, int | str]] = []
    log_count = 0
    for archive in archives:
        with ZipFile(archive) as zf:
            for member in zf.namelist():
                if not member.endswith(".log"):
                    continue
                log_count += 1
                text = zf.read(member).decode("utf-8", errors="replace")
                stats_lines = [line for line in text.splitlines() if line.startswith(PREFIX)]
                if not stats_lines:
                    continue
                values = {key.replace("-", "_"): int(value) for key, value in PAIR_RE.findall(stats_lines[-1])}
                rows.append(
                    {
                        "archive": archive.name,
                        "log": Path(member).name,
                        **values,
                    }
                )
    return rows, log_count


def summarize(rows: list[dict[str, int | str]], log_count: int) -> dict[str, object]:
    keys = sorted({key for row in rows for key in row if key not in {"archive", "log"}})
    distributions: dict[str, dict[str, int | float]] = {}
    for key in keys:
        values = [int(row.get(key, 0)) for row in rows]
        distributions[key] = {
            "sum": sum(values),
            "min": min(values, default=0),
            "median": median(values) if values else 0,
            "p90": percentile(values, 0.90),
            "p95": percentile(values, 0.95),
            "p99": percentile(values, 0.99),
            "max": max(values, default=0),
        }

    timing_keys = [key for key in keys if key.endswith("_ns")]
    timing_totals = {key: sum(int(row.get(key, 0)) for row in rows) for key in timing_keys}
    sync_total = timing_totals.get("sync_ns", 0)
    timing_share_of_sync = {
        key: (value / sync_total if sync_total else 0.0)
        for key, value in timing_totals.items()
        if key != "sync_ns"
    }

    ranking_keys = [
        "sync_ns",
        "offline_ns",
        "copy_ns",
        "worklist_ns",
        "checkpoint_ns",
        "rollback_ns",
        "history_ns",
        "rebuild_ns",
        "peak_undo_bytes",
        "peak_snapshot_equivalent_bytes",
        "max_active_events",
        "max_history_base_bytes",
    ]
    top_tasks = {
        key: [
            {"log": str(row["log"]), "value": int(row.get(key, 0))}
            for row in sorted(rows, key=lambda item: int(item.get(key, 0)), reverse=True)[:10]
            if int(row.get(key, 0)) > 0
        ]
        for key in ranking_keys
        if key in keys
    }

    return {
        "archives": sorted({str(row["archive"]) for row in rows}),
        "logs": log_count,
        "logs_with_stats": len(rows),
        "logs_without_stats": log_count - len(rows),
        "distributions": distributions,
        "timing_totals_ns": timing_totals,
        "timing_share_of_sync": timing_share_of_sync,
        "top_tasks": top_tasks,
    }


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path, help="Directory containing BenchExec logfiles.zip archives")
    parser.add_argument("--output-prefix", type=Path, required=True)
    args = parser.parse_args()

    archives = sorted(args.input.rglob("*.logfiles.zip"))
    if not archives:
        raise SystemExit(f"no logfiles.zip archives below {args.input}")
    rows, log_count = read_rows(archives)
    if not rows:
        raise SystemExit("archives contain no CAAT statistics")

    args.output_prefix.parent.mkdir(parents=True, exist_ok=True)
    keys = sorted({key for row in rows for key in row if key not in {"archive", "log"}})
    with args.output_prefix.with_suffix(".tsv").open("w", newline="", encoding="utf-8") as out:
        writer = csv.DictWriter(out, fieldnames=["archive", "log", *keys], delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)
    args.output_prefix.with_suffix(".json").write_text(
        json.dumps(summarize(rows, log_count), indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
