#!/usr/bin/env python3
"""Analyze latest CAAT-SC TIMEOUT tasks for which Deagle reports a correct result."""

from __future__ import annotations

import csv
import json
import re
import statistics
from collections import Counter, defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
COMPARE = ROOT / "optimization-analysis/svcomp2026/fair-coverage/comparison-latest-vs-deagle-60s/all-six"
CSV_PATH = COMPARE / "html/adapted-725-previous-latest-sc-tso-pso-genmc-deagle-60s.table.csv"
RAW = COMPARE / "raw"
TASK_ROOT = Path("/Users/sujie/Documents/Codes/sv-benchmarks/c")
OUT = Path(__file__).resolve().parent / "analysis-output"

METHODS = ["previous_caat_sc", "genmc", "caat_sc", "caat_tso", "caat_pso", "deagle"]
LOG_DIRS = {
    "previous_caat_sc": next(RAW.glob("adapted-previous-caat-sc.*.logfiles")),
    "genmc": next(RAW.glob("adapted-genmc.*.logfiles")),
    "caat_sc": next(RAW.glob("adapted-caat-sc.*.logfiles")),
    "caat_tso": next(RAW.glob("adapted-caat-tso.*.logfiles")),
    "caat_pso": next(RAW.glob("adapted-caat-pso.*.logfiles")),
    "deagle": next(RAW.glob("adapted-deagle.*.logfiles")),
}
LOG_PREFIX = {
    "previous_caat_sc": "Previous GenMC+CAAT-SC",
    "genmc": "GenMC",
    "caat_sc": "GenMC+CAAT-SC",
    "caat_tso": "GenMC+CAAT-TSO",
    "caat_pso": "GenMC+CAAT-PSO",
    "deagle": "Deagle",
}


def number(value: str) -> float | None:
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def percentile(values: list[float], q: float) -> float | None:
    if not values:
        return None
    values = sorted(values)
    pos = (len(values) - 1) * q
    lo = int(pos)
    hi = min(lo + 1, len(values) - 1)
    return values[lo] + (values[hi] - values[lo]) * (pos - lo)


def read_results() -> list[dict]:
    with CSV_PATH.open(newline="") as handle:
        raw_rows = list(csv.reader(handle, delimiter="\t"))
    records = []
    for row in raw_rows[3:]:
        if len(row) < 38:
            continue
        rec = {"task": row[0], "expected": row[1]}
        for i, method in enumerate(METHODS):
            start = 2 + 6 * i
            rec[method] = {
                "status": row[start],
                "category": row[start + 1],
                "cpu_s": number(row[start + 2]),
                "wall_s": number(row[start + 3]),
                "rss_mb": number(row[start + 4]),
                "termination": row[start + 5],
            }
        records.append(rec)
    return records


def locate_log(method: str, task: str) -> Path:
    basename = Path(task).name
    path = LOG_DIRS[method] / f"{LOG_PREFIX[method]}.{basename}.log"
    if not path.exists():
        raise FileNotFoundError(path)
    return path


def parse_yaml_input(task_path: Path) -> str | None:
    text = task_path.read_text(errors="replace")
    match = re.search(r"(?m)^input_files:\s*['\"]?([^'\"\n]+)", text)
    return match.group(1).strip() if match else None


