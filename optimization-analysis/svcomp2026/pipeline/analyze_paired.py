#!/usr/bin/env python3
"""Analyze repeated paired BenchExec results without third-party packages."""

from __future__ import annotations

import argparse
from collections import Counter, defaultdict
import csv
import html
import math
from pathlib import Path
import random
import statistics


SOLVED = {"true", "false(unreach-call)"}
COLORS = ["#0072B2", "#E69F00", "#009E73", "#CC79A7", "#D55E00"]


def is_timeout(status: str) -> bool:
    return status.startswith("TIMEOUT")


def is_oom(status: str) -> bool:
    return status.startswith("OUT OF MEMORY")


def args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--baseline", required=True)
    parser.add_argument("--manifest", type=Path)
    parser.add_argument("--bootstrap", type=int, default=10_000)
    parser.add_argument(
        "--contrast",
        action="append",
        default=[],
        metavar="NUMERATOR/DENOMINATOR",
        help="additional planned paired contrast",
    )
    return parser.parse_args()


def quantile(values: list[float], probability: float) -> float:
    ordered = sorted(values)
    if not ordered:
        return math.nan
    position = (len(ordered) - 1) * probability
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    return ordered[lower] * (upper - position) + ordered[upper] * (position - lower)


def geometric_mean(values: list[float]) -> float:
    return math.exp(statistics.fmean(math.log(value) for value in values))


def bootstrap_geomean(values: list[float], samples: int, seed: int = 20260714):
    rng = random.Random(seed)
    estimates = []
    for _ in range(samples):
        draw = [values[rng.randrange(len(values))] for _ in values]
        estimates.append(geometric_mean(draw))
    return quantile(estimates, 0.025), quantile(estimates, 0.975)


def exact_sign_p(wins: int, losses: int) -> float:
    n = wins + losses
    if n == 0:
        return 1.0
    tail = sum(math.comb(n, k) for k in range(0, min(wins, losses) + 1)) / 2**n
    return min(1.0, 2 * tail)


def write_tsv(
    path: Path,
    rows: list[dict[str, object]],
    fieldnames: list[str] | None = None,
) -> None:
    if not rows:
        if fieldnames:
            with path.open("w", encoding="utf-8", newline="") as output:
                csv.DictWriter(output, fieldnames=fieldnames, delimiter="\t").writeheader()
        return
    with path.open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(
            output, fieldnames=fieldnames or list(rows[0]), delimiter="\t"
        )
        writer.writeheader()
        writer.writerows(rows)


def svg_profile(path: Path, costs: dict[str, dict[str, float]], tasks: list[str]) -> None:
    width, height = 760, 440
    left, right, top, bottom = 80, 24, 24, 62
    plot_w, plot_h = width - left - right, height - top - bottom
    taus = [10 ** (i * math.log10(20) / 100) for i in range(101)]
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
             '<rect width="100%" height="100%" fill="white"/>']
    for i in range(6):
        y = top + plot_h * (1 - i / 5)
        parts.append(f'<line x1="{left}" y1="{y:.1f}" x2="{left+plot_w}" y2="{y:.1f}" stroke="#ddd"/>')
        parts.append(f'<text x="{left-10}" y="{y+4:.1f}" text-anchor="end" font-size="11">{i/5:.1f}</text>')
    methods = sorted(costs)
    best = {task: min((costs[m].get(task, math.inf) for m in methods), default=math.inf) for task in tasks}
    for index, method in enumerate(methods):
        points = []
        for tau in taus:
            fraction = sum(
                math.isfinite(best[task])
                and math.isfinite(costs[method].get(task, math.inf))
                and costs[method][task] <= tau * best[task]
                for task in tasks
            ) / len(tasks)
            x = left + plot_w * math.log10(tau) / math.log10(20)
            y = top + plot_h * (1 - fraction)
            points.append(f"{x:.1f},{y:.1f}")
        parts.append(f'<polyline points="{" ".join(points)}" fill="none" stroke="{COLORS[index % len(COLORS)]}" stroke-width="2"/>')
        parts.append(f'<line x1="{left+20}" y1="{top+18+index*20}" x2="{left+48}" y2="{top+18+index*20}" stroke="{COLORS[index % len(COLORS)]}" stroke-width="2"/>')
        parts.append(f'<text x="{left+55}" y="{top+22+index*20}" font-size="11">{method}</text>')
    parts += [f'<line x1="{left}" y1="{top+plot_h}" x2="{left+plot_w}" y2="{top+plot_h}" stroke="black"/>',
              f'<line x1="{left}" y1="{top}" x2="{left}" y2="{top+plot_h}" stroke="black"/>',
              f'<text x="{left+plot_w/2}" y="{height-18}" text-anchor="middle" font-size="13">Performance ratio τ (log scale, failures = ∞)</text>',
              f'<text x="18" y="{top+plot_h/2}" transform="rotate(-90 18 {top+plot_h/2})" text-anchor="middle" font-size="13">Fraction of tasks</text>',
              '</svg>']
    path.write_text("\n".join(parts) + "\n", encoding="utf-8")


