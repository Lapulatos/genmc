#!/usr/bin/env python3
import argparse
import csv
import json
import statistics
from collections import defaultdict
from pathlib import Path


def integer(row, key):
    return int(row.get(key, "") or 0)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("results", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    rows = list(csv.DictReader(args.results.open(), delimiter="\t"))
    grouped = defaultdict(list)
    for row in rows:
        grouped[row["case"]].append(row)

    violations = []
    cases = []
    for name, group in sorted(grouped.items()):
        if len(group) != 4:
            violations.append(f"{name}: expected 4 rows, found {len(group)}")
            continue
        baselines = {r["workers"]: r for r in group if r["mode"] == "baseline"}
        rvf = sorted((r for r in group if r["mode"] == "rvf"), key=lambda r: r["workers"])
        if set(baselines) != {"1", "2"} or len(rvf) != 2:
            violations.append(f"{name}: missing paired baseline or RVF worker row")
            continue
        for row in rvf:
            baseline = baselines[row["workers"]]
            if row["status"] != baseline["status"]:
                violations.append(f"{name}/n{row['workers']}: exit status mismatch")
            if row["verdict_sha256"] != baseline["verdict_sha256"]:
                violations.append(f"{name}/n{row['workers']}: semantic verdict mismatch")
            if row["gate"].startswith("enabled") and integer(row, "fail_open") != 0:
                violations.append(f"{name}/n{row['workers']}: unsafe late fail-open occurred")
            if (
                row["gate"].startswith("native-fallback")
                and baseline["status"] == "0"
                and integer(row, "complete") != integer(baseline, "complete")
            ):
                violations.append(f"{name}/n{row['workers']}: fallback changed execution count")
        worker_key_0 = (rvf[0]["status"], rvf[0]["verdict_sha256"])
        worker_key_1 = (rvf[1]["status"], rvf[1]["verdict_sha256"])
        if rvf[0]["status"] == "0":
            worker_key_0 += (rvf[0]["complete"],)
            worker_key_1 += (rvf[1]["complete"],)
        if worker_key_0 != worker_key_1:
            violations.append(f"{name}: one/two-worker RVF mismatch")
        cases.append(
            {
                "case": name,
                "gate": rvf[0]["gate"],
                "baseline_complete": integer(baselines["1"], "complete"),
                "rvf_complete": integer(rvf[0], "complete"),
                "complete_reduction": integer(baselines["1"], "complete")
                - integer(rvf[0], "complete"),
                "rvf_blocked": integer(rvf[0], "blocked"),
                "rvf_loads_reduced": integer(rvf[0], "reduced"),
                "rvf_singleton_bypass": integer(rvf[0], "singleton_bypass"),
                "rvf_native_reads_synthesized": integer(
                    rvf[0], "native_reads_synthesized"
                ),
                "baseline_elapsed_ns": integer(baselines["1"], "elapsed_ns"),
                "rvf_n1_elapsed_ns": integer(rvf[0], "elapsed_ns"),
                "rvf_n2_elapsed_ns": integer(rvf[1], "elapsed_ns"),
            }
        )

    supported = [case for case in cases if case["gate"].startswith("enabled")]
    fallback = [case for case in cases if case["gate"].startswith("native-fallback")]
    pre_gate_failure = [case for case in cases if not case["gate"]]
    ratios = [case["rvf_n1_elapsed_ns"] / case["baseline_elapsed_ns"] for case in supported]
    report = {
        "input_rows": len(rows),
        "cases": len(cases),
        "supported_cases": len(supported),
        "fallback_cases": len(fallback),
        "pre_gate_failure_cases": len(pre_gate_failure),
        "timed_out_rows": sum(row["status"] in {"124", "137"} for row in rows),
        "invariant_violations": violations,
        "total_baseline_complete_supported": sum(c["baseline_complete"] for c in supported),
        "total_rvf_complete_supported": sum(c["rvf_complete"] for c in supported),
        "total_complete_reduction_supported": sum(c["complete_reduction"] for c in supported),
        "total_rvf_loads_reduced": sum(c["rvf_loads_reduced"] for c in supported),
        "total_rvf_singleton_bypass": sum(c["rvf_singleton_bypass"] for c in supported),
        "total_rvf_native_reads_synthesized": sum(
            c["rvf_native_reads_synthesized"] for c in supported
        ),
        "median_rvf_n1_over_baseline_elapsed": statistics.median(ratios) if ratios else None,
        "cases_detail": cases,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n")
    print(json.dumps({k: v for k, v in report.items() if k != "cases_detail"}, indent=2))
    raise SystemExit(bool(violations))


if __name__ == "__main__":
    main()
