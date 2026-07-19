#!/usr/bin/env python3
"""Analyze Optimization 12 CAT/CAAT packed-value profile archives."""

from __future__ import annotations

import argparse
import bz2
import csv
import json
import re
import statistics
import xml.etree.ElementTree as ET
import zipfile
from collections import Counter
from pathlib import Path


INTEGER = re.compile(r"([a-z][a-z0-9-]*)=([0-9]+)")
BASE = re.compile(
    r'^CAT base value profile: name="([^"]*)" type=(set|relation) '
    r"max-universe=([0-9]+) max-facts=([0-9]+) max-bytes=([0-9]+)$"
)
PREDICATE = re.compile(
    r'^CAT predicate value profile: id=([0-9]+) name="([^"]*)" kind=([0-9]+) '
    r"type=(set|relation) max-universe=([0-9]+) max-sampled-facts=([0-9]+) "
    r"max-bytes=([0-9]+) attempts=([0-9]+)(?: evaluation-ns=([0-9]+))?$"
)
PREDICATE_KIND_NAMES = {
    0: "Base",
    1: "Alias",
    2: "Union",
    3: "Composition",
    4: "Difference",
    5: "Intersection",
    6: "Product",
    7: "Identity",
    8: "Domain",
    9: "Range",
    10: "Inverse",
    11: "Optional",
    12: "TransitiveClosure",
    13: "ReflexiveTransitiveClosure",
}


def median(values: list[float | int]) -> float:
    return float(statistics.median(values)) if values else 0.0


def projected_sparse(kind: str, universe: int, facts: int) -> int:
    if kind == "set":
        return (universe + 7) // 8
    # Conservative 32-bit CSR payload: row offsets plus target IDs.
    return 4 * (universe + 1) + 4 * facts


def projected_structural_base(value: dict) -> int:
    """Conservative payload for a typed primitive view, excluding existing event fields."""
    universe = value["universe"]
    name = value["name"]
    if name in {"loc", "int", "ext", "fr"}:
        return 0
    if name in {"po", "co"}:
        return 8 * universe
    if name in {"rf", "rmw"}:
        return 4 * universe
    return min(value["bytes"], projected_sparse(value["type"], universe, value["facts"]))


def parse_xml(path: Path) -> dict[str, dict[str, str | int | float]]:
    with bz2.open(path, "rb") as source:
        root = ET.parse(source).getroot()
    rows: dict[str, dict[str, str | int | float]] = {}
    for run in root.findall("run"):
        columns = {column.attrib["title"]: column.attrib.get("value", "") for column in run}
        task = Path(run.attrib["name"]).name
        rows[task] = {
            "status": columns.get("status", "missing"),
            "cpu_s": float(str(columns.get("cputime", "0s")).removesuffix("s") or 0),
            "wall_s": float(str(columns.get("walltime", "0s")).removesuffix("s") or 0),
            "rss_bytes": int(str(columns.get("memory", "0B")).removesuffix("B") or 0),
        }
    return rows


def update_peak(target: dict, key, record: dict) -> None:
    previous = target.get(key)
    if previous is None:
        target[key] = record
        return
    for field in ("universe", "facts", "bytes", "attempts", "evaluation_ns"):
        previous[field] = max(previous[field], record[field])


def parse_log(text: str) -> tuple[dict[str, int], dict, dict, Counter]:
    main: dict[str, int] = {}
    bases: dict[str, dict] = {}
    predicates: dict[tuple[int, str, int, str], dict] = {}
    stages: Counter = Counter()
    for line in text.splitlines():
        if line.startswith("CAT incremental statistics:"):
            stage = re.search(r"\bstage=([a-z]+)", line)
            stages[stage.group(1) if stage else "legacy"] += 1
            for key, raw in INTEGER.findall(line):
                main[key] = max(main.get(key, 0), int(raw))
            continue
        match = BASE.match(line)
        if match:
            name, kind, universe, facts, storage = match.groups()
            update_peak(
                bases,
                name,
                {
                    "scope": "base",
                    "id": -1,
                    "name": name,
                    "predicate_kind": -1,
                    "type": kind,
                    "universe": int(universe),
                    "facts": int(facts),
                    "bytes": int(storage),
                    "attempts": 0,
                    "evaluation_ns": 0,
                },
            )
            continue
        match = PREDICATE.match(line)
        if match:
            pred_id, name, pred_kind, kind, universe, facts, storage, attempts, elapsed = (
                match.groups()
            )
            key = (int(pred_id), name, int(pred_kind), kind)
            update_peak(
                predicates,
                key,
                {
                    "scope": "predicate",
                    "id": int(pred_id),
                    "name": name,
                    "predicate_kind": int(pred_kind),
                    "type": kind,
                    "universe": int(universe),
                    "facts": int(facts),
                    "bytes": int(storage),
                    "attempts": int(attempts),
                    "evaluation_ns": int(elapsed or 0),
                },
            )
    return main, bases, predicates, stages


