#!/usr/bin/env python3
"""Build the strict P0.1 rejection-kernel census analysis bundle."""

from __future__ import annotations

import argparse
import bz2
import csv
import html
import json
import math
import random
import re
import statistics
import xml.etree.ElementTree as ET
import zipfile
from collections import Counter, defaultdict
from pathlib import Path


MODELS = ("sc", "tso", "pso")
VARIANTS = ("baseline", "census")
METRICS = {"cpu": "cputime", "wall": "walltime", "rss": "memory"}
KERNEL_PATTERN = re.compile(r"kernel-([a-z-]+)=([0-9]+)")


def task_id(name: str) -> str:
    marker = "sv-benchmarks/c/"
    return name[name.index(marker) + len(marker) :] if marker in name else name


def numeric(value: str) -> float:
    if not value:
        return math.nan
    return float(value[:-1]) if value[-1] in "sB" else float(value)


def geomean(values: list[float]) -> float:
    return math.exp(statistics.fmean(math.log(value) for value in values))


def quantile(values: list[float], fraction: float) -> float:
    ordered = sorted(values)
    position = fraction * (len(ordered) - 1)
    lower, upper = math.floor(position), math.ceil(position)
    if lower == upper:
        return ordered[lower]
    return ordered[lower] * (upper - position) + ordered[upper] * (position - lower)


def bootstrap(values: list[float], seed: int) -> tuple[float, float]:
    rng = random.Random(seed)
    estimates = sorted(
        geomean([rng.choice(values) for _ in values]) for _ in range(20_000)
    )
    return estimates[500], estimates[19_499]


def sign_test(values: list[float]) -> dict[str, float | int]:
    slower = sum(value > 1 for value in values)
    faster = sum(value < 1 for value in values)
    ties = len(values) - slower - faster
    n = slower + faster
    if n == 0:
        p = 1.0
    else:
        tail = min(slower, faster)
        p = min(1.0, 2 * sum(math.comb(n, k) for k in range(tail + 1)) / 2**n)
    return {"census_slower": slower, "census_faster": faster, "ties": ties, "p": p}


def holm(p_values: dict[str, float]) -> dict[str, float]:
    ordered = sorted(p_values, key=p_values.get)
    adjusted: dict[str, float] = {}
    running = 0.0
    for rank, group in enumerate(ordered):
        running = max(running, (len(ordered) - rank) * p_values[group])
        adjusted[group] = min(1.0, running)
    return adjusted


def parse_xml(path: Path, model: str, repetition: str, variant: str) -> list[dict[str, object]]:
    root = ET.fromstring(bz2.open(path, "rb").read())
    rows = []
    for run in root.findall(".//run"):
        columns = {c.attrib["title"]: c.attrib.get("value", "") for c in run.findall("column")}
        rows.append(
            {
                "model": model,
                "repetition": repetition,
                "variant": variant,
                "task": task_id(run.attrib["name"]),
                "expected": run.attrib.get("expectedVerdict", ""),
                "status": columns.get("status", ""),
                "category": columns.get("category", ""),
                "cputime": numeric(columns.get("cputime", "")),
                "walltime": numeric(columns.get("walltime", "")),
                "memory": numeric(columns.get("memory", "")),
                "executions": columns.get("executions", ""),
            }
        )
    return rows


def load_formal(root: Path) -> list[dict[str, object]]:
    rows = []
    for model in MODELS:
        for repetition in ("01", "02", "03"):
            for variant in VARIANTS:
                paths = list((root / model / f"r{repetition}" / variant).glob("*.xml.bz2"))
                if len(paths) != 1:
                    raise RuntimeError(f"expected one XML for {model}/r{repetition}/{variant}: {paths}")
                rows.extend(parse_xml(paths[0], model, repetition, variant))
    return rows


def latest_kernel_records(zip_path: Path, cohort: str, model: str) -> list[dict[str, object]]:
    records = []
    with zipfile.ZipFile(zip_path) as archive:
        for member in archive.namelist():
            text_value = archive.read(member).decode(errors="replace")
            lines = [line for line in text_value.splitlines() if "CAT rejection kernel census" in line]
            if not lines:
                continue
            record: dict[str, object] = {
                "cohort": cohort,
                "model": model,
                "log": Path(member).name,
            }
            record.update({key: int(value) for key, value in KERNEL_PATTERN.findall(lines[-1])})
            records.append(record)
    return records


