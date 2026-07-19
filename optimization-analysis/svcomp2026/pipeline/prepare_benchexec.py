#!/usr/bin/env python3
"""Generate deterministic task sets and BenchExec XML for GenMC experiments."""

from __future__ import annotations

import argparse
import bz2
import csv
from pathlib import Path
import xml.etree.ElementTree as ET
from xml.sax.saxutils import escape


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("experiment_root", type=Path)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("--property", default="unreach-call")
    parser.add_argument("--limit", type=int, default=0)
    parser.add_argument("--name", default="genmc-census")
    parser.add_argument("--tool-module", default="pipeline.tools.genmc_svcomp")
    parser.add_argument(
        "--plain-rundefinitions",
        action="store_true",
        help="do not pass the internal --svcomp-backend selector",
    )
    parser.add_argument(
        "--no-common-options",
        action="store_true",
        help="omit GenMC-specific common command-line options",
    )
    parser.add_argument(
        "--cat-stats",
        action="store_true",
        help="enable opt-in CAT/CAAT diagnostics and expose scale columns",
    )
    parser.add_argument("--time-limit", default="10 s")
    parser.add_argument("--memory-limit", default="4 GB")
    parser.add_argument("--repetitions", type=int, default=1)
    parser.add_argument(
        "--backends",
        nargs="+",
        default=["genmc-sc", "cat-sc", "caat-sc"],
    )
    parser.add_argument(
        "--census-xml",
        type=Path,
        nargs="+",
        help="retain only tasks categorized correct in this BenchExec result",
    )
    parser.add_argument(
        "--allowlist",
        type=Path,
        help="newline-delimited task YAML paths to retain",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = args.experiment_root.resolve()
    with args.manifest.open(encoding="utf-8", newline="") as source:
        rows = [
            row
            for row in csv.DictReader(source, delimiter="\t")
            if row["property"] == args.property and row["expected_verdict"]
        ]
    rows.sort(
        key=lambda row: (
            int(row.get("adapter_transitive_bytes") or row["source_bytes"]),
            row["task_yaml"],
        )
    )
    if args.census_xml:
        correct_sets: list[set[str]] = []
        for census_xml in args.census_xml:
            correct: set[str] = set()
            if census_xml.suffix == ".bz2":
                with bz2.open(census_xml, "rb") as source:
                    result_root = ET.parse(source).getroot()
            else:
                result_root = ET.parse(census_xml).getroot()
            for run in result_root.findall("run"):
                columns = {
                    column.get("title", ""): column.get("value", "")
                    for column in run.findall("column")
                }
                if columns.get("category") != "correct":
                    continue
                name = run.get("name", "")
                marker = "sv-benchmarks/"
                correct.add(name.split(marker, 1)[1] if marker in name else name)
            correct_sets.append(correct)
        correct = set.intersection(*correct_sets)
        rows = [row for row in rows if row["task_yaml"] in correct]
    if args.allowlist:
        allowed = {
            line.strip()
            for line in args.allowlist.read_text(encoding="utf-8").splitlines()
            if line.strip() and not line.startswith("#")
        }
        rows = [row for row in rows if row["task_yaml"] in allowed]
    if args.limit:
        rows = rows[: args.limit]
    if not rows:
        raise SystemExit("selection is empty")

    definitions = root / "definitions"
    definitions.mkdir(parents=True, exist_ok=True)
    task_set = definitions / f"{args.name}.set"
    benchmark_root = root / "sv-benchmarks"
    task_set.write_text(
        "".join(
            f"{(benchmark_root / row['task_yaml']).resolve()}\n" for row in rows
        ),
        encoding="utf-8",
    )

    property_file = benchmark_root / "c" / "properties" / f"{args.property}.prp"
    if args.repetitions < 1:
        raise SystemExit("--repetitions must be at least 1")

    runs: list[tuple[str, int]] = []
    for repetition in range(1, args.repetitions + 1):
        offset = (repetition - 1) % len(args.backends)
        rotated = args.backends[offset:] + args.backends[:offset]
        runs.extend((backend, repetition) for backend in rotated)

    run_definitions = "\n".join(
        f'''  <rundefinition name="{escape(backend)}.r{repetition:02d}">
    {"" if args.plain_rundefinitions else f'<option name="--svcomp-backend={escape(backend)}"/>'}
    <tasks name="{escape(args.property)}">
      <includesfile>{escape(str(task_set))}</includesfile>
      <propertyfile>{escape(str(property_file))}</propertyfile>
    </tasks>
  </rundefinition>'''
        for backend, repetition in runs
    )
    common_options = "" if args.no_common_options else '''  <option name="--disable-estimation"/>
  <option name="--disable-mm-detector"/>
  <option name="--nthreads=1"/>
  <option name="--v1"/>'''
    if args.cat_stats:
        common_options += '\n  <option name="--cat-stats"/>'
    diagnostic_columns = "" if not args.cat_stats else '''
  <column title="max-active-events">max-active-events=([0-9]+)</column>
  <column title="max-stable-events">max-stable-events=([0-9]+)</column>
  <column title="max-inactive-events">max-inactive-events=([0-9]+)</column>
  <column title="max-current-base-bytes">max-current-base-bytes=([0-9]+)</column>
  <column title="max-history-base-bytes">max-history-base-bytes=([0-9]+)</column>
  <column title="max-base-relation-pairs">max-base-relation-pairs=([0-9]+)</column>
  <column title="max-base-relation-density-ppm">max-base-relation-density-ppm=([0-9]+)</column>
  <column title="rebuilds">rebuild=([0-9]+)</column>
  <column title="profiled-queries">profiled-queries=([0-9]+)</column>'''
    xml = f'''<?xml version="1.0"?>
<!DOCTYPE benchmark PUBLIC "+//IDN sosy-lab.org//DTD BenchExec benchmark 3.0//EN" "https://www.sosy-lab.org/benchexec/benchmark-3.0.dtd">
<benchmark tool="{escape(args.tool_module)}" timelimit="{escape(args.time_limit)}" memlimit="{escape(args.memory_limit)}" cpuCores="1">
{common_options}
  <columns>
  <column title="executions">Number of complete executions explored: (.*)</column>
  <column title="blocked">Number of blocked executions seen: (.*)</column>
{diagnostic_columns}
  </columns>
{run_definitions}
</benchmark>
'''
    xml_path = definitions / f"{args.name}.xml"
    xml_path.write_text(xml, encoding="utf-8")
    print(f"selected {len(rows)} tasks")
    print(f"generated {len(runs)} run definitions ({args.repetitions} repetitions)")
    print(task_set)
    print(xml_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
