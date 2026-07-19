#!/usr/bin/env python3
"""Relate CAAT graph diagnostics to formal paired CAT/CAAT performance."""

from __future__ import annotations

import argparse
from collections import defaultdict
import csv
import html
import math
from pathlib import Path
import random
import statistics


SOLVED = {"true", "false(unreach-call)"}
METRICS = (
    "max_stable_events",
    "max_inactive_events",
    "max_current_base_bytes",
    "max_history_base_bytes",
    "max_base_relation_pairs",
    "max_base_relation_density_ppm",
    "rebuilds",
    "profiled_queries",
)
COLORS = {"sc": "#0072B2", "tso": "#009E73", "pso": "#D55E00"}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("diagnostics", type=Path)
    parser.add_argument("--sc", type=Path, required=True)
    parser.add_argument("--tso", type=Path, required=True)
    parser.add_argument("--pso", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--bootstrap", type=int, default=10_000)
    return parser.parse_args()


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as source:
        return list(csv.DictReader(source, delimiter="\t"))


def write_tsv(path: Path, rows: list[dict[str, object]]) -> None:
    if not rows:
        raise SystemExit(f"refusing to write empty table {path}")
    with path.open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=list(rows[0]), delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def geomean(values: list[float]) -> float:
    return math.exp(statistics.fmean(math.log(value) for value in values))


def ranks(values: list[float]) -> list[float]:
    order = sorted(range(len(values)), key=values.__getitem__)
    result = [0.0] * len(values)
    index = 0
    while index < len(order):
        end = index + 1
        while end < len(order) and values[order[end]] == values[order[index]]:
            end += 1
        rank = (index + 1 + end) / 2
        for position in order[index:end]:
            result[position] = rank
        index = end
    return result


def pearson(left: list[float], right: list[float]) -> float:
    left_mean, right_mean = statistics.fmean(left), statistics.fmean(right)
    numerator = sum((x - left_mean) * (y - right_mean) for x, y in zip(left, right))
    denominator = math.sqrt(
        sum((x - left_mean) ** 2 for x in left)
        * sum((y - right_mean) ** 2 for y in right)
    )
    return numerator / denominator if denominator else math.nan


def spearman(left: list[float], right: list[float]) -> float:
    return pearson(ranks(left), ranks(right))


def quantile(values: list[float], probability: float) -> float:
    ordered = sorted(values)
    position = (len(ordered) - 1) * probability
    lower, upper = math.floor(position), math.ceil(position)
    if lower == upper:
        return ordered[lower]
    return ordered[lower] * (upper - position) + ordered[upper] * (position - lower)


def bootstrap_spearman(
    left: list[float], right: list[float], samples: int, seed: int
) -> tuple[float, float]:
    rng = random.Random(seed)
    estimates: list[float] = []
    for _ in range(samples):
        indices = [rng.randrange(len(left)) for _ in left]
        estimate = spearman([left[i] for i in indices], [right[i] for i in indices])
        if math.isfinite(estimate):
            estimates.append(estimate)
    return quantile(estimates, 0.025), quantile(estimates, 0.975)


def comparable_primary(path: Path, model: str) -> dict[str, dict[str, float]]:
    grouped: dict[tuple[str, str], list[dict[str, str]]] = defaultdict(list)
    for row in read_tsv(path):
        grouped[(row["backend"], row["task_yaml"])].append(row)
    result: dict[str, dict[str, float]] = {}
    cat, caat = f"cat-{model}", f"caat-{model}"
    tasks = {task for backend, task in grouped if backend == cat}
    for task in tasks:
        cat_rows, caat_rows = grouped[(cat, task)], grouped[(caat, task)]
        if len(cat_rows) != 5 or len(caat_rows) != 5:
            continue
        if not all(row["category"] == "correct" and row["status"] in SOLVED for row in cat_rows + caat_rows):
            continue
        if len({row["status"] for row in cat_rows + caat_rows}) != 1:
            continue
        cat_wall = statistics.median(float(row["walltime_seconds"]) for row in cat_rows)
        caat_wall = statistics.median(float(row["walltime_seconds"]) for row in caat_rows)
        cat_memory = statistics.median(float(row["memory_bytes"]) for row in cat_rows)
        caat_memory = statistics.median(float(row["memory_bytes"]) for row in caat_rows)
        result[task] = {
            "cat_wall_seconds": cat_wall,
            "caat_wall_seconds": caat_wall,
            "wall_ratio_caat_over_cat": caat_wall / cat_wall,
            "cat_memory_bytes": cat_memory,
            "caat_memory_bytes": caat_memory,
            "memory_ratio_caat_over_cat": caat_memory / cat_memory,
        }
    return result


def scatter_svg(path: Path, rows: list[dict[str, object]], metric: str, title: str) -> None:
    width, height = 760, 450
    left, right, top, bottom = 78, 30, 45, 66
    plot_w, plot_h = width - left - right, height - top - bottom
    points = [row for row in rows if float(row[metric]) >= 0]
    xs = [math.log10(1 + float(row[metric])) for row in points]
    ys = [math.log2(float(row["wall_ratio_caat_over_cat"])) for row in points]
    xmin, xmax = min(xs), max(xs)
    ymin, ymax = min(min(ys), -0.25), max(max(ys), 0.25)
    if xmin == xmax:
        xmax += 1
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="white"/>',
        f'<text x="{width/2}" y="25" text-anchor="middle" font-size="16" font-weight="bold">{html.escape(title)}</text>',
    ]
    zero_y = top + plot_h * (ymax / (ymax - ymin))
    parts.append(f'<line x1="{left}" y1="{zero_y:.1f}" x2="{left+plot_w}" y2="{zero_y:.1f}" stroke="#555" stroke-dasharray="4 4"/>')
    for row, x_value, y_value in zip(points, xs, ys):
        x = left + plot_w * (x_value - xmin) / (xmax - xmin)
        y = top + plot_h * (ymax - y_value) / (ymax - ymin)
        model = str(row["model"])
        parts.append(
            f'<circle cx="{x:.1f}" cy="{y:.1f}" r="3.3" fill="{COLORS[model]}" fill-opacity="0.68">'
            f'<title>{html.escape(str(row["task_yaml"]))}: {metric}={row[metric]}, ratio={float(row["wall_ratio_caat_over_cat"]):.4g}</title></circle>'
        )
    for index, model in enumerate(("sc", "tso", "pso")):
        x = left + 12 + index * 72
        parts.append(f'<circle cx="{x}" cy="{top+12}" r="4" fill="{COLORS[model]}"/><text x="{x+8}" y="{top+16}" font-size="11">{model.upper()}</text>')
    parts += [
        f'<line x1="{left}" y1="{top+plot_h}" x2="{left+plot_w}" y2="{top+plot_h}" stroke="black"/>',
        f'<line x1="{left}" y1="{top}" x2="{left}" y2="{top+plot_h}" stroke="black"/>',
        f'<text x="{left+plot_w/2}" y="{height-18}" text-anchor="middle" font-size="13">log10(1 + {html.escape(metric)})</text>',
        f'<text x="18" y="{top+plot_h/2}" transform="rotate(-90 18 {top+plot_h/2})" text-anchor="middle" font-size="13">log2(CAAT / CAT wall time)</text>',
        '</svg>',
    ]
    path.write_text("\n".join(parts) + "\n", encoding="utf-8")


