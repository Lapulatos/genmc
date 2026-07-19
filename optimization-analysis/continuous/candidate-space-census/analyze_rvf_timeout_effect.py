#!/usr/bin/env python3
"""Analyze whether optimistic RVF opportunity is concentrated in hard tasks."""

from __future__ import annotations

import argparse
import csv
import html
import math
import random
import statistics
from collections import Counter
from pathlib import Path


MODELS = ("sc", "tso", "pso")
COMPLETE = {"true", "false(unreach-call)"}
OUTCOME_ORDER = ("complete", "timeout", "oom")
COLORS = {"complete": "#0072B2", "timeout": "#D55E00", "oom": "#CC79A7"}


def outcome(status: str) -> str | None:
    if status in COMPLETE:
        return "complete"
    if status == "TIMEOUT":
        return "timeout"
    if status == "OUT OF MEMORY":
        return "oom"
    return None


def percentile(values: list[float], fraction: float) -> float:
    ordered = sorted(values)
    if not ordered:
        return math.nan
    position = (len(ordered) - 1) * fraction
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    weight = position - lower
    return ordered[lower] * (1 - weight) + ordered[upper] * weight


def describe(rows: list[dict[str, object]]) -> dict[str, float | int]:
    values = [float(row["fraction"]) for row in rows]
    offered = sum(int(row["offered"]) for row in rows)
    same = sum(int(row["same"]) for row in rows)
    return {
        "n": len(values),
        "median": statistics.median(values) if values else math.nan,
        "q1": percentile(values, 0.25),
        "q3": percentile(values, 0.75),
        "mean": statistics.fmean(values) if values else math.nan,
        "weighted": same / offered if offered else math.nan,
    }


def mann_whitney(high: list[float], low: list[float]) -> dict[str, float]:
    """Two-sided normal approximation with tie correction and rank-biserial effect."""
    tagged = [(value, 1) for value in high] + [(value, 0) for value in low]
    tagged.sort(key=lambda pair: pair[0])
    rank_sum = 0.0
    ties: list[int] = []
    begin = 0
    while begin < len(tagged):
        end = begin + 1
        while end < len(tagged) and tagged[end][0] == tagged[begin][0]:
            end += 1
        average_rank = (begin + 1 + end) / 2
        rank_sum += average_rank * sum(group for _, group in tagged[begin:end])
        ties.append(end - begin)
        begin = end
    n1, n0 = len(high), len(low)
    u = rank_sum - n1 * (n1 + 1) / 2
    expected = n1 * n0 / 2
    total = n1 + n0
    tie_term = sum(size**3 - size for size in ties)
    variance = n1 * n0 / 12 * (
        total + 1 - tie_term / (total * (total - 1))
    )
    continuity = 0.5 if u > expected else (-0.5 if u < expected else 0.0)
    z = (u - expected - continuity) / math.sqrt(variance) if variance else 0.0
    p = math.erfc(abs(z) / math.sqrt(2))
    effect = 2 * u / (n1 * n0) - 1
    return {"u": u, "z": z, "p": p, "effect": effect}


def bootstrap_median_difference(
    high: list[float], low: list[float], seed: int, repetitions: int = 10000
) -> tuple[float, float]:
    rng = random.Random(seed)
    differences = []
    for _ in range(repetitions):
        high_sample = [high[rng.randrange(len(high))] for _ in high]
        low_sample = [low[rng.randrange(len(low))] for _ in low]
        differences.append(statistics.median(high_sample) - statistics.median(low_sample))
    return percentile(differences, 0.025), percentile(differences, 0.975)


def holm_adjust(raw: dict[str, float]) -> dict[str, float]:
    ordered = sorted(raw, key=raw.get)
    adjusted: dict[str, float] = {}
    running = 0.0
    count = len(ordered)
    for index, key in enumerate(ordered):
        running = max(running, (count - index) * raw[key])
        adjusted[key] = min(1.0, running)
    return adjusted


def esc(value: object) -> str:
    return html.escape(str(value))