def svg_coverage(path: Path, rows: list[dict[str, object]]) -> None:
    width = 820
    left, right, top, row_height = 150, 32, 70, 54
    plot_width = width - left - right
    height = top + row_height * len(rows) + 55
    categories = [
        ("solved_runs", "solved", "#009E73"),
        ("timeout_runs", "timeout", "#E69F00"),
        ("oom_runs", "OOM", "#D55E00"),
        ("other_failures", "other", "#777777"),
    ]
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="white"/>',
        '<text x="16" y="28" font-size="16" font-weight="bold">Run coverage and failure modes</text>',
    ]
    legend_x = left
    for _, label, color in categories:
        parts.append(f'<rect x="{legend_x}" y="38" width="13" height="13" fill="{color}"/>')
        parts.append(f'<text x="{legend_x + 18}" y="49" font-size="11">{label}</text>')
        legend_x += 92
    for index, row in enumerate(rows):
        y = top + index * row_height
        total = int(row["runs"])
        parts.append(
            f'<text x="{left - 10}" y="{y + 19}" text-anchor="end" font-size="12">'
            f'{html.escape(str(row["backend"]))}</text>'
        )
        x = left
        for key, label, color in categories:
            value = int(row[key])
            segment = plot_width * value / total if total else 0
            if segment:
                parts.append(
                    f'<rect x="{x:.2f}" y="{y}" width="{segment:.2f}" height="25" fill="{color}">'
                    f'<title>{label}: {value}/{total}</title></rect>'
                )
                if segment >= 34:
                    parts.append(
                        f'<text x="{x + segment / 2:.2f}" y="{y + 17}" text-anchor="middle" '
                        f'font-size="10" fill="white">{value}</text>'
                    )
            x += segment
        parts.append(f'<text x="{left + plot_width + 8}" y="{y + 19}" font-size="11">n={total}</text>')
    parts.append(
        f'<text x="{left + plot_width / 2}" y="{height - 18}" text-anchor="middle" font-size="12">'
        'Fraction of repeated runs</text>'
    )
    parts.append('</svg>')
    path.write_text("\n".join(parts) + "\n", encoding="utf-8")