def main() -> int:
    config = parse_args()
    if config.bootstrap < 1:
        raise SystemExit("--bootstrap must be positive")
    config.output.mkdir(parents=True, exist_ok=True)
    diagnostic_rows = read_tsv(config.diagnostics)
    diagnostics = {
        (row["backend"].removeprefix("caat-"), row["task_yaml"]): row
        for row in diagnostic_rows
        if row["backend"].startswith("caat-")
        and row["category"] == "correct"
        and all(row[metric] for metric in METRICS)
    }
    primary_paths = {"sc": config.sc, "tso": config.tso, "pso": config.pso}
    task_rows: list[dict[str, object]] = []
    for model, primary_path in primary_paths.items():
        for task, primary in comparable_primary(primary_path, model).items():
            diagnostic = diagnostics.get((model, task))
            if diagnostic is None:
                continue
            task_rows.append(
                {
                    "model": model,
                    "task_yaml": task,
                    **primary,
                    **{metric: int(diagnostic[metric]) for metric in METRICS},
                }
            )
    write_tsv(config.output / "scale-task-summary.tsv", task_rows)

    correlations: list[dict[str, object]] = []
    for model in ("sc", "tso", "pso"):
        model_rows = [row for row in task_rows if row["model"] == model]
        for metric_index, metric in enumerate(METRICS):
            left = [float(row[metric]) for row in model_rows]
            right = [math.log(float(row["wall_ratio_caat_over_cat"])) for row in model_rows]
            rho = spearman(left, right)
            low, high = bootstrap_spearman(
                left, right, config.bootstrap, 20260715 + metric_index
            )
            correlations.append(
                {
                    "model": model,
                    "metric": metric,
                    "tasks": len(model_rows),
                    "spearman_rho": f"{rho:.6g}",
                    "bootstrap_95_ci_low": f"{low:.6g}",
                    "bootstrap_95_ci_high": f"{high:.6g}",
                }
            )
    write_tsv(config.output / "correlation-summary.tsv", correlations)

    bins: list[dict[str, object]] = []
    for model in ("sc", "tso", "pso"):
        model_rows = [row for row in task_rows if row["model"] == model]
        for metric in ("max_stable_events", "max_base_relation_density_ppm"):
            cuts = [quantile([float(row[metric]) for row in model_rows], q) for q in (0.25, 0.5, 0.75)]
            for index in range(4):
                selected = [
                    row for row in model_rows
                    if sum(float(row[metric]) > cut for cut in cuts) == index
                ]
                if not selected:
                    continue
                ratios = [float(row["wall_ratio_caat_over_cat"]) for row in selected]
                memory = [float(row["memory_ratio_caat_over_cat"]) for row in selected]
                bins.append(
                    {
                        "model": model,
                        "metric": metric,
                        "quartile_bin": f"Q{index+1}",
                        "tasks": len(selected),
                        "metric_min": min(float(row[metric]) for row in selected),
                        "metric_max": max(float(row[metric]) for row in selected),
                        "wall_ratio_caat_over_cat_geomean": f"{geomean(ratios):.6g}",
                        "memory_ratio_caat_over_cat_geomean": f"{geomean(memory):.6g}",
                    }
                )
    write_tsv(config.output / "metric-bin-summary.tsv", bins)
    scatter_svg(
        config.output / "stable-events-vs-wall-ratio.svg",
        task_rows,
        "max_stable_events",
        "CAAT/CAT wall-time ratio versus stable event domain",
    )
    scatter_svg(
        config.output / "relation-density-vs-wall-ratio.svg",
        task_rows,
        "max_base_relation_density_ppm",
        "CAAT/CAT wall-time ratio versus base-relation density",
    )

    counts = {model: sum(row["model"] == model for row in task_rows) for model in ("sc", "tso", "pso")}
    report = [
        "# CAT/CAAT scale-diagnostic analysis",
        "",
        "- Timing ratios come only from the formal five-repetition paired runs.",
        "- Graph metrics come from a separate one-repetition `--cat-stats` run and are not used as performance timings.",
        "- CAT emits no incremental graph counters, so these metrics describe CAAT's internal workload; associations are not causal attributions.",
        f"- Comparable tasks with complete diagnostics: SC {counts['sc']}, TSO {counts['tso']}, PSO {counts['pso']}.",
        "- Spearman correlations use task as the unit and deterministic task bootstrap confidence intervals.",
        "",
        "## Interpretation boundary",
        "",
        "A confidence interval crossing zero does not support a monotone association. Even when an interval excludes zero, the result only associates CAAT graph structure with the paired CAAT/CAT ratio; it does not identify which implementation operation caused the difference.",
    ]
    (config.output / "analysis-report.md").write_text("\n".join(report) + "\n", encoding="utf-8")
    (config.output / "figure-catalog.md").write_text(
        "# Figure catalog\n\n"
        "- `stable-events-vs-wall-ratio.svg`: tests whether CAAT/CAT slowdown changes with stable event-domain size; the horizontal dashed line is equal time.\n"
        "- `relation-density-vs-wall-ratio.svg`: tests whether CAAT/CAT slowdown changes with base-relation density; the horizontal dashed line is equal time.\n"
        "- Each point is one task that all five formal repetitions solved in both methods and whose separate diagnostic run completed.\n",
        encoding="utf-8",
    )
    print(f"analyzed {len(task_rows)} task-model cells")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