def svg_header(width: int, height: int) -> list[str]:
    return [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
        f'viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="white"/>',
        '<style>text{font-family:Arial,sans-serif;fill:#222}.axis{stroke:#444;stroke-width:1}'
        '.grid{stroke:#ddd;stroke-width:1}.line{fill:none;stroke-width:2}</style>',
    ]


def make_ecdf(groups: dict[str, dict[str, list[dict[str, object]]]], path: Path) -> None:
    width, height = 1050, 360
    left, top, plot_height, panel_width = 65, 35, 265, 300
    gap = 40
    lines = svg_header(width, height)
    for panel, model in enumerate(MODELS):
        x0 = left + panel * (panel_width + gap)
        y0 = top
        for tick in range(6):
            x = x0 + panel_width * tick / 5
            y = y0 + plot_height - plot_height * tick / 5
            lines += [
                f'<line class="grid" x1="{x:.1f}" y1="{y0}" x2="{x:.1f}" '
                f'y2="{y0 + plot_height}"/>',
                f'<line class="grid" x1="{x0}" y1="{y:.1f}" '
                f'x2="{x0 + panel_width}" y2="{y:.1f}"/>',
                f'<text x="{x:.1f}" y="{y0 + plot_height + 19}" font-size="10" '
                f'text-anchor="middle">{tick / 5:.1f}</text>',
            ]
            if panel == 0:
                lines.append(
                    f'<text x="{x0 - 9}" y="{y + 4:.1f}" font-size="10" '
                    f'text-anchor="end">{tick / 5:.1f}</text>'
                )
        lines += [
            f'<line class="axis" x1="{x0}" y1="{y0 + plot_height}" '
            f'x2="{x0 + panel_width}" y2="{y0 + plot_height}"/>',
            f'<line class="axis" x1="{x0}" y1="{y0}" x2="{x0}" '
            f'y2="{y0 + plot_height}"/>',
            f'<text x="{x0 + panel_width / 2}" y="22" font-size="14" '
            f'font-weight="bold" text-anchor="middle">{model.upper()}</text>',
        ]
        for label in ("complete", "timeout"):
            values = sorted(float(row["fraction"]) for row in groups[model][label])
            if not values:
                continue
            points = [(x0, y0 + plot_height)]
            for index, value in enumerate(values, start=1):
                points.append(
                    (x0 + panel_width * min(1.0, value),
                     y0 + plot_height * (1 - index / len(values)))
                )
            point_text = " ".join(f"{x:.2f},{y:.2f}" for x, y in points)
            lines.append(
                f'<polyline class="line" stroke="{COLORS[label]}" points="{point_text}"/>'
            )
    lines += [
        '<text x="525" y="350" font-size="12" text-anchor="middle">'
        'Per-task optimistic same-value RF fraction</text>',
        '<text x="15" y="170" font-size="12" text-anchor="middle" '
        'transform="rotate(-90 15 170)">Empirical cumulative fraction</text>',
        f'<line x1="820" y1="320" x2="845" y2="320" stroke="{COLORS["complete"]}" '
        'stroke-width="2"/><text x="850" y="324" font-size="11">completed verdict</text>',
        f'<line x1="820" y1="339" x2="845" y2="339" stroke="{COLORS["timeout"]}" '
        'stroke-width="2"/><text x="850" y="343" font-size="11">TIMEOUT</text>',
        '</svg>',
    ]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def make_scatter(groups: dict[str, dict[str, list[dict[str, object]]]], path: Path) -> None:
    width, height = 1050, 360
    left, top, plot_height, panel_width = 65, 35, 265, 300
    gap = 40
    lines = svg_header(width, height)
    for panel, model in enumerate(MODELS):
        x0 = left + panel * (panel_width + gap)
        y0 = top
        for tick in range(8):
            x = x0 + panel_width * tick / 7
            lines.append(
                f'<line class="grid" x1="{x:.1f}" y1="{y0}" x2="{x:.1f}" '
                f'y2="{y0 + plot_height}"/>'
            )
            lines.append(
                f'<text x="{x:.1f}" y="{y0 + plot_height + 19}" font-size="10" '
                f'text-anchor="middle">10^{tick}</text>'
            )
        for tick in range(6):
            y = y0 + plot_height - plot_height * tick / 5
            lines.append(
                f'<line class="grid" x1="{x0}" y1="{y:.1f}" '
                f'x2="{x0 + panel_width}" y2="{y:.1f}"/>'
            )
            if panel == 0:
                lines.append(
                    f'<text x="{x0 - 9}" y="{y + 4:.1f}" font-size="10" '
                    f'text-anchor="end">{tick / 5:.1f}</text>'
                )
        lines += [
            f'<line class="axis" x1="{x0}" y1="{y0 + plot_height}" '
            f'x2="{x0 + panel_width}" y2="{y0 + plot_height}"/>',
            f'<line class="axis" x1="{x0}" y1="{y0}" x2="{x0}" '
            f'y2="{y0 + plot_height}"/>',
            f'<text x="{x0 + panel_width / 2}" y="22" font-size="14" '
            f'font-weight="bold" text-anchor="middle">{model.upper()}</text>',
        ]
        for label in OUTCOME_ORDER:
            for row in groups[model][label]:
                x = x0 + panel_width * min(7.0, math.log10(max(1, int(row["offered"])))) / 7
                y = y0 + plot_height * (1 - float(row["fraction"]))
                lines.append(
                    f'<circle cx="{x:.2f}" cy="{y:.2f}" r="2.2" '
                    f'fill="{COLORS[label]}" fill-opacity="0.48"/>'
                )
    lines += [
        '<text x="525" y="350" font-size="12" text-anchor="middle">RF candidates offered (log scale)</text>',
        '<text x="15" y="170" font-size="12" text-anchor="middle" '
        'transform="rotate(-90 15 170)">Optimistic same-value RF fraction</text>',
        '</svg>',
    ]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("rows", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    figures = args.output / "figures"
    figures.mkdir(exist_ok=True)

    groups = {
        model: {label: [] for label in OUTCOME_ORDER}
        for model in MODELS
    }
    status_counts: dict[str, Counter[str]] = {model: Counter() for model in MODELS}
    missing_counts: dict[str, Counter[str]] = {model: Counter() for model in MODELS}
    with args.rows.open(encoding="utf-8", newline="") as source:
        for raw in csv.DictReader(source):
            model = raw["model"]
            status = raw["status"]
            status_counts[model][status] += 1
            label = outcome(status)
            if label is None:
                continue
            if not raw.get("rf_offered"):
                missing_counts[model][label] += 1
                continue
            offered = int(raw["rf_offered"])
            same = int(raw.get("rf_same_value_candidate_upper_bound") or 0)
            groups[model][label].append({
                "task": raw["task"],
                "offered": offered,
                "same": same,
                "fraction": same / offered if offered else 0.0,
            })

    tests: dict[str, dict[str, float | tuple[float, float]]] = {}
    raw_p: dict[str, float] = {}
    for index, model in enumerate(MODELS):
        high = [float(row["fraction"]) for row in groups[model]["timeout"]]
        low = [float(row["fraction"]) for row in groups[model]["complete"]]
        test = mann_whitney(high, low)
        test["ci"] = bootstrap_median_difference(high, low, 20260717 + index)
        tests[model] = test
        raw_p[model] = float(test["p"])
    adjusted = holm_adjust(raw_p)

    make_ecdf(groups, figures / "figure-01-rvf-fraction-ecdf.svg")
    make_scatter(groups, figures / "figure-02-opportunity-vs-search-size.svg")

    table = [
        "| Model | Outcome | n with counters | Missing | Median [IQR] | Per-task mean | Aggregate weighted |",
        "|---|---:|---:|---:|---:|---:|---:|",
    ]
    for model in MODELS:
        for label in OUTCOME_ORDER:
            desc = describe(groups[model][label])
            table.append(
                f"| {model.upper()} | {label} | {desc['n']} | {missing_counts[model][label]} | "
                f"{desc['median']:.2%} [{desc['q1']:.2%}, {desc['q3']:.2%}] | "
                f"{desc['mean']:.2%} | {desc['weighted']:.2%} |"
            )

    report = [
        "# RVF opportunity and non-termination analysis",
        "",
        "## Analysis question",
        "",
        "Is the optimistic same-value RF opportunity concentrated in tasks that fail to finish within 60 seconds? The unit of analysis is one task within one memory model. Counter-bearing rows are included; a row with zero offered RF candidates has zero operational quotient opportunity.",
        "",
        "## Key findings",
        "",
    ]
    for model in MODELS:
        complete = describe(groups[model]["complete"])
        timeout = describe(groups[model]["timeout"])
        test = tests[model]
        ci = test["ci"]
        report.append(
            f"- {model.upper()}: the task-level median is {timeout['median']:.2%} for TIMEOUT versus "
            f"{complete['median']:.2%} for completed verdicts. Mann–Whitney U={test['u']:.1f}, "
            f"Holm-adjusted p={adjusted[model]:.3g}, rank-biserial effect={test['effect']:.3f}; "
            f"the bootstrap 95% CI for the median difference is [{ci[0]:.2%}, {ci[1]:.2%}]."
        )
    report += [
        "",
        "The unadjusted association is large and consistent across the three models, but it is partly a search-size effect: many completed tasks offer no RF choice at all. It supports measuring a quotient on the hard cohort; it does not show that an unusually high same-value fraction causes timeout.",
        "",
        "## Exact descriptive table",
        "",
        *table,
        "",
        "## Search-size sensitivity",
        "",
        "The table below repeats the task-level medians after excluding small searches. The 10,000-candidate row is the most useful common-support check; 100,000 leaves too few completed TSO/PSO tasks for a stable comparison.",
        "",
        "| Model | Minimum RF offered | Completed n | Completed median | TIMEOUT n | TIMEOUT median | Difference |",
        "|---|---:|---:|---:|---:|---:|---:|",
    ]
    for model in MODELS:
        for threshold in (1000, 10000, 100000):
            complete_rows = [
                row for row in groups[model]["complete"]
                if int(row["offered"]) >= threshold
            ]
            timeout_rows = [
                row for row in groups[model]["timeout"]
                if int(row["offered"]) >= threshold
            ]
            complete_desc = describe(complete_rows)
            timeout_desc = describe(timeout_rows)
            difference = float(timeout_desc["median"]) - float(complete_desc["median"])
            report.append(
                f"| {model.upper()} | {threshold:,} | {complete_desc['n']} | "
                f"{complete_desc['median']:.2%} | {timeout_desc['n']} | "
                f"{timeout_desc['median']:.2%} | {difference:+.2%} |"
            )
    report += [
        "",
        "## Claim candidates",
        "",
        "- Claim:",
        "  - Source evidence: all 2,175 BenchExec task/model rows; task-level non-parametric contrasts above.",
        "  - Allowed wording: Counter-bearing TIMEOUT tasks expose an aggregate 28–30% optimistic same-value RF opportunity and millions of concrete alternatives, so the hard cohort is a relevant target for an RVF experiment.",
        "  - Forbidden stronger wording: TIMEOUT tasks have a uniquely larger same-value fraction after controlling for search size, or RVF will eliminate roughly 30% of runtime/timeouts.",
        "  - Uncertainty: the unadjusted contrast is confounded by search size; same-value grouping is only an upper bound; full RVF classes and witness-generation cost are unmeasured.",
        "  - Next check: integrate the SC representative generator and compare actual offered/queued/popped candidates on the same tasks.",
        "  - Decision: weaken and keep.",
        "",
        "## Limits",
        "",
        "- Counter logging is missing for preprocessing/early-failure rows and for some TIMEOUT/OOM rows; missingness is reported per group and is not assumed random. Zero-offered counter rows are retained as zero opportunity.",
        "- Tasks recur across SC/TSO/PSO, so the three model contrasts are not independent replications. Holm correction controls only the three reported within-model tests.",
        "- The workload is the adapted fixed-seed SV-COMP set; conclusions do not establish official unbounded-input SV-COMP coverage.",
        "- The search-size thresholds are sensitivity analyses, not preregistered cutoffs; no size-adjusted causal effect is claimed.",
    ]
    (args.output / "analysis-report.md").write_text("\n".join(report) + "\n", encoding="utf-8")

    appendix = [
        "# Statistical appendix",
        "",
        "- Primary metric: `(rf_same_value_candidate_upper_bound / rf_offered)`; higher means a larger optimistic local opportunity.",
        "- Unit: task × memory-model row, analyzed separately by model.",
        "- Groups: completed reachability verdict versus 60-second TIMEOUT; OOM is descriptive only.",
        "- Distribution: bounded, zero-inflated and strongly skewed; therefore the primary test is two-sided Mann–Whitney with average ranks, tie-corrected normal approximation and continuity correction.",
        "- Effect: rank-biserial correlation, positive when TIMEOUT values are larger.",
        "- Uncertainty: 10,000 stratified bootstrap resamples of the task-level median difference, fixed seeds 20260717–20260719.",
        "- Multiple comparisons: Holm adjustment across SC, TSO and PSO.",
        "",
        "| Model | U | z | raw p | Holm p | Rank-biserial | Median-difference 95% CI |",
        "|---|---:|---:|---:|---:|---:|---:|",
    ]
    for model in MODELS:
        test = tests[model]
        ci = test["ci"]
        appendix.append(
            f"| {model.upper()} | {test['u']:.1f} | {test['z']:.3f} | {test['p']:.4g} | "
            f"{adjusted[model]:.4g} | {test['effect']:.3f} | [{ci[0]:.2%}, {ci[1]:.2%}] |"
        )
    appendix += ["", "## Status inventory", ""]
    for model in MODELS:
        appendix.append(
            f"- {model.upper()}: " + ", ".join(
                f"{esc(status)}={count}" for status, count in sorted(status_counts[model].items())
            )
        )
    (args.output / "stats-appendix.md").write_text("\n".join(appendix) + "\n", encoding="utf-8")

    catalog = [
        "# Figure catalog",
        "",
        "## Figure 1: `figures/figure-01-rvf-fraction-ecdf.svg`",
        "",
        "- Purpose: compare the full task-level opportunity distributions rather than only aggregate totals.",
        "- Data: counter-bearing completed and TIMEOUT rows; panels are SC, TSO and PSO.",
        "- Caption requirements: define the optimistic fraction, ECDF, outcome groups and per-panel sample sizes from the descriptive table.",
        "- Observation: TIMEOUT curves are shifted toward larger fractions, while many completed tasks have no RF opportunity.",
        "- Interpretation: the unadjusted difference motivates a hard-cohort experiment but is partly explained by search size.",
        "- Caveat: the plot does not control for RF search size and does not measure realizable RVF classes or runtime saved.",
        "",
        "## Figure 2: `figures/figure-02-opportunity-vs-search-size.svg`",
        "",
        "- Purpose: show whether the association is confined to tiny traces or persists at large RF candidate counts.",
        "- Data: completed, TIMEOUT and OOM rows with counters; x is logarithmic RF offered and y is the optimistic fraction.",
        "- Caption requirements: state log scale, point transparency and that each point is one task/model row.",
        "- Observation: large completed and TIMEOUT searches both cluster around roughly 28–30%; TIMEOUT rows dominate the largest-search region, while OOM counter coverage is sparse.",
        "- Interpretation: absolute removable-candidate opportunity grows in the timeout cohort, but fractional enrichment beyond search size is not established.",
        "- Caveat: overlapping points and censored 60-second runs prevent a causal slope estimate.",
    ]
    (args.output / "figure-catalog.md").write_text("\n".join(catalog) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