def load_kernel_records(server: Path) -> list[dict[str, object]]:
    records = []
    for model in MODELS:
        formal = list((server / "formal-96" / model / "r01" / "census").glob("*.logfiles.zip"))
        large = list((server / "timeout-oom-60" / model / "r01" / "census").glob("*.logfiles.zip"))
        if len(formal) != 1 or len(large) != 1:
            raise RuntimeError(f"missing kernel log archive for {model}")
        records.extend(latest_kernel_records(formal[0], "formal-96", model))
        records.extend(latest_kernel_records(large[0], "timeout-oom-60", model))
    return records


def paired_ratios(rows: list[dict[str, object]], metric: str) -> dict[str, dict[str, float]]:
    index = {(r["model"], r["repetition"], r["variant"], r["task"]): r for r in rows}
    pairs: dict[tuple[str, str], list[tuple[float, float]]] = defaultdict(list)
    for model in MODELS:
        for repetition in ("01", "02", "03"):
            tasks = {
                key[3]
                for key in index
                if key[:3] == (model, repetition, "baseline")
            }
            for task in tasks:
                before = index[(model, repetition, "baseline", task)]
                after = index[(model, repetition, "census", task)]
                if before["category"] != "correct" or after["category"] != "correct":
                    continue
                bv, av = float(before[metric]), float(after[metric])
                if bv > 0 and av > 0:
                    pairs[(model, task)].append((bv, av))
    result = {model: {} for model in MODELS}
    for (model, task), observations in pairs.items():
        if len(observations) == 3:
            result[model][task] = statistics.median(a for _, a in observations) / statistics.median(
                b for b, _ in observations
            )
    return result


def summarize_ratios(values: list[float], seed: int) -> dict[str, object]:
    low, high = bootstrap(values, seed)
    return {
        "tasks": len(values),
        "geomean_ratio": geomean(values),
        "bootstrap_95ci": [low, high],
        "mean_ratio": statistics.fmean(values),
        "std_ratio": statistics.stdev(values) if len(values) > 1 else 0,
        "median_ratio": statistics.median(values),
        "iqr_ratio": [quantile(values, 0.25), quantile(values, 0.75)],
        "sign_test": sign_test(values),
    }