def model_from(path: Path) -> str:
    return path.parent.name


def task_from_member(member: str) -> str:
    name = Path(member).name
    markers = (".structural-census.", ".fair-preventive-timeout-oom.")
    marker = next((candidate for candidate in markers if candidate in name), None)
    if marker is None or not name.endswith(".log"):
        raise ValueError(f"unexpected log member: {member}")
    return name.split(marker, 1)[1][:-4]


def write_tsv(path: Path, fields: list[str], rows: list[dict]) -> None:
    with path.open("w", newline="", encoding="utf-8") as output:
        writer = csv.DictWriter(output, fieldnames=fields, delimiter="\t", extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    task_rows: list[dict] = []
    value_rows: list[dict] = []
    for archive in sorted(args.input.glob("*/*.logfiles.zip")):
        model = model_from(archive)
        xml_path = next(archive.parent.glob("*.xml.bz2"))
        xml_rows = parse_xml(xml_path)
        with zipfile.ZipFile(archive) as logs:
            for member in logs.namelist():
                task = task_from_member(member)
                main_stats, bases, predicates, stages = parse_log(
                    logs.read(member).decode("utf-8", errors="replace")
                )
                metadata = xml_rows[task]
                values = list(bases.values()) + list(predicates.values())
                for value in values:
                    value["model"] = model
                    value["task"] = task
                    value["sparse_bytes"] = projected_sparse(
                        value["type"], value["universe"], value["facts"]
                    )
                    value["best_bytes"] = min(value["bytes"], value["sparse_bytes"])
                    value_rows.append(dict(value))

                base_relation = sum(v["bytes"] for v in bases.values() if v["type"] == "relation")
                base_set = sum(v["bytes"] for v in bases.values() if v["type"] == "set")
                pred_relation = sum(
                    v["bytes"] for v in predicates.values() if v["type"] == "relation"
                )
                pred_set = sum(v["bytes"] for v in predicates.values() if v["type"] == "set")
                normalized_base_relation = sum(
                    v["bytes"]
                    for v in predicates.values()
                    if v["type"] == "relation" and v["predicate_kind"] == 0
                )
                reach_relation = sum(
                    v["bytes"]
                    for v in predicates.values()
                    if v["type"] == "relation" and v["name"] == "reach"
                )
                target_values = [
                    v
                    for v in values
                    if v["type"] == "relation"
                    and (
                        v["scope"] == "base"
                        or v["predicate_kind"] == 0
                        or v["name"] in {"order", "reach"}
                    )
                ]
                target_packed = sum(v["bytes"] for v in target_values)
                target_sparse = sum(
                    projected_sparse(v["type"], v["universe"], v["facts"])
                    for v in target_values
                )
                structural_projection = sum(
                    projected_structural_base(value)
                    for value in bases.values()
                    if value["type"] == "relation"
                )
                # Base normalized predicates become aliases to primitive views. Dense order
                # plus reach become one sparse direct-order graph and topological rank array.
                order = next(
                    (value for value in predicates.values() if value["name"] == "order"),
                    None,
                )
                reach_count = sum(
                    value["name"] == "reach" and value["type"] == "relation"
                    for value in predicates.values()
                )
                if reach_count:
                    if order:
                        structural_projection += reach_count * (
                            8 * order["universe"]
                            + projected_sparse("relation", order["universe"], order["facts"])
                        )
                    else:
                        structural_projection += sum(
                            projected_sparse(value["type"], value["universe"], value["facts"])
                            for value in predicates.values()
                            if value["name"] == "reach" and value["type"] == "relation"
                        )
                task_rows.append(
                    {
                        "model": model,
                        "task": task,
                        **metadata,
                        "profile_records": sum(stages.values()),
                        "checkpoint_records": stages["checkpoint"],
                        "final_records": stages["final"],
                        "max_active_events": main_stats.get("max-active-events", 0),
                        "max_stable_events": main_stats.get("max-stable-events", 0),
                        "profiled_queries": main_stats.get("profiled-queries", 0),
                        "max_current_base_bytes": main_stats.get("max-current-base-bytes", 0),
                        "max_published_predicate_bytes": main_stats.get(
                            "max-published-predicate-bytes", 0
                        ),
                        "max_transactional_predicate_bytes_lower_bound": main_stats.get(
                            "max-transactional-predicate-bytes-lower-bound", 0
                        ),
                        "base_relation_peak_sum": base_relation,
                        "base_set_peak_sum": base_set,
                        "predicate_relation_peak_sum": pred_relation,
                        "predicate_set_peak_sum": pred_set,
                        "predicate_evaluation_ns_sum": sum(
                            value["evaluation_ns"] for value in predicates.values()
                        ),
                        "normalized_base_relation_peak_sum": normalized_base_relation,
                        "reach_relation_peak_sum": reach_relation,
                        "target_relation_peak_sum": target_packed,
                        "all_relation_peak_sum": base_relation + pred_relation,
                        "target_relation_share": (
                            target_packed / (base_relation + pred_relation)
                            if base_relation + pred_relation
                            else 0
                        ),
                        "target_sparse_projection": target_sparse,
                        "target_packed_to_sparse": (
                            target_packed / target_sparse if target_sparse else 0
                        ),
                        "target_structural_projection": structural_projection,
                        "target_packed_to_structural": (
                            target_packed / structural_projection
                            if structural_projection
                            else 0
                        ),
                    }
                )

    fields = list(task_rows[0])
    write_tsv(args.output / "task-summary.tsv", fields, task_rows)
    write_tsv(
        args.output / "value-summary.tsv",
        [
            "model",
            "task",
            "scope",
            "id",
            "name",
            "predicate_kind",
            "type",
            "universe",
            "facts",
            "bytes",
            "attempts",
            "evaluation_ns",
            "sparse_bytes",
            "best_bytes",
        ],
        value_rows,
    )

    profiled = [row for row in task_rows if row["profile_records"]]
    large = [row for row in profiled if row["max_stable_events"] >= 512]
    resources = [row for row in task_rows if row["status"] in {"TIMEOUT", "OUT OF MEMORY"}]
    resource_profiled = [row for row in resources if row["profile_records"]]
    by_model = {}
    predicate_hotspots = {}
    predicate_kind_time = {}
    for model in ("sc", "tso", "pso"):
        rows = [row for row in task_rows if row["model"] == model]
        observed = [row for row in rows if row["profile_records"]]
        by_model[model] = {
            "tasks": len(rows),
            "statuses": dict(Counter(str(row["status"]) for row in rows)),
            "profiled_tasks": len(observed),
            "resource_tasks": sum(
                row["status"] in {"TIMEOUT", "OUT OF MEMORY"} for row in rows
            ),
            "resource_profiled": sum(
                row["status"] in {"TIMEOUT", "OUT OF MEMORY"}
                and row["profile_records"] > 0
                for row in rows
            ),
            "median_events_profiled": median([row["max_stable_events"] for row in observed]),
            "max_events": max((row["max_stable_events"] for row in observed), default=0),
            "median_current_base_bytes": median(
                [row["max_current_base_bytes"] for row in observed]
            ),
            "median_published_predicate_bytes": median(
                [row["max_published_predicate_bytes"] for row in observed]
            ),
            "median_target_relation_share": median(
                [row["target_relation_share"] for row in observed]
            ),
            "median_target_packed_to_sparse_large": median(
                [row["target_packed_to_sparse"] for row in observed if row["max_stable_events"] >= 512]
            ),
            "median_target_packed_to_structural_large": median(
                [
                    row["target_packed_to_structural"]
                    for row in observed
                    if row["max_stable_events"] >= 512
                ]
            ),
        }
        aggregated: dict[tuple, dict] = {}
        for value in value_rows:
            if value["model"] != model or value["scope"] != "predicate":
                continue
            key = (
                value["id"],
                value["name"],
                value["predicate_kind"],
                value["type"],
            )
            record = aggregated.setdefault(
                key,
                {
                    "id": value["id"],
                    "name": value["name"],
                    "predicate_kind": value["predicate_kind"],
                    "kind_name": PREDICATE_KIND_NAMES.get(
                        value["predicate_kind"], f"Kind{value['predicate_kind']}"
                    ),
                    "type": value["type"],
                    "evaluation_ns": 0,
                    "attempts": 0,
                    "maximum_bytes": 0,
                },
            )
            record["evaluation_ns"] += value["evaluation_ns"]
            record["attempts"] += value["attempts"]
            record["maximum_bytes"] = max(record["maximum_bytes"], value["bytes"])
        predicate_hotspots[model] = sorted(
            aggregated.values(), key=lambda record: record["evaluation_ns"], reverse=True
        )[:12]
        kind_totals: Counter = Counter()
        for record in aggregated.values():
            kind_totals[record["kind_name"]] += record["evaluation_ns"]
        total_time = sum(kind_totals.values())
        predicate_kind_time[model] = [
            {
                "kind": kind,
                "evaluation_ns": elapsed,
                "share": elapsed / total_time if total_time else 0,
            }
            for kind, elapsed in kind_totals.most_common()
        ]

    summary = {
        "tasks": len(task_rows),
        "profiled_tasks": len(profiled),
        "resource_tasks": len(resources),
        "resource_profiled": len(resource_profiled),
        "large_profiled_tasks": len(large),
        "opportunity_gate": {
            "median_target_relation_share_at_least_0_30": median(
                [row["target_relation_share"] for row in profiled]
            )
            >= 0.30,
            "median_large_packed_to_sparse_at_least_2": median(
                [row["target_packed_to_sparse"] for row in large]
            )
            >= 2.0,
            "median_large_packed_to_structural_at_least_2": median(
                [row["target_packed_to_structural"] for row in large]
            )
            >= 2.0,
        },
        "by_model": by_model,
        "predicate_hotspots": predicate_hotspots,
        "predicate_kind_time": predicate_kind_time,
        "largest_event_tasks": sorted(
            profiled, key=lambda row: row["max_stable_events"], reverse=True
        )[:15],
    }
    (args.output / "summary.json").write_text(
        json.dumps(summary, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )

    report = [
        "# Optimization 12 structural storage census",
        "",
        f"- Cells: {summary['tasks']}; profiles recovered: {summary['profiled_tasks']}.",
        f"- TIMEOUT/OOM cells: {summary['resource_tasks']}; checkpoint profiles recovered: {summary['resource_profiled']}.",
        f"- Profiled cells with at least 512 stable events: {summary['large_profiled_tasks']}.",
        "- Sparse projection is a conservative 32-bit CSR payload: 4*(N+1)+4*E bytes; it excludes allocator and index metadata.",
        "- Structural projection is not generic CSR: po/co use O(N) chains, loc/int/ext are attribute views, fr is derived on demand, normalized Base nodes alias primitives, and reach uses direct order edges plus topological ranks.",
        "- Per-value maxima need not occur simultaneously; `*_peak_sum` is an opportunity upper bound, while the main packed-byte counters are exact observed aggregate peaks.",
        "",
        "## Opportunity gate",
        "",
        f"- Target relations occupy >=30% of relation peak sums: {summary['opportunity_gate']['median_target_relation_share_at_least_0_30']}.",
        f"- For N>=512, target packed/sparse median is >=2x: {summary['opportunity_gate']['median_large_packed_to_sparse_at_least_2']}.",
        f"- For N>=512, target packed/structural-view median is >=2x: {summary['opportunity_gate']['median_large_packed_to_structural_at_least_2']}.",
        "",
        "## By model",
        "",
        "| Model | Profiled | Resource profiled | Median events | Max events | Median base bytes | Median predicate bytes | Median target share | Large packed/sparse | Large packed/structural |",
        "|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|",
    ]
    for model, row in by_model.items():
        report.append(
            f"| {model.upper()} | {row['profiled_tasks']}/{row['tasks']} | "
            f"{row['resource_profiled']}/{row['resource_tasks']} | "
            f"{row['median_events_profiled']:.0f} | {row['max_events']} | "
            f"{row['median_current_base_bytes']:.0f} | "
            f"{row['median_published_predicate_bytes']:.0f} | "
            f"{row['median_target_relation_share']:.3f} | "
            f"{row['median_target_packed_to_sparse_large']:.2f}x | "
            f"{row['median_target_packed_to_structural_large']:.2f}x |"
        )
    report += ["", "## Predicate operation-time hotspots", ""]
    for model in ("sc", "tso", "pso"):
        report += [
            f"### {model.upper()}",
            "",
            "| ID | Name | Kind | Type | Evaluation ns | Attempts | Maximum bytes |",
            "|---:|---|---:|---|---:|---:|---:|",
        ]
        for row in predicate_hotspots[model]:
            report.append(
                f"| {row['id']} | {row['name']} | {row['kind_name']} | "
                f"{row['type']} | {row['evaluation_ns']} | {row['attempts']} | "
                f"{row['maximum_bytes']} |"
            )
        report.append("")
        report.append(
            "Kind totals: "
            + ", ".join(
                f"{row['kind']}={row['share']:.1%}"
                for row in predicate_kind_time[model]
                if row["evaluation_ns"]
            )
            + "."
        )
        report.append("")
    report += ["", "## Largest observed graphs", "", "| Model | Task | Status | Events | RSS | Base bytes | Predicate bytes | Target share | Packed/sparse | Packed/structural |", "|---|---|---|---:|---:|---:|---:|---:|---:|---:|"]
    for row in summary["largest_event_tasks"]:
        report.append(
            f"| {str(row['model']).upper()} | {row['task']} | {row['status']} | "
            f"{row['max_stable_events']} | {row['rss_bytes']} | "
            f"{row['max_current_base_bytes']} | {row['max_published_predicate_bytes']} | "
            f"{row['target_relation_share']:.3f} | {row['target_packed_to_sparse']:.2f}x | "
            f"{row['target_packed_to_structural']:.2f}x |"
        )
    (args.output / "report.md").write_text("\n".join(report) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
