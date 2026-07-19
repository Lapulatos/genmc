#!/usr/bin/env python3
"""Normalize BenchExec XML files into a stable TSV and JSON summary."""

from __future__ import annotations

import argparse
import bz2
from collections import Counter
import csv
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET
import zipfile


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("xml", type=Path, nargs="+")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--summary", type=Path)
    parser.add_argument(
        "--log-archive",
        type=Path,
        nargs="+",
        help="ZIP archive(s) or log directories used to backfill custom columns",
    )
    return parser.parse_args()


def strip_unit(value: str, suffix: str) -> str:
    return value[: -len(suffix)] if value.endswith(suffix) else value


def parse_log_name(name: str) -> tuple[str, str, str] | None:
    basename = Path(name).name
    match = re.fullmatch(r"([^.]+)\.(r[0-9]+)\.(.+\.yml)\.log", basename)
    if match:
        return match.group(1), match.group(2), match.group(3)
    match = re.fullmatch(r"([^.]+)\.(.+\.yml)\.log", basename)
    if match:
        return match.group(1), "r01", match.group(2)
    return None


def custom_values(text: str) -> dict[str, str]:
    patterns = {
        "executions": r"Number of complete executions explored:\s*([0-9]+)",
        "blocked": r"Number of blocked executions seen:\s*([0-9]+)",
    }
    for identifier in (
        "max-active-events", "max-stable-events", "max-inactive-events",
        "max-current-base-bytes", "max-history-base-bytes",
        "max-base-relation-pairs", "max-base-relation-density-ppm",
        "rebuild", "profiled-queries",
    ):
        patterns[identifier] = rf"\b{re.escape(identifier)}=([0-9]+)\b"
    return {
        name: matches[-1]
        for name, pattern in patterns.items()
        if (matches := re.findall(pattern, text))
    }


def load_log_values(paths: list[Path] | None) -> dict[tuple[str, str, str], dict[str, str]]:
    result: dict[tuple[str, str, str], dict[str, str]] = {}
    for path in paths or []:
        if path.is_dir():
            entries = (
                (str(log.relative_to(path)), log.read_text(encoding="utf-8", errors="ignore"))
                for log in path.rglob("*.log")
            )
        else:
            archive = zipfile.ZipFile(path)
            entries = (
                (name, archive.read(name).decode("utf-8", errors="ignore"))
                for name in archive.namelist()
                if name.endswith(".log")
            )
        for name, text in entries:
            key = parse_log_name(name)
            if key:
                result[key] = custom_values(text)
        if not path.is_dir():
            archive.close()
    return result


def main() -> int:
    args = parse_args()
    rows: list[dict[str, str]] = []
    metadata: list[dict[str, str]] = []
    log_values = load_log_values(args.log_archive)
    for xml_path in args.xml:
        if xml_path.suffix == ".bz2":
            with bz2.open(xml_path, "rb") as source:
                root = ET.parse(source).getroot()
        else:
            root = ET.parse(xml_path).getroot()
        run_name = root.get("name", "")
        name_parts = run_name.split(".")
        backend = name_parts[0]
        repetition = (
            name_parts[1]
            if len(name_parts) > 1 and re.fullmatch(r"r[0-9]+", name_parts[1])
            else "r01"
        )
        metadata.append(
            {
                "file": str(xml_path),
                "backend": backend,
                "repetition": repetition,
                "tool": root.get("tool", ""),
                "version": root.get("version", ""),
                "date": root.get("date", ""),
                "timelimit": root.get("timelimit", ""),
                "memlimit": root.get("memlimit", ""),
            }
        )
        for run in root.findall("run"):
            columns = {
                column.get("title", ""): column.get("value", "")
                for column in run.findall("column")
            }
            task_yaml = run.get("name", "")
            marker = "sv-benchmarks/"
            if marker in task_yaml:
                task_yaml = task_yaml.split(marker, 1)[1]
            recovered = log_values.get(
                (backend, repetition, Path(task_yaml).name), {}
            )
            rows.append(
                {
                    "backend": backend,
                    "repetition": repetition,
                    "task_yaml": task_yaml,
                    "property": run.get("properties", ""),
                    "expected_verdict": run.get("expectedVerdict", ""),
                    "status": columns.get("status", ""),
                    "category": columns.get("category", ""),
                    "cputime_seconds": strip_unit(columns.get("cputime", ""), "s"),
                    "walltime_seconds": strip_unit(columns.get("walltime", ""), "s"),
                    "memory_bytes": strip_unit(columns.get("memory", ""), "B"),
                    "returnvalue": columns.get("returnvalue", ""),
                    "exitsignal": columns.get("exitsignal", ""),
                    "starttime": columns.get("starttime", ""),
                    "executions": columns.get("executions", "")
                    or recovered.get("executions", ""),
                    "blocked": columns.get("blocked", "")
                    or recovered.get("blocked", ""),
                    "max_active_events": columns.get("max-active-events", "")
                    or recovered.get("max-active-events", ""),
                    "max_stable_events": columns.get("max-stable-events", "")
                    or recovered.get("max-stable-events", ""),
                    "max_inactive_events": columns.get("max-inactive-events", "")
                    or recovered.get("max-inactive-events", ""),
                    "max_current_base_bytes": columns.get("max-current-base-bytes", "")
                    or recovered.get("max-current-base-bytes", ""),
                    "max_history_base_bytes": columns.get("max-history-base-bytes", "")
                    or recovered.get("max-history-base-bytes", ""),
                    "max_base_relation_pairs": columns.get("max-base-relation-pairs", "")
                    or recovered.get("max-base-relation-pairs", ""),
                    "max_base_relation_density_ppm": columns.get(
                        "max-base-relation-density-ppm", ""
                    ) or recovered.get("max-base-relation-density-ppm", ""),
                    "rebuilds": columns.get("rebuilds", "")
                    or recovered.get("rebuild", ""),
                    "profiled_queries": columns.get("profiled-queries", "")
                    or recovered.get("profiled-queries", ""),
                }
            )

    rows.sort(
        key=lambda row: (
            row["backend"],
            row["task_yaml"],
            row["property"],
            row["repetition"],
        )
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=list(rows[0]), delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)

    by_backend: dict[str, dict[str, object]] = {}
    for backend in sorted({row["backend"] for row in rows}):
        selected = [row for row in rows if row["backend"] == backend]
        status = Counter(row["status"] for row in selected)
        category = Counter(row["category"] for row in selected)
        by_backend[backend] = {
            "runs": len(selected),
            "status": dict(status.most_common()),
            "category": dict(category.most_common()),
            "completed": sum(
                value
                for key, value in status.items()
                if key == "true" or key.startswith("false")
            ),
        }
    summary_data = {"metadata": metadata, "by_backend": by_backend}
    summary = args.summary or args.output.with_suffix(".summary.json")
    summary.write_text(
        json.dumps(summary_data, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    print(f"wrote {len(rows)} runs to {args.output}")
    print(summary)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
