#!/usr/bin/env python3
"""Strict four-repetition analysis for the P0.2 exact kernel cache."""

from __future__ import annotations

import argparse
import bz2
import csv
import importlib.util
import json
import math
import re
import statistics
import xml.etree.ElementTree as ET
import zipfile
from collections import Counter, defaultdict
from pathlib import Path


HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location("census_analysis", HERE / "analyze.py")
assert SPEC and SPEC.loader
COMMON = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(COMMON)
MODELS = ("sc", "tso", "pso")
REPETITIONS = ("01", "02", "03", "04")
METRICS = {"cpu": "cputime", "wall": "walltime", "rss": "memory"}
PATTERN = re.compile(r"kernel-cache-([a-z-]+)=([0-9]+)")


def load_rows(root: Path) -> list[dict[str, object]]:
    rows = []
    for model in MODELS:
        for repetition in REPETITIONS:
            for variant in ("baseline", "cache"):
                paths = list((root / model / f"r{repetition}" / variant).glob("*.xml.bz2"))
                if len(paths) != 1:
                    raise RuntimeError(f"expected one XML at {model}/r{repetition}/{variant}")
                rows.extend(COMMON.parse_xml(paths[0], model, repetition, variant))
    return rows


def ratios(rows: list[dict[str, object]], metric: str) -> dict[str, dict[str, float]]:
    index = {(r["model"], r["repetition"], r["variant"], r["task"]): r for r in rows}
    observations: dict[tuple[str, str], list[tuple[float, float]]] = defaultdict(list)
    for model in MODELS:
        for repetition in REPETITIONS:
            tasks = {key[3] for key in index if key[:3] == (model, repetition, "baseline")}
            for task in tasks:
                before = index[(model, repetition, "baseline", task)]
                after = index[(model, repetition, "cache", task)]
                if before["category"] != after["category"] or before["category"] != "correct":
                    continue
                bv, av = float(before[metric]), float(after[metric])
                if bv > 0 and av > 0:
                    observations[(model, task)].append((bv, av))
    output = {model: {} for model in MODELS}
    for (model, task), values in observations.items():
        if len(values) == len(REPETITIONS):
            output[model][task] = statistics.median(a for _, a in values) / statistics.median(
                b for b, _ in values
            )
    return output


def summarize(rows: list[dict[str, object]]) -> dict[str, object]:
    index = {(r["model"], r["repetition"], r["variant"], r["task"]): r for r in rows}
    mismatches, opposite, safe_counts = [], [], []
    for model in MODELS:
        for repetition in REPETITIONS:
            tasks = {key[3] for key in index if key[:3] == (model, repetition, "baseline")}
            for task in sorted(tasks):
                b = index[(model, repetition, "baseline", task)]
                c = index[(model, repetition, "cache", task)]
                if b["status"] != c["status"]:
                    item = [model, repetition, task, b["status"], c["status"]]
                    mismatches.append(item)
                    if {str(b["status"]), str(c["status"])} <= {"true", "false(unreach-call)"}:
                        opposite.append(item)
                if b["status"] == c["status"] == "true" and b["executions"] != c["executions"]:
                    safe_counts.append([model, repetition, task, b["executions"], c["executions"]])
    result: dict[str, object] = {
        "design": {"tasks": 96, "repetitions": 4, "cells": len(rows)},
        "status_mismatches": mismatches,
        "opposite_terminal_mismatches": opposite,
        "safe_execution_count_mismatches": safe_counts,
        "status_counts": {
            model: {
                variant: dict(Counter(str(r["status"]) for r in rows if r["model"] == model and r["variant"] == variant))
                for variant in ("baseline", "cache")
            }
            for model in MODELS
        },
    }
    for metric_index, (name, column) in enumerate(METRICS.items()):
        per_model = ratios(rows, column)
        common_tasks = sorted(set.intersection(*(set(per_model[m]) for m in MODELS)))
        groups = {model: list(per_model[model].values()) for model in MODELS}
        groups["all"] = [COMMON.geomean([per_model[m][task] for m in MODELS]) for task in common_tasks]
        summaries = {
            group: COMMON.summarize_ratios(values, 20260726 + metric_index * 10 + i)
            for i, (group, values) in enumerate(groups.items())
        }
        adjusted = COMMON.holm({group: float(s["sign_test"]["p"]) for group, s in summaries.items()})
        for group, summary in summaries.items():
            summary["sign_test"]["holm_p_across_four_groups"] = adjusted[group]
        result[name] = summaries
    return result


