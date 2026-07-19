#!/usr/bin/env python3
"""Pair the latest five-method census with Deagle and the previous CAAT run."""

from __future__ import annotations

import argparse
from collections import Counter
import csv
import json
import math
from pathlib import Path
import random
import statistics


SOLVED = {"true", "false(unreach-call)"}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--current", type=Path, required=True)
    parser.add_argument("--previous", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--bootstrap", type=int, default=10_000)
    return parser.parse_args()


def load(path: Path) -> dict[str, dict[str, dict[str, str]]]:
    with path.open(encoding="utf-8", newline="") as source:
        rows = list(csv.DictReader(source, delimiter="\t"))
    result: dict[str, dict[str, dict[str, str]]] = {}
    for row in rows:
        method = row["backend"]
        task = row["task_yaml"]
        if task in result.setdefault(method, {}):
            raise ValueError(f"duplicate row for {method}: {task}")
        result[method][task] = row
    counts = {method: len(tasks) for method, tasks in result.items()}
    if set(counts.values()) != {725}:
        raise ValueError(f"expected 725 rows per method, found {counts}")
    return result


def write_tsv(path: Path, rows: list[dict[str, object]]) -> None:
    if not rows:
        raise ValueError(f"refusing to write empty table {path}")
    with path.open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=list(rows[0]), delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def geomean(values: list[float]) -> float:
    return math.exp(statistics.fmean(math.log(value) for value in values))


def quantile(values: list[float], probability: float) -> float:
    ordered = sorted(values)
    position = (len(ordered) - 1) * probability
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    return ordered[lower] * (upper - position) + ordered[upper] * (position - lower)


def bootstrap_ci(values: list[float], samples: int, seed: int) -> tuple[float, float]:
    rng = random.Random(seed)
    estimates = []
    for _ in range(samples):
        draw = [values[rng.randrange(len(values))] for _ in values]
        estimates.append(geomean(draw))
    return quantile(estimates, 0.025), quantile(estimates, 0.975)


def summarize(label: str, rows: dict[str, dict[str, str]]) -> dict[str, object]:
    statuses = Counter(row["status"] for row in rows.values())
    categories = Counter(row["category"] for row in rows.values())
    solved = [row for row in rows.values() if row["status"] in SOLVED]
    return {
        "method": label,
        "tasks": len(rows),
        "correct": categories["correct"],
        "wrong": categories["wrong"],
        "solved_status": len(solved),
        "timeout": sum(value for key, value in statuses.items() if key.startswith("TIMEOUT")),
        "oom": sum(value for key, value in statuses.items() if key.startswith("OUT OF MEMORY")),
        "other_unknown": len(rows) - len(solved)
        - sum(value for key, value in statuses.items() if key.startswith("TIMEOUT"))
        - sum(value for key, value in statuses.items() if key.startswith("OUT OF MEMORY")),
        "median_cpu_s_solved": f"{statistics.median(float(row['cputime_seconds']) for row in solved):.9g}",
        "median_rss_bytes_solved": f"{statistics.median(float(row['memory_bytes']) for row in solved):.9g}",
    }


def paired_metrics(
    numerator_label: str,
    numerator: dict[str, dict[str, str]],
    denominator_label: str,
    denominator: dict[str, dict[str, str]],
    samples: int,
    seed: int,
) -> dict[str, object]:
    tasks = sorted(set(numerator) & set(denominator))
    common = [
        task
        for task in tasks
        if numerator[task]["status"] in SOLVED
        and numerator[task]["status"] == denominator[task]["status"]
    ]
    if not common:
        raise ValueError(f"no same-verdict terminal pairs for {numerator_label}/{denominator_label}")
    metrics = {
        "cpu": "cputime_seconds",
        "wall": "walltime_seconds",
        "rss": "memory_bytes",
    }
    output: dict[str, object] = {
        "numerator": numerator_label,
        "denominator": denominator_label,
        "same_verdict_terminal_tasks": len(common),
        "both_terminal_different_verdict": sum(
            numerator[task]["status"] in SOLVED
            and denominator[task]["status"] in SOLVED
            and numerator[task]["status"] != denominator[task]["status"]
            for task in tasks
        ),
    }
    for offset, (name, column) in enumerate(metrics.items()):
        ratios = [float(numerator[task][column]) / float(denominator[task][column]) for task in common]
        low, high = bootstrap_ci(ratios, samples, seed + offset)
        output[f"{name}_ratio_geomean"] = f"{geomean(ratios):.6g}"
        output[f"{name}_ci_low"] = f"{low:.6g}"
        output[f"{name}_ci_high"] = f"{high:.6g}"
        output[f"{name}_numerator_better"] = sum(value < 1 for value in ratios)
        output[f"{name}_denominator_better"] = sum(value > 1 for value in ratios)
    return output


def status_class(status: str) -> str:
    if status in SOLVED:
        return status
    if status.startswith("TIMEOUT"):
        return "TIMEOUT"
    if status.startswith("OUT OF MEMORY"):
        return "OOM"
    return status


def transitions(
    before: dict[str, dict[str, str]], after: dict[str, dict[str, str]]
) -> list[dict[str, object]]:
    counts = Counter(
        (status_class(before[task]["status"]), status_class(after[task]["status"]))
        for task in before
    )
    return [
        {"before": source, "after": target, "tasks": count}
        for (source, target), count in sorted(counts.items())
    ]


def main() -> int:
    args = parse_args()
    current = load(args.current)
    previous = load(args.previous)
    args.output.mkdir(parents=True, exist_ok=True)

    current_order = [
        "GenMC", "GenMC+CAAT-SC", "GenMC+CAAT-TSO", "GenMC+CAAT-PSO", "Deagle"
    ]
    summaries = [summarize(method, current[method]) for method in current_order]
    summaries.append(summarize("Previous GenMC+CAAT-SC", previous["GenMC+CAAT"]))
    write_tsv(args.output / "coverage-summary.tsv", summaries)

    comparisons = [
        ("GenMC", current["GenMC"], "Deagle", current["Deagle"]),
        ("GenMC+CAAT-SC", current["GenMC+CAAT-SC"], "Deagle", current["Deagle"]),
        ("GenMC+CAAT-TSO", current["GenMC+CAAT-TSO"], "Deagle", current["Deagle"]),
        ("GenMC+CAAT-PSO", current["GenMC+CAAT-PSO"], "Deagle", current["Deagle"]),
        ("Latest GenMC+CAAT-SC", current["GenMC+CAAT-SC"], "Previous GenMC+CAAT-SC", previous["GenMC+CAAT"]),
        ("Current GenMC", current["GenMC"], "Previous GenMC", previous["GenMC"]),
    ]
    pairwise = [
        paired_metrics(*comparison, args.bootstrap, 20260716 + index * 10)
        for index, comparison in enumerate(comparisons)
    ]
    write_tsv(args.output / "pairwise-same-verdict.tsv", pairwise)
    transition_rows = transitions(previous["GenMC+CAAT"], current["GenMC+CAAT-SC"])
    write_tsv(args.output / "previous-to-latest-transitions.tsv", transition_rows)

    report = [
        "# Latest CAAT census comparison",
        "",
        "- Dataset: the same 725 adapted C.Concurrency tasks.",
        "- Limits: 60 CPU seconds, 4 GB, one core per task, one run per method.",
        "- Ratios use only task pairs where both methods terminate with the same true/false verdict.",
        "- TSO/PSO coverage counts are descriptive because the task labels are SC competition labels.",
        "",
        "## Coverage",
        "",
    ]
    for row in summaries:
        report.append(
            f"- {row['method']}: correct={row['correct']}, wrong={row['wrong']}, "
            f"terminal={row['solved_status']}, TIMEOUT={row['timeout']}, OOM={row['oom']}, "
            f"other unknown={row['other_unknown']}."
        )
    report += ["", "## Same-verdict paired cost ratios", ""]
    for row in pairwise:
        report.append(
            f"- {row['numerator']} / {row['denominator']}: n={row['same_verdict_terminal_tasks']}, "
            f"CPU={row['cpu_ratio_geomean']} "
            f"[{row['cpu_ci_low']}, {row['cpu_ci_high']}], "
            f"RSS={row['rss_ratio_geomean']} "
            f"[{row['rss_ci_low']}, {row['rss_ci_high']}]."
        )
    (args.output / "analysis-report.md").write_text("\n".join(report) + "\n", encoding="utf-8")
    (args.output / "summary.json").write_text(
        json.dumps(
            {"coverage": summaries, "pairwise": pairwise, "transitions": transition_rows},
            indent=2,
            ensure_ascii=False,
        )
        + "\n",
        encoding="utf-8",
    )
    print(f"methods={len(summaries)} comparisons={len(pairwise)} transitions={len(transition_rows)}")
    print(args.output / "analysis-report.md")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