def source_features(task: str) -> dict:
    task_path = TASK_ROOT / task
    input_name = parse_yaml_input(task_path)
    input_path = task_path.parent / input_name if input_name else None
    source_path = input_path
    if input_path and input_path.suffix == ".i":
        candidate = input_path.with_suffix(".c")
        if candidate.exists():
            source_path = candidate
    if not source_path or not source_path.exists():
        return {"source": "", "source_missing": True}
    text = source_path.read_text(errors="replace")
    code = re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)
    return {
        "source": str(source_path),
        "source_missing": False,
        "source_bytes": source_path.stat().st_size,
        "source_loc": sum(bool(line.strip()) for line in text.splitlines()),
        "pthread_create_calls": len(re.findall(r"\bpthread_create\s*\(", code)),
        "pthread_join_calls": len(re.findall(r"\bpthread_join\s*\(", code)),
        "mutex_ops": len(re.findall(r"\bpthread_mutex_(?:lock|unlock|trylock)\s*\(", code)),
        "atomic_begin_calls": len(re.findall(r"\b__VERIFIER_atomic_begin\s*\(", code)),
        "nondet_calls": len(re.findall(r"\b__VERIFIER_nondet_[A-Za-z0-9_]*\s*\(", code)),
        "assume_calls": len(re.findall(r"\b(?:__VERIFIER_assume|assume_abort_if_not)\s*\(", code)),
        "loop_tokens": len(re.findall(r"\b(?:while|for|do)\b", code)),
        "thread_routines": len(re.findall(r"\bvoid\s*\*\s*[A-Za-z_$][\w$]*\s*\([^;{}]*\)\s*\{", code)),
        "weak_encoding_identifiers": len(re.findall(r"\b(?:weak\$\$|[A-Za-z_$][\w$]*\$(?:w_buff|r_buff|flush_delayed))", code)),
    }


def deagle_features(log_text: str, status: str) -> dict:
    if "No loops!" in log_text:
        unwind_mode = "no_loops"
    elif "The bound of loops is not determined!" in log_text and "Unwindset:" in log_text:
        unwind_mode = "fallback_bound_3"
    elif "All loops can be statically determined!" in log_text or "Unwindset:" in log_text:
        unwind_mode = "suggested_finite_bound"
    elif "timeout 5 " in log_text:
        unwind_mode = "auto_unwind_attempt"
    else:
        unwind_mode = "unclassified"
    return {
        "deagle_unwind_mode": unwind_mode,
        "deagle_true_incomplete_bound": status == "true" and unwind_mode == "fallback_bound_3",
        "deagle_log_has_failure": "Property: FAILURE" in log_text,
        "deagle_log_has_success": "Property: SUCCESS" in log_text,
    }