def main() -> int:
    config = args()
    if config.bootstrap < 1:
        raise SystemExit("--bootstrap must be at least 1")
    with config.input.open(encoding="utf-8", newline="") as source:
        rows = list(csv.DictReader(source, delimiter="\t"))
    if not rows:
        raise SystemExit("input has no data rows")
    properties = {row["property"] for row in rows}
    if len(properties) != 1:
        raise SystemExit(f"analysis requires exactly one property, found {sorted(properties)}")
    analyzed_property = next(iter(properties))
    config.output.mkdir(parents=True, exist_ok=True)
    metadata: dict[str, dict[str, str]] = {}
    if config.manifest:
        with config.manifest.open(encoding="utf-8", newline="") as source:
            metadata = {
                row["task_yaml"]: row
                for row in csv.DictReader(source, delimiter="\t")
                if row["property"] == analyzed_property
            }
    grouped: dict[tuple[str, str], list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        grouped[(row["backend"], row["task_yaml"])].append(row)
    methods = sorted({row["backend"] for row in rows})
    tasks = sorted({row["task_yaml"] for row in rows})
    if config.baseline not in methods:
        raise SystemExit(f"unknown baseline {config.baseline!r}")
    repetition_ids = sorted({row["repetition"] for row in rows})
    repetitions = len(repetition_ids)
    duplicate_keys = [
        (method, task, repetition)
        for (method, task), cell in grouped.items()
        for repetition, count in Counter(row["repetition"] for row in cell).items()
        if count != 1
    ]
    if duplicate_keys:
        raise SystemExit(f"duplicate backend/task/repetition rows: {duplicate_keys[:3]}")
    incomplete_cells = [
        (method, task, sorted(row["repetition"] for row in grouped.get((method, task), [])))
        for method in methods
        for task in tasks
        if sorted(row["repetition"] for row in grouped.get((method, task), []))
        != repetition_ids
    ]
    if incomplete_cells:
        raise SystemExit(
            "incomplete repetition grid; first cells: " + repr(incomplete_cells[:3])
        )

    task_summary: list[dict[str, object]] = []
    costs: dict[str, dict[str, float]] = {method: {} for method in methods}
    cpu_costs: dict[str, dict[str, float]] = {method: {} for method in methods}
    memory_costs: dict[str, dict[str, float]] = {method: {} for method in methods}
    for method in methods:
        for task in tasks:
            cell = grouped.get((method, task), [])
            statuses = Counter(row["status"] for row in cell)
            complete = len(cell) == repetitions and all(row["status"] in SOLVED for row in cell)
            wall = [float(row["walltime_seconds"]) for row in cell if row["status"] in SOLVED]
            cpu = [float(row["cputime_seconds"]) for row in cell if row["status"] in SOLVED]
            memory = [float(row["memory_bytes"]) for row in cell if row["status"] in SOLVED]
            if complete:
                costs[method][task] = statistics.median(wall)
                cpu_costs[method][task] = statistics.median(cpu)
                memory_costs[method][task] = statistics.median(memory)
            task_summary.append({
                "backend": method, "task_yaml": task, "runs": len(cell),
                "family": metadata.get(task, {}).get("family", ""),
                "source_bytes": metadata.get(task, {}).get("source_bytes", ""),
                "source_lines": metadata.get(task, {}).get("source_lines", ""),
                "adapter_source_lines": metadata.get(task, {}).get(
                    "adapter_source_lines", ""
                ),
                "adapter_transitive_lines": metadata.get(task, {}).get(
                    "adapter_transitive_lines", ""
                ),
                "all_repetitions_solved": str(complete).lower(),
                "statuses": ";".join(f"{key}:{value}" for key, value in sorted(statuses.items())),
                "median_wall_seconds": f"{statistics.median(wall):.9g}" if wall else "",
                "median_cpu_seconds": f"{statistics.median(cpu):.9g}" if cpu else "",
                "median_memory_bytes": f"{statistics.median(memory):.9g}" if memory else "",
            })
    write_tsv(config.output / "paired-task-summary.tsv", task_summary)

    failure_matrix = []
    for task in tasks:
        method_statuses = {
            method: ";".join(
                f"{status}:{count}"
                for status, count in sorted(
                    Counter(row["status"] for row in grouped[(method, task)]).items()
                )
            )
            for method in methods
        }
        if any(
            any(row["status"] not in SOLVED for row in grouped[(method, task)])
            for method in methods
        ):
            failure_matrix.append(
                {
                    "task_yaml": task,
                    "expected_verdict": metadata.get(task, {}).get("expected_verdict", ""),
                    **method_statuses,
                }
            )
    write_tsv(
        config.output / "failure-matrix.tsv",
        failure_matrix,
        ["task_yaml", "expected_verdict", *methods],
    )

    # A performance comparison is meaningful only after all methods agree on
    # the property verdict for that task. Stable-but-different answers are a
    # correctness mismatch, not a timing observation.
    verdict_mismatches = []
    for task in tasks:
        verdicts = {
            method: next(iter({row["status"] for row in grouped[(method, task)]}))
            for method in methods
            if grouped.get((method, task))
            and all(row["status"] in SOLVED for row in grouped[(method, task)])
            and len({row["status"] for row in grouped[(method, task)]}) == 1
        }
        if len(set(verdicts.values())) > 1:
            verdict_mismatches.append(
                {"task_yaml": task, **{method: verdicts.get(method, "") for method in methods}}
            )
            for method in methods:
                costs[method].pop(task, None)
                cpu_costs[method].pop(task, None)
                memory_costs[method].pop(task, None)
    write_tsv(
        config.output / "verdict-mismatches.tsv",
        verdict_mismatches,
        ["task_yaml", *methods],
    )

    descriptive = []
    for method in methods:
        selected = [row for row in rows if row["backend"] == method]
        statuses = Counter(row["status"] for row in selected)
        solved = [row for row in selected if row["status"] in SOLVED]
        timeout_runs = sum(value for status, value in statuses.items() if is_timeout(status))
        oom_runs = sum(value for status, value in statuses.items() if is_oom(status))
        descriptive.append({
            "backend": method, "runs": len(selected), "tasks": len(tasks),
            "solved_runs": len(solved), "timeout_runs": timeout_runs,
            "oom_runs": oom_runs,
            "other_failures": len(selected) - len(solved) - timeout_runs - oom_runs,
            "median_wall_seconds_solved": f"{statistics.median(float(row['walltime_seconds']) for row in solved):.9g}" if solved else "",
            "median_memory_bytes_solved": f"{statistics.median(float(row['memory_bytes']) for row in solved):.9g}" if solved else "",
        })
    write_tsv(config.output / "descriptive-summary.tsv", descriptive)
    svg_coverage(config.output / "coverage-status.svg", descriptive)

    repetition_rows = []
    for method in methods:
        for repetition in sorted({row["repetition"] for row in rows if row["backend"] == method}):
            selected = [
                row for row in rows
                if row["backend"] == method and row["repetition"] == repetition
            ]
            statuses = Counter(row["status"] for row in selected)
            timeout_runs = sum(
                value for status, value in statuses.items() if is_timeout(status)
            )
            oom_runs = sum(value for status, value in statuses.items() if is_oom(status))
            memories = [float(row["memory_bytes"]) for row in selected if row["memory_bytes"]]
            repetition_rows.append({
                "backend": method, "repetition": repetition, "runs": len(selected),
                "solved": sum(statuses[status] for status in SOLVED),
                "timeouts": timeout_runs, "ooms": oom_runs,
                "total_cpu_seconds_observed": f"{sum(float(row['cputime_seconds']) for row in selected):.9g}",
                "total_wall_seconds_observed": f"{sum(float(row['walltime_seconds']) for row in selected):.9g}",
                "maximum_peak_rss_bytes": f"{max(memories):.9g}" if memories else "",
                "median_peak_rss_bytes": f"{statistics.median(memories):.9g}" if memories else "",
            })
    write_tsv(config.output / "per-repetition-summary.tsv", repetition_rows)

    pairwise = []
    raw_p = []
    comparisons = [(method, config.baseline) for method in methods if method != config.baseline]
    for contrast in config.contrast:
        try:
            numerator, denominator = contrast.split("/", 1)
        except ValueError as error:
            raise SystemExit(f"invalid --contrast {contrast!r}") from error
        if numerator not in methods or denominator not in methods:
            raise SystemExit(f"unknown backend in --contrast {contrast!r}")
        if (numerator, denominator) not in comparisons:
            comparisons.append((numerator, denominator))
    for method, denominator in comparisons:
        common = sorted(set(costs[denominator]) & set(costs[method]))
        ratios = [costs[method][task] / costs[denominator][task] for task in common]
        if not ratios:
            continue
        low, high = bootstrap_geomean(ratios, config.bootstrap)
        cpu_ratios = [cpu_costs[method][task] / cpu_costs[denominator][task] for task in common]
        memory_ratios = [memory_costs[method][task] / memory_costs[denominator][task] for task in common]
        cpu_low, cpu_high = bootstrap_geomean(cpu_ratios, config.bootstrap, seed=20260715)
        memory_low, memory_high = bootstrap_geomean(memory_ratios, config.bootstrap, seed=20260716)
        wins = sum(value < 1 for value in ratios)
        losses = sum(value > 1 for value in ratios)
        ties = len(ratios) - wins - losses
        p = exact_sign_p(wins, losses)
        contrast_name = f"{method}/{denominator}"
        raw_p.append((contrast_name, p))
        pairwise.append({
            "baseline": denominator, "backend": method, "paired_tasks": len(common),
            "wall_ratio_geomean": f"{geometric_mean(ratios):.6g}",
            "bootstrap_95ci_low": f"{low:.6g}", "bootstrap_95ci_high": f"{high:.6g}",
            "cpu_ratio_geomean": f"{geometric_mean(cpu_ratios):.6g}",
            "cpu_95ci_low": f"{cpu_low:.6g}", "cpu_95ci_high": f"{cpu_high:.6g}",
            "memory_ratio_geomean": f"{geometric_mean(memory_ratios):.6g}",
            "memory_95ci_low": f"{memory_low:.6g}", "memory_95ci_high": f"{memory_high:.6g}",
            "backend_faster": wins, "baseline_faster": losses, "ties": ties,
            "exact_sign_p": f"{p:.6g}", "holm_p": "",
        })
    ordered = sorted(raw_p, key=lambda item: item[1])
    adjusted = {}
    running = 0.0
    for rank, (method, p) in enumerate(ordered):
        running = max(running, min(1.0, p * (len(ordered) - rank)))
        adjusted[method] = running
    for row in pairwise:
        contrast_name = f"{row['backend']}/{row['baseline']}"
        row["holm_p"] = f"{adjusted[contrast_name]:.6g}"
    write_tsv(config.output / "pairwise-summary.tsv", pairwise)

    runtime_bins = [
        ("lt-0.1s", 0.0, 0.1),
        ("0.1-1s", 0.1, 1.0),
        ("1-10s", 1.0, 10.0),
        ("ge-10s", 10.0, math.inf),
    ]
    runtime_rows = []
    for label, lower, upper in runtime_bins:
        bin_tasks = [
            task for task, value in costs[config.baseline].items()
            if lower <= value < upper
        ]
        if not bin_tasks:
            continue
        for method in methods:
            solved = set(costs[method]) & set(bin_tasks)
            common = solved & set(costs[config.baseline])
            wall_ratios = [
                costs[method][task] / costs[config.baseline][task]
                for task in common
            ]
            memory_ratios = [
                memory_costs[method][task] / memory_costs[config.baseline][task]
                for task in common
            ]
            runtime_rows.append({
                "baseline_runtime_bin": label,
                "backend": method,
                "tasks": len(bin_tasks),
                "all_repetitions_solved": len(solved),
                "wall_ratio_geomean": (
                    f"{geometric_mean(wall_ratios):.6g}" if wall_ratios else ""
                ),
                "memory_ratio_geomean": (
                    f"{geometric_mean(memory_ratios):.6g}" if memory_ratios else ""
                ),
            })
    write_tsv(config.output / "dynamic-scale-summary.tsv", runtime_rows)

    if metadata:
        bins = [
            ("0001-0050", 0, 50), ("0051-0100", 51, 100),
            ("0101-0250", 101, 250), ("0251-0500", 251, 500),
            ("0501-plus", 501, math.inf),
        ]
        scale_rows = []
        for label, lower, upper in bins:
            bin_tasks = [
                task for task in tasks
                if lower
                <= int(
                    metadata.get(task, {}).get("adapter_transitive_lines")
                    or metadata.get(task, {}).get("source_lines", 0)
                )
                <= upper
            ]
            if not bin_tasks:
                continue
            for method in methods:
                solved = set(costs[method]) & set(bin_tasks)
                common = solved & set(costs[config.baseline])
                ratios = [costs[method][task] / costs[config.baseline][task] for task in common]
                scale_rows.append({
                    "source_line_bin": label, "backend": method,
                    "tasks": len(bin_tasks), "all_repetitions_solved": len(solved),
                    "paired_with_baseline": len(common),
                    "wall_ratio_geomean": f"{geometric_mean(ratios):.6g}" if ratios else "",
                })
        write_tsv(config.output / "scale-summary.tsv", scale_rows)
    svg_profile(config.output / "performance-profile.svg", costs, tasks)

    report = ["# Paired BenchExec analysis", "",
              f"- Unit of analysis: task; repetitions are summarized within each task.",
              f"- Input rows: {len(rows)}; tasks: {len(tasks)}; repetitions per complete cell: {repetitions}.",
              f"- Baseline: `{config.baseline}`.",
              f"- Cross-backend verdict mismatches excluded: {len(verdict_mismatches)}.",
              f"- Tasks with at least one timeout/OOM/error: {len(failure_matrix)}.",
              "- Uncensored ratios include only tasks solved by both methods in every repetition.",
              "- The performance profile retains failures as infinite cost.", "", "## Pairwise wall-time results", ""]
    for row in pairwise:
        report.append(
            f"- `{row['backend']}` / `{row['baseline']}`: {row['wall_ratio_geomean']}x "
            f"(task-bootstrap 95% CI {row['bootstrap_95ci_low']}–{row['bootstrap_95ci_high']}), "
            f"CPU {row['cpu_ratio_geomean']}x "
            f"[{row['cpu_95ci_low']}, {row['cpu_95ci_high']}], peak RSS "
            f"{row['memory_ratio_geomean']}x "
            f"[{row['memory_95ci_low']}, {row['memory_95ci_high']}], "
            f"n={row['paired_tasks']}, wins/losses/ties={row['backend_faster']}/"
            f"{row['baseline_faster']}/{row['ties']}, Holm sign-test p={row['holm_p']}."
        )
    (config.output / "analysis-report.md").write_text("\n".join(report) + "\n", encoding="utf-8")
    (config.output / "stats-appendix.md").write_text(
        "# Statistical appendix\n\n"
        "Wall times are strongly skewed and resource-censored, so no normality-based "
        "test is used. Each task contributes one median per backend. Geometric mean "
        f"ratios use a deterministic {config.bootstrap:,}-sample task bootstrap; the exact paired "
        "sign test discards exact ties and Holm-corrects the planned baseline contrasts. "
        "Solved-only ratios cannot describe coverage, so timeout/OOM counts and the "
        "failure-aware performance profile are co-primary evidence.\n",
        encoding="utf-8",
    )
    (config.output / "figure-catalog.md").write_text(
        "# Figure catalog\n\n"
        "## `performance-profile.svg`\n\n"
        "- Purpose: compare task coverage and relative wall time without dropping failures.\n"
        "- Data: per-task median wall time across complete repetitions; failures have infinite cost.\n"
        "- Read: higher curves are better; the right-edge height is the solved-task fraction.\n"
        "- Caveat: close curves on sub-0.1 s tasks are sensitive to process-startup noise.\n\n"
        "## `coverage-status.svg`\n\n"
        "- Purpose: expose solved, timeout, OOM, and other outcomes before solved-only ratios.\n"
        "- Data: exact repeated-run counts from `descriptive-summary.tsv`.\n"
        "- Read: compare both solved fraction and the type of resource failure.\n"
        "- Caveat: repeated runs are shown for stability, not treated as independent tasks.\n",
        encoding="utf-8",
    )
    print(f"analyzed {len(rows)} rows across {len(tasks)} tasks and {len(methods)} backends")
    print(config.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