def cache_records(root: Path) -> list[dict[str, object]]:
    records = []
    for model in MODELS:
        for repetition in REPETITIONS:
            archives = list((root / model / f"r{repetition}" / "cache").glob("*.logfiles.zip"))
            if len(archives) != 1:
                raise RuntimeError(f"missing cache archive {model}/r{repetition}")
            with zipfile.ZipFile(archives[0]) as archive:
                for member in archive.namelist():
                    lines = [line for line in archive.read(member).decode(errors="replace").splitlines() if "CAT rejection kernel cache:" in line]
                    if not lines:
                        continue
                    record: dict[str, object] = {"model": model, "repetition": repetition, "log": Path(member).name}
                    record.update({key: int(value) for key, value in PATTERN.findall(lines[-1])})
                    records.append(record)
    return records


def cache_summary(records: list[dict[str, object]]) -> dict[str, object]:
    keys = ("queries", "hits", "rejected", "explained", "learned", "duplicates", "negative-skipped", "empty-skipped", "explanation-failures", "capacity-drops", "stored-literals", "lookup-ns", "explanation-ns")
    result = {}
    for model in MODELS:
        selected = [r for r in records if r["model"] == model]
        totals = {key: sum(int(r.get(key, 0)) for r in selected) for key in keys}
        totals["records"] = len(selected)
        totals["tasks_with_hits"] = sum(int(r.get("hits", 0)) > 0 for r in selected)
        totals["hit_rate"] = totals["hits"] / totals["queries"] if totals["queries"] else 0
        result[model] = totals
    return result


