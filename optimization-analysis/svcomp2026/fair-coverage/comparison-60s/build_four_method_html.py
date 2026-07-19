#!/usr/bin/env python3
"""Build one-column-per-method BenchExec HTML for the adapted 725-task census."""

from __future__ import annotations

import argparse
import bz2
import csv
from dataclasses import dataclass
import os
from pathlib import Path
import re
import shutil
import subprocess
import xml.etree.ElementTree as ET
from urllib.parse import unquote
import zipfile


@dataclass(frozen=True)
class Method:
    label: str
    slug: str
    result_root: Path
    xml_pattern: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("genmc_root", type=Path)
    parser.add_argument("comparison_root", type=Path)
    parser.add_argument("benchmark_root", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument(
        "--latest-five",
        action="store_true",
        help="compare native GenMC, the latest CAAT SC/TSO/PSO, and Deagle",
    )
    parser.add_argument(
        "--previous-latest-caat",
        action="store_true",
        help="compare the previous and latest CAAT-SC census runs",
    )
    return parser.parse_args()


def canonical_task(name: str) -> str:
    marker = "sv-benchmarks/c/"
    if marker not in name:
        raise ValueError(f"cannot canonicalize task path: {name}")
    return name.split(marker, 1)[1]


def read_xml(path: Path) -> ET.Element:
    with bz2.open(path, "rb") as source:
        return ET.parse(source).getroot()


def timestamp(path: Path) -> str:
    match = re.search(r"\.(\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2})\.", path.name)
    if not match:
        raise ValueError(f"cannot extract BenchExec timestamp from {path}")
    return match.group(1)


def copy_task_snapshot(tasks: set[str], benchmark_root: Path, raw: Path) -> None:
    destination = raw / "sv-benchmarks" / "c"
    for task in sorted(tasks):
        source = benchmark_root / "c" / task
        target = destination / task
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
    property_source = benchmark_root / "c/properties/unreach-call.prp"
    property_target = destination / "properties/unreach-call.prp"
    property_target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(property_source, property_target)


def extract_logs(
    archives: list[Path],
    source_prefixes: set[str],
    target_prefix: str,
    destination: Path,
) -> int:
    destination.mkdir(parents=True, exist_ok=True)
    written: set[str] = set()
    for archive in archives:
        with zipfile.ZipFile(archive) as source:
            for member in source.infolist():
                if member.is_dir():
                    continue
                basename = Path(member.filename).name
                source_prefix = next(
                    (prefix for prefix in source_prefixes if basename.startswith(prefix + ".")),
                    None,
                )
                if source_prefix is None:
                    raise ValueError(f"unexpected log name {basename} in {archive}")
                target_name = target_prefix + basename[len(source_prefix) :]
                if target_name in written:
                    raise ValueError(f"duplicate merged log {target_name}")
                written.add(target_name)
                (destination / target_name).write_bytes(source.read(member))
    return len(written)


def merge_method(method: Method, raw: Path) -> tuple[Path, set[str], int]:
    xml_paths = sorted(method.result_root.glob(method.xml_pattern))
    if not xml_paths:
        raise ValueError(f"no XML files for {method.label} under {method.result_root}")
    roots = [read_xml(path) for path in xml_paths]
    runs_by_task: dict[str, ET.Element] = {}
    source_prefixes: set[str] = set()
    for root in roots:
        source_prefixes.add(root.attrib["name"].removesuffix(".unreach-call"))
        for run in root.findall("run"):
            task = canonical_task(run.attrib["name"])
            if task in runs_by_task:
                raise ValueError(f"duplicate task for {method.label}: {task}")
            columns = {
                column.attrib.get("title", ""): column.attrib.get("value", "")
                for column in run.findall("column")
            }
            if not columns.get("status") or not columns.get("category"):
                raise ValueError(
                    f"incomplete result for {method.label}: {task} {columns!r}"
                )
            run.attrib["name"] = f"sv-benchmarks/c/{task}"
            run.attrib["propertyFile"] = "sv-benchmarks/c/properties/unreach-call.prp"
            runs_by_task[task] = run

    merged = roots[0]
    for run in list(merged.findall("run")):
        merged.remove(run)
    merged.attrib["benchmarkname"] = f"adapted-{method.slug}"
    merged.attrib["name"] = f"{method.label}.unreach-call"
    merged.attrib["block"] = "unreach-call"
    for task in sorted(runs_by_task):
        merged.append(runs_by_task[task])
    ET.indent(merged)

    stamp = timestamp(xml_paths[0])
    prefix = f"adapted-{method.slug}.{stamp}"
    xml_output = raw / f"{prefix}.results.{method.slug}.unreach-call.xml.bz2"
    with bz2.open(xml_output, "wb") as output:
        ET.ElementTree(merged).write(output, encoding="utf-8", xml_declaration=True)

    archives = sorted(method.result_root.glob("**/*.logfiles.zip"))
    if not archives:
        raise ValueError(f"no log archive for {method.label}")
    log_count = extract_logs(
        archives,
        source_prefixes,
        method.label,
        raw / f"{prefix}.logfiles",
    )
    return xml_output, set(runs_by_task), log_count


def table_run_sets(path: Path) -> tuple[str, ...]:
    with path.open(encoding="utf-8", newline="") as source:
        rows = list(csv.reader(source, delimiter="\t"))
    values = []
    for value in rows[1][2:]:
        if value and (not values or value != values[-1]):
            values.append(value)
    return tuple(values)


def verify_links(html_path: Path) -> int:
    text = html_path.read_text(encoding="utf-8")
    links = re.findall(r'"href": "([^"]+\.(?:yml|log))"', text)
    missing = [
        link
        for link in links
        if not (html_path.parent / unquote(link)).resolve().is_file()
    ]
    if missing:
        raise ValueError(f"{len(missing)}/{len(links)} missing HTML links: {missing[:5]}")
    return len(links)


def main() -> int:
    args = parse_args()
    output = args.output.resolve()
    if output.exists():
        shutil.rmtree(output)
    raw = output / "raw"
    html = output / "html"
    raw.mkdir(parents=True)
    html.mkdir()

    if args.latest_five and args.previous_latest_caat:
        raise SystemExit("choose at most one comparison profile")
    if args.latest_five:
        methods = (
            Method("GenMC", "genmc", args.genmc_root.resolve(), "*.xml.bz2"),
            Method("GenMC+CAAT-SC", "caat-sc", args.comparison_root.resolve() / "sc", "*.xml.bz2"),
            Method("GenMC+CAAT-TSO", "caat-tso", args.comparison_root.resolve() / "tso", "*.xml.bz2"),
            Method("GenMC+CAAT-PSO", "caat-pso", args.comparison_root.resolve() / "pso", "*.xml.bz2"),
            Method("Deagle", "deagle", args.comparison_root.resolve() / "deagle", "*.xml.bz2"),
        )
    elif args.previous_latest_caat:
        methods = (
            Method(
                "Previous GenMC+CAAT-SC",
                "previous-caat-sc",
                args.genmc_root.resolve(),
                "*.xml.bz2",
            ),
            Method(
                "Latest GenMC+CAAT-SC",
                "latest-caat-sc",
                args.comparison_root.resolve() / "sc",
                "*.xml.bz2",
            ),
        )
    else:
        methods = (
            Method("GenMC", "genmc", args.genmc_root.resolve(), "shard*/*.xml.bz2"),
            Method("GenMC+CAT", "cat", args.comparison_root.resolve() / "cat", "*.xml.bz2"),
            Method("GenMC+CAAT", "caat", args.comparison_root.resolve() / "caat", "*.xml.bz2"),
            Method("Deagle", "deagle", args.comparison_root.resolve() / "deagle", "*.xml.bz2"),
        )
    xml_inputs = []
    reference_tasks: set[str] | None = None
    total_logs = 0
    for method in methods:
        xml_path, tasks, log_count = merge_method(method, raw)
        if reference_tasks is None:
            reference_tasks = tasks
        elif tasks != reference_tasks:
            raise ValueError(
                f"task mismatch for {method.label}: "
                f"missing={len(reference_tasks - tasks)} extra={len(tasks - reference_tasks)}"
            )
        if log_count != len(tasks):
            raise ValueError(
                f"log mismatch for {method.label}: logs={log_count} tasks={len(tasks)}"
            )
        xml_inputs.append(xml_path)
        total_logs += log_count

    assert reference_tasks is not None
    if len(reference_tasks) != 725:
        raise ValueError(f"expected 725 tasks, found {len(reference_tasks)}")
    copy_task_snapshot(reference_tasks, args.benchmark_root.resolve(), raw)
    generator = shutil.which("table-generator")
    if not generator:
        raise SystemExit("table-generator is not installed")
    if args.latest_five:
        name = "adapted-725-genmc-caat-sc-tso-pso-deagle-60s"
    elif args.previous_latest_caat:
        name = "adapted-725-previous-vs-latest-caat-sc-60s"
    else:
        name = "adapted-725-genmc-cat-caat-deagle-60s"
    subprocess.run(
        [
            generator,
            "-q",
            "-f",
            "html",
            "-f",
            "csv",
            "-o",
            str(html),
            "-n",
            name,
            *(str(path) for path in xml_inputs),
        ],
        check=True,
        env=os.environ.copy(),
    )
    csv_path = html / f"{name}.table.csv"
    actual = table_run_sets(csv_path)
    expected = tuple(
        f"adapted-{method.slug}.{method.label}.unreach-call" for method in methods
    )
    if actual != expected:
        raise ValueError(f"method columns {actual!r}, expected {expected!r}")
    link_count = verify_links(html / f"{name}.table.html")
    print(f"tasks={len(reference_tasks)}")
    print(f"methods={len(methods)}")
    print(f"logs={total_logs}")
    print(f"verified_links={link_count}")
    print(html / f"{name}.table.html")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