def classify(row: dict) -> str:
    family = row["family"]
    if family == "pthread-wmm":
        return "loop-free weak-memory encoding / explicit-schedule explosion"
    if row["deagle_true_incomplete_bound"]:
        return "unbounded loop / Deagle bounded-TRUE caveat"
    if row.get("loop_tokens", 0) > 0 or row.get("pthread_create_calls", 0) >= 4:
        return "loop or repeated-thread/state-space explosion"
    if row["caat_sc_rss_mb"] >= 512:
        return "large-state time+memory pressure"
    return "finite non-WMM exploration explosion"


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    all_records = read_results()
    primary = [
        rec for rec in all_records
        if rec["caat_sc"]["status"] == "TIMEOUT" and rec["deagle"]["category"] == "correct"
    ]
    rows = []
    for rec in primary:
        task = rec["task"]
        dlog = locate_log("deagle", task).read_text(errors="replace")
        clog = locate_log("caat_sc", task).read_text(errors="replace")
        row = {
            "task": task,
            "family": task.split("/", 1)[0],
            "expected": rec["expected"],
            "previous_status": rec["previous_caat_sc"]["status"],
            "genmc_status": rec["genmc"]["status"],
            "caat_sc_status": rec["caat_sc"]["status"],
            "caat_sc_cpu_s": rec["caat_sc"]["cpu_s"],
            "caat_sc_rss_mb": rec["caat_sc"]["rss_mb"],
            "deagle_status": rec["deagle"]["status"],
            "deagle_cpu_s": rec["deagle"]["cpu_s"],
            "deagle_rss_mb": rec["deagle"]["rss_mb"],
            "caat_log_output_bytes": len(clog.split("\n\n--------------------------------------------------------------------------------\n\n", 1)[-1]),
        }
        row.update(source_features(task))
        row.update(deagle_features(dlog, rec["deagle"]["status"]))
        row["has_nondet"] = row.get("nondet_calls", 0) > 0
        row["strict_semantics_comparable"] = (
            not row["has_nondet"] and not row["deagle_true_incomplete_bound"]
        )
        row["mechanism_cluster"] = classify(row)
        rows.append(row)

    fields = list(rows[0])
    with (OUT / "primary-cohort.tsv").open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fields, delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)

    by_family = Counter(row["family"] for row in rows)
    family_expected = defaultdict(Counter)
    for row in rows:
        family_expected[row["family"]][row["expected"]] += 1
    rss = [row["caat_sc_rss_mb"] for row in rows if row["caat_sc_rss_mb"] is not None]
    dcpu = [row["deagle_cpu_s"] for row in rows if row["deagle_cpu_s"] is not None]
    all_family = Counter(rec["task"].split("/", 1)[0] for rec in all_records)
    family_coverage = {
        family: {
            "all_tasks": all_family[family],
            "primary_tasks": count,
            "primary_rate": count / all_family[family],
        }
        for family, count in by_family.most_common()
    }
    clean = [row for row in rows if row["strict_semantics_comparable"]]
    nondet_by_expected = defaultdict(Counter)
    for row in rows:
        nondet_by_expected[row["expected"]]["all"] += 1
        if row["has_nondet"]:
            nondet_by_expected[row["expected"]]["has_nondet"] += 1
    summary = {
        "comparison": {
            "tasks_total": len(all_records),
            "primary_cohort_definition": "latest CAAT-SC TIMEOUT and Deagle category correct",
            "primary_cohort_tasks": len(rows),
            "caat_sc_timeouts_total": sum(rec["caat_sc"]["status"] == "TIMEOUT" for rec in all_records),
            "native_genmc_timeouts_total": sum(rec["genmc"]["status"] == "TIMEOUT" for rec in all_records),
            "deagle_correct_total": sum(rec["deagle"]["category"] == "correct" for rec in all_records),
        },
        "expected": dict(Counter(row["expected"] for row in rows)),
        "families": dict(by_family.most_common()),
        "family_coverage": family_coverage,
        "family_expected": {family: dict(counts) for family, counts in family_expected.items()},
        "native_genmc_status": dict(Counter(row["genmc_status"] for row in rows)),
        "previous_caat_status": dict(Counter(row["previous_status"] for row in rows)),
        "deagle_unwind_mode": dict(Counter(row["deagle_unwind_mode"] for row in rows)),
        "deagle_bounded_true_incomplete": sum(row["deagle_true_incomplete_bound"] for row in rows),
        "semantic_comparability": {
            "has_nondet_seed_mismatch": sum(row["has_nondet"] for row in rows),
            "nondet_by_expected": {key: dict(value) for key, value in nondet_by_expected.items()},
            "strict_tasks": len(clean),
            "strict_expected": dict(Counter(row["expected"] for row in clean)),
            "strict_native_genmc_status": dict(Counter(row["genmc_status"] for row in clean)),
            "strict_tasks_list": [row["task"] for row in clean],
        },
        "mechanism_clusters": dict(Counter(row["mechanism_cluster"] for row in rows)),
        "caat_sc_rss_mb": {
            "median": statistics.median(rss), "p90": percentile(rss, .90), "max": max(rss),
            "under_64_mb": sum(value < 64 for value in rss),
            "at_least_512_mb": sum(value >= 512 for value in rss),
        },
        "deagle_cpu_s": {
            "median": statistics.median(dcpu), "p90": percentile(dcpu, .90), "max": max(dcpu),
            "under_1_s": sum(value < 1 for value in dcpu),
        },
        "source_features": {
            key: {
                "median": statistics.median(values), "p90": percentile(values, .90), "max": max(values)
            }
            for key in [
                "source_loc", "pthread_create_calls", "atomic_begin_calls", "nondet_calls",
                "loop_tokens", "weak_encoding_identifiers"
            ]
            if (values := [row[key] for row in rows if not row.get("source_missing")])
        },
    }
    (OUT / "summary.json").write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")
    print(json.dumps(summary, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