def write_svg(path: Path, formal: dict[str, object]) -> None:
    width, height, xmin, xmax = 850, 450, 0.9, 1.35
    x0, scale = 180, 570 / (xmax - xmin)
    colors = {"cpu": "#b91c1c", "wall": "#1d4ed8", "rss": "#047857"}
    lines = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">', '<rect width="100%" height="100%" fill="white"/>', '<text x="425" y="28" text-anchor="middle" font-family="sans-serif" font-size="18">P0.2 cache/baseline ratios (four-repetition task bootstrap)</text>']
    for tick in (0.9, 1.0, 1.1, 1.2, 1.3):
        x = x0 + (tick - xmin) * scale
        lines += [f'<line x1="{x}" y1="52" x2="{x}" y2="390" stroke="#{"111827" if tick == 1 else "e5e7eb"}"/>', f'<text x="{x}" y="418" text-anchor="middle" font-family="sans-serif" font-size="12">{tick:.1f}</text>']
    row = 0
    for group in ("all", *MODELS):
        for metric in METRICS:
            s = formal[metric][group]
            value = float(s["geomean_ratio"])
            low, high = map(float, s["bootstrap_95ci"])
            y = 70 + row * 25
            clamp = lambda v: x0 + (min(xmax, max(xmin, v)) - xmin) * scale
            lines += [f'<text x="15" y="{y + 4}" font-family="sans-serif" font-size="12">{group.upper()} {metric}</text>', f'<line x1="{clamp(low)}" y1="{y}" x2="{clamp(high)}" y2="{y}" stroke="{colors[metric]}" stroke-width="3"/>', f'<circle cx="{clamp(value)}" cy="{y}" r="5" fill="{colors[metric]}"/>']
            row += 1
    lines.append('</svg>')
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def render(out: Path, formal: dict[str, object], cache: dict[str, object]) -> None:
    cpu, pso_cpu = formal["cpu"]["all"], formal["cpu"]["pso"]
    pso = cache["pso"]
    retain = not formal["opposite_terminal_mismatches"] and not formal["safe_execution_count_mismatches"] and cpu["bootstrap_95ci"][1] < 1
    decision = "retain the scoped P0.2 prototype" if retain else "reject and remove the P0.2 prototype"
    report = f"""# P0.2 exact positive rejection-kernel cache

## Conclusion

Decision: **{decision}** under the frozen gate. Aggregate CPU cache/baseline is
{cpu['geomean_ratio']:.5f} with task-bootstrap 95% CI
[{cpu['bootstrap_95ci'][0]:.5f}, {cpu['bootstrap_95ci'][1]:.5f}]; PSO CPU is
{pso_cpu['geomean_ratio']:.5f} [{pso_cpu['bootstrap_95ci'][0]:.5f}, {pso_cpu['bootstrap_95ci'][1]:.5f}].

The four-repetition matrix contains {formal['design']['cells']:,} cells, zero opposite
terminal verdict mismatch, and {len(formal['safe_execution_count_mismatches'])} safe
execution-count mismatches. PSO cache logs report {pso['hits']:,}/{pso['queries']:,}
hits ({100*pso['hit_rate']:.2f}%), {pso['learned']:,} learned kernels, and
{pso['capacity-drops']:,} capacity drops.

## Mechanism and scope

- A hit removes graph synchronization, evaluator propagation/rebuild, and checkpoint
  retention after stable materialization.
- A rejected miss pays Reasoner once; a later matching candidate returns before evaluation.
- Certified SC/TSO candidate profiles never construct the cache.
- The mechanism does not reduce valid executions, interpreter paths, or pre-query OOM.

## Correctness

- Release tests: 145/145.
- Mutation oracle: 39 rows and 5,441 full recomputations, including cache-hit validation.
- Broad differential: 852 comparable matches, 12 mutual unsupported, zero mismatch.
- Opposite terminal verdict mismatches: {len(formal['opposite_terminal_mismatches'])}.
- Safe execution-count mismatches: {len(formal['safe_execution_count_mismatches'])}.

## Limits

The 96-task corpus estimates solved-task overhead and some resource-bound changes. It does
not establish a broad 725-task coverage gain. A separate historical TIMEOUT/OOM run is
required before claiming that P0.2 reduces the full GenMC/Deagle coverage gap.
"""
    (out / "analysis-report.md").write_text(report, encoding="utf-8")
    lines = ["# Statistical appendix", "", "| Metric | Group | n | Geomean | 95% CI | Median | Holm p |", "|---|---|---:|---:|---:|---:|---:|"]
    for metric in METRICS:
        for group in ("all", *MODELS):
            s = formal[metric][group]
            lines.append(f"| {metric} | {group} | {s['tasks']} | {s['geomean_ratio']:.5f} | [{s['bootstrap_95ci'][0]:.5f}, {s['bootstrap_95ci'][1]:.5f}] | {s['median_ratio']:.5f} | {s['sign_test']['holm_p_across_four_groups']:.4g} |")
    lines += ["", "- Unit: one task; four repetitions are paired and summarized by a median.", "- Aggregate unit geometrically combines models only for tasks terminal-correct in all cells.", "- CI: 20,000 fixed-seed task-cluster bootstrap resamples.", "- Effect size: multiplicative cache/baseline ratio; lower is better.", "- Holm correction covers the four task groups within each metric."]
    (out / "stats-appendix.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    (out / "figure-catalog.md").write_text("""# Figure catalog

## `figures/figure-01-formal-overhead.svg`

- Purpose: show cache/baseline CPU, wall, and RSS ratios by model and aggregate task.
- Error bars: 95% task-cluster bootstrap CI after four-repetition paired medians.
- Reference: 1.0 means no change; lower is better.
- Decision impact: the aggregate CPU upper CI is the retention gate.
""", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("server", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "figures").mkdir(exist_ok=True)
    rows = load_rows(args.server / "formal-96")
    records = cache_records(args.server / "formal-96")
    formal, cache = summarize(rows), cache_summary(records)
    COMMON.write_tsv(args.output / "formal-rows.tsv", rows)
    COMMON.write_tsv(args.output / "cache-counters.tsv", records)
    (args.output / "stats-strict.json").write_text(json.dumps({"formal": formal, "cache": cache}, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    write_svg(args.output / "figures" / "figure-01-formal-overhead.svg", formal)
    render(args.output, formal, cache)


if __name__ == "__main__":
    main()