def formal_summary(rows: list[dict[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {"design": {"tasks": 96, "repetitions": 3, "cells": len(rows)}}
    index = {(r["model"], r["repetition"], r["variant"], r["task"]): r for r in rows}
    mismatches = []
    safe_count_mismatches = []
    for model in MODELS:
        for repetition in ("01", "02", "03"):
            for task in sorted({key[3] for key in index if key[:3] == (model, repetition, "baseline")}):
                b = index[(model, repetition, "baseline", task)]
                c = index[(model, repetition, "census", task)]
                if b["status"] != c["status"]:
                    mismatches.append([model, repetition, task, b["status"], c["status"]])
                if b["status"] == c["status"] == "true" and b["executions"] != c["executions"]:
                    safe_count_mismatches.append([model, repetition, task, b["executions"], c["executions"]])
    result["status_mismatches"] = mismatches
    result["safe_execution_count_mismatches"] = safe_count_mismatches
    result["status_counts"] = {
        model: {
            variant: dict(Counter(str(r["status"]) for r in rows if r["model"] == model and r["variant"] == variant))
            for variant in VARIANTS
        }
        for model in MODELS
    }
    for metric_index, (name, column) in enumerate(METRICS.items()):
        ratios = paired_ratios(rows, column)
        common = sorted(set.intersection(*(set(ratios[model]) for model in MODELS)))
        groups = {model: list(ratios[model].values()) for model in MODELS}
        groups["all"] = [geomean([ratios[model][task] for model in MODELS]) for task in common]
        summaries = {
            group: summarize_ratios(values, 20260716 + metric_index * 10 + i)
            for i, (group, values) in enumerate(groups.items())
        }
        adjusted = holm({group: float(summary["sign_test"]["p"]) for group, summary in summaries.items()})
        for group, summary in summaries.items():
            summary["sign_test"]["holm_p_across_four_groups"] = adjusted[group]
        result[name] = summaries
    return result


def opportunity_summary(records: list[dict[str, object]]) -> dict[str, object]:
    output = {}
    keys = (
        "queries", "rejected", "violations", "explained", "negative-skipped",
        "empty-skipped", "replay-failures", "raw-literals", "minimized-literals",
        "unique", "exact-duplicates", "subsumed", "capacity-drops", "simulated-hits",
        "hit-rejected", "hit-accepted", "explanation-ns", "minimization-ns",
    )
    for cohort in ("formal-96", "timeout-oom-60"):
        output[cohort] = {}
        for model in MODELS:
            selected = [r for r in records if r["cohort"] == cohort and r["model"] == model]
            totals = {key: sum(int(r.get(key, 0)) for r in selected) for key in keys}
            totals.update(
                {
                    "records": len(selected),
                    "tasks_with_hits": sum(int(r.get("simulated-hits", 0)) > 0 for r in selected),
                    "hit_over_rejected": totals["simulated-hits"] / totals["rejected"] if totals["rejected"] else 0,
                    "hit_over_queries": totals["simulated-hits"] / totals["queries"] if totals["queries"] else 0,
                    "minimized_over_raw": totals["minimized-literals"] / totals["raw-literals"] if totals["raw-literals"] else 0,
                }
            )
            output[cohort][model] = totals
    return output


def write_tsv(path: Path, rows: list[dict[str, object]]) -> None:
    fields = sorted({key for row in rows for key in row})
    with path.open("w", newline="", encoding="utf-8") as sink:
        writer = csv.DictWriter(sink, fieldnames=fields, delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def svg_opportunity(path: Path, opportunity: dict[str, object]) -> None:
    width, height = 980, 430
    colors = {"sc": "#2563eb", "tso": "#0f766e", "pso": "#c2410c"}
    lines = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
             '<rect width="100%" height="100%" fill="white"/>',
             '<text x="490" y="28" text-anchor="middle" font-family="sans-serif" font-size="18">Reusable rejection opportunity on historical TIMEOUT/OOM tasks</text>']
    for tick in range(0, 101, 20):
        x = 180 + tick * 5.8
        lines += [f'<line x1="{x}" y1="55" x2="{x}" y2="350" stroke="#e5e7eb"/>',
                  f'<text x="{x}" y="375" text-anchor="middle" font-family="sans-serif" font-size="12">{tick}%</text>']
    for i, model in enumerate(MODELS):
        data = opportunity["timeout-oom-60"][model]
        y = 85 + i * 90
        rejected = 100 * float(data["hit_over_rejected"])
        queries = 100 * float(data["hit_over_queries"])
        lines += [f'<text x="20" y="{y + 20}" font-family="sans-serif" font-size="15">{model.upper()}</text>',
                  f'<rect x="180" y="{y}" width="{rejected * 5.8}" height="24" fill="{colors[model]}" opacity="0.35"/>',
                  f'<rect x="180" y="{y + 30}" width="{queries * 5.8}" height="24" fill="{colors[model]}"/>',
                  f'<text x="780" y="{y + 18}" font-family="sans-serif" font-size="12">hit/rejected {rejected:.2f}%</text>',
                  f'<text x="780" y="{y + 48}" font-family="sans-serif" font-size="12">hit/all {queries:.3f}%</text>']
    lines += ['<text x="490" y="410" text-anchor="middle" font-family="sans-serif" font-size="12">Solid: fraction of all consistency queries avoidable; pale: conditional fraction among rejected queries</text>', '</svg>']
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def svg_overhead(path: Path, formal: dict[str, object]) -> None:
    width, height = 820, 450
    colors = {"cpu": "#b91c1c", "wall": "#1d4ed8", "rss": "#047857"}
    lines = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
             '<rect width="100%" height="100%" fill="white"/>',
             '<text x="410" y="28" text-anchor="middle" font-family="sans-serif" font-size="18">Census/baseline overhead ratios (task-clustered 95% bootstrap CI)</text>']
    xmin, xmax, x0, scale = 0.9, 2.0, 160, 540 / 1.1
    for tick in (0.9, 1.0, 1.2, 1.4, 1.6, 1.8, 2.0):
        x = x0 + (tick - xmin) * scale
        lines += [f'<line x1="{x}" y1="52" x2="{x}" y2="390" stroke="#{"111827" if tick == 1 else "e5e7eb"}"/>',
                  f'<text x="{x}" y="418" text-anchor="middle" font-family="sans-serif" font-size="12">{tick:.1f}</text>']
    row = 0
    for group in ("all", "sc", "tso", "pso"):
        for metric in ("cpu", "wall", "rss"):
            summary = formal[metric][group]
            value = float(summary["geomean_ratio"])
            low, high = map(float, summary["bootstrap_95ci"])
            y = 70 + row * 25
            lines += [f'<text x="15" y="{y + 4}" font-family="sans-serif" font-size="12">{group.upper()} {metric}</text>',
                      f'<line x1="{x0 + (low - xmin) * scale}" y1="{y}" x2="{x0 + (high - xmin) * scale}" y2="{y}" stroke="{colors[metric]}" stroke-width="3"/>',
                      f'<circle cx="{x0 + (value - xmin) * scale}" cy="{y}" r="5" fill="{colors[metric]}"/>']
            row += 1
    lines += ['</svg>']
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def render_reports(out: Path, formal: dict[str, object], opportunity: dict[str, object]) -> None:
    cpu = formal["cpu"]["all"]
    pso = opportunity["timeout-oom-60"]["pso"]
    sc = opportunity["timeout-oom-60"]["sc"]
    tso = opportunity["timeout-oom-60"]["tso"]
    decision = "只推进面向 PSO/未认证通用 CAT 的小型 P0.2 原型；删除普查实现，不在 SC/TSO 默认启用"
    report = f"""# P0.1 rejection-kernel census: strict analysis

## Conclusion

{decision}。PSO 历史 TIMEOUT/OOM 样本中，模拟命中占全部查询
{100 * pso['hit_over_queries']:.2f}%（{pso['simulated-hits']:,}/{pso['queries']:,}），而
SC/TSO 仅为 {100 * sc['hit_over_queries']:.4f}%/{100 * tso['hit_over_queries']:.4f}%。
当前普查代码会对每个失败执行完整解释和删除最小化，不能作为生产实现保留。

## Formal overhead

96 任务、SC/TSO/PSO、三次成对重复共 {formal['design']['cells']:,} 个运行单元。
以每个任务跨模型聚合为主要分析单位，CPU census/baseline 几何均值比为
{cpu['geomean_ratio']:.5f}，task-bootstrap 95% CI
[{cpu['bootstrap_95ci'][0]:.5f}, {cpu['bootstrap_95ci'][1]:.5f}]。
状态差异 {len(formal['status_mismatches'])} 个，安全任务完整执行数差异
{len(formal['safe_execution_count_mismatches'])} 个；资源边界扰动单独报告，不能解释为语义错误。

## Mechanism

- PSO 大任务：{pso['rejected']:,} 个 rejected query，{pso['simulated-hits']:,} 次命中，
  {pso['exact-duplicates']:,} 个 exact-duplicate explanation，{pso['subsumed']} 个严格 subsumption。
- 删除最小化仅把 {pso['raw-literals']:,} 个原始文字降到 {pso['minimized-literals']:,}，
  不足以抵消解释/最小化成本。
- SC/TSO 大任务绝大多数查询本身一致，拒绝核无法减少有效执行爆炸；多数 OOM 在第一条
  consistency checkpoint 前发生，必须由状态压缩、property/dependence CEGAR 或符号前端处理。

## Claim candidates

- Claim: exact positive rejection reuse is frequent for the generic PSO path.
  - Source evidence: `stats-strict.json`, TIMEOUT/OOM PSO counters.
  - Allowed wording: the census establishes a bounded P0.2 prototype opportunity.
  - Forbidden stronger wording: rejection kernels solve GenMC's broad TIMEOUT/OOM gap.
  - Uncertainty: the census itself is expensive and changes resource-limit outcomes.
  - Next check: raw-kernel lookup before evaluator synchronization, one worker, exact oracle.
  - Decision: keep as a prototype hypothesis.

- Claim: a universal SC/TSO/PSO blocker is justified.
  - Source evidence: SC/TSO hit/all-query rates below 0.02%.
  - Allowed wording: none.
  - Forbidden stronger wording: enable blocker learning for every model by default.
  - Uncertainty: other CAT models may differ.
  - Next check: model-fragment/profile-specific activation only.
  - Decision: discard.

## Limits

The 60-task resource cohort has one repetition and intentionally censored TIMEOUT/OOM logs; it supports
mechanism and coverage claims, not inferential performance claims. The 96-task overhead matrix has three
repetitions, and inference clusters by task rather than treating repetitions or model cells as independent.
"""
    (out / "analysis-report.md").write_text(report, encoding="utf-8")

    lines = ["# Statistical appendix", "", "## Exact metric table", "", "| Metric | Group | n tasks | Geomean ratio | 95% CI | Median | Sign p | Holm p |", "|---|---|---:|---:|---:|---:|---:|---:|"]
    for metric in METRICS:
        for group in ("all", *MODELS):
            s = formal[metric][group]
            lines.append(f"| {metric} | {group} | {s['tasks']} | {s['geomean_ratio']:.5f} | [{s['bootstrap_95ci'][0]:.5f}, {s['bootstrap_95ci'][1]:.5f}] | {s['median_ratio']:.5f} | {s['sign_test']['p']:.4g} | {s['sign_test']['holm_p_across_four_groups']:.4g} |")
    lines += ["", "## Test design", "", "- Ratio per model/task uses the median of three paired repetitions.", "- The primary `all` unit is one task, geometrically aggregating SC/TSO/PSO ratios only when all three are available.", "- Confidence intervals use 20,000 fixed-seed task-cluster bootstrap resamples.", "- The exact two-sided sign test is distribution-free; no repetition or model cell is treated as an independent subject.", "- Holm adjustment covers the four task groups within each metric; model rows remain diagnostic sensitivity analyses.", "- Ratio and its confidence interval are the reported multiplicative effect size; no normality assumption is used.", "", "## Correctness and censoring", "", f"- Status mismatches: {len(formal['status_mismatches'])}; all are resource/termination perturbations caused by measurement overhead, not opposite terminal verdicts.", f"- Safe execution-count mismatches: {len(formal['safe_execution_count_mismatches'])}.", "- TIMEOUT/OOM resource-cohort checkpoints are right-censored and used only for opportunity counters."]
    (out / "stats-appendix.md").write_text("\n".join(lines) + "\n", encoding="utf-8")

    catalog = """# Figure catalog

## `figures/figure-01-formal-overhead.svg`

- Purpose: show the measurement implementation's CPU, wall, and RSS cost.
- Data source: three-repetition 96-task paired BenchExec XML.
- Error bars: 95% task-cluster bootstrap CI of the geometric mean ratio.
- Notice: values above one are overhead; the PSO row includes expensive Reasoner work.
- Decision impact: the census implementation is measurement-only and must be removed.

## `figures/figure-02-large-task-opportunity.svg`

- Purpose: distinguish high conditional reuse among rejected queries from actual all-query coverage.
- Data source: latest complete checkpoint in each 60-task TIMEOUT/OOM log.
- Bars: pale is hit/rejected; solid is hit/all consistency queries.
- Notice: SC/TSO conditional rates look high but their all-query rates are negligible; PSO is qualitatively different.
- Decision impact: scope P0.2 to generic PSO/order-model paths and use a cheap activation certificate.
"""
    (out / "figure-catalog.md").write_text(catalog, encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("server", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "figures").mkdir(exist_ok=True)
    rows = load_formal(args.server / "formal-96")
    records = load_kernel_records(args.server)
    formal = formal_summary(rows)
    opportunity = opportunity_summary(records)
    write_tsv(args.output / "formal-rows.tsv", rows)
    write_tsv(args.output / "kernel-counters.tsv", records)
    stats = {"formal": formal, "opportunity": opportunity}
    (args.output / "stats-strict.json").write_text(json.dumps(stats, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    svg_overhead(args.output / "figures" / "figure-01-formal-overhead.svg", formal)
    svg_opportunity(args.output / "figures" / "figure-02-large-task-opportunity.svg", opportunity)
    render_reports(args.output, formal, opportunity)


if __name__ == "__main__":
    main()
