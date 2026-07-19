#!/usr/bin/env python3
"""Exhaustively compare small SC program states under RF-DPOR and SC-RVF."""

import argparse
import csv
import hashlib
import itertools
import json
import re
import subprocess
import tempfile
import time
from pathlib import Path


def canonical_shapes():
    """Return x/y and thread-symmetry representatives of six access choices."""
    representatives = {}
    for shape in itertools.product(range(2), repeat=6):
        swap_variables = tuple(1 - value for value in shape)
        swap_threads = shape[3:] + shape[:3]
        swap_both = tuple(1 - value for value in swap_threads)
        representative = min(shape, swap_variables, swap_threads, swap_both)
        representatives.setdefault(representative, shape)
    return sorted(representatives)


def source_for(shape, outcome, loop_iterations=0, unroll_iterations=0, native_prefix=False):
    names = tuple("xy"[choice] for choice in shape)
    thread0_body = f"""
    atomic_store_explicit(&{names[0]}, 1, memory_order_seq_cst);
    int value = atomic_load_explicit(&{names[1]}, memory_order_seq_cst);
    atomic_store_explicit(&r0, value, memory_order_seq_cst);
    atomic_store_explicit(&{names[2]}, 2, memory_order_seq_cst);"""
    thread1_body = f"""
    atomic_store_explicit(&{names[3]}, 1, memory_order_seq_cst);
    int value = atomic_load_explicit(&{names[4]}, memory_order_seq_cst);
    atomic_store_explicit(&r1, value, memory_order_seq_cst);
    atomic_store_explicit(&{names[5]}, 2, memory_order_seq_cst);"""
    if loop_iterations:
        thread0_body = f"""
    for (int iteration = 0; iteration != {loop_iterations}; ++iteration) {{
        {thread0_body}
    }}"""
        thread1_body = f"""
    for (int iteration = 0; iteration != {loop_iterations}; ++iteration) {{
        {thread1_body}
    }}"""
    elif unroll_iterations:
        thread0_body = "\n".join(f"    {{\n{thread0_body}\n    }}" for _ in range(unroll_iterations))
        thread1_body = "\n".join(f"    {{\n{thread1_body}\n    }}" for _ in range(unroll_iterations))
    prefix = "    int native = native_boundary;\n    (void)native;\n" if native_prefix else ""
    return f"""#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>

static atomic_int x;
static atomic_int y;
static atomic_int r0;
static atomic_int r1;
static volatile int native_boundary = 1;

static void *thread0(void *arg)
{{
{prefix}
{thread0_body}
    return arg;
}}

static void *thread1(void *arg)
{{
{prefix}
{thread1_body}
    return arg;
}}

int main(void)
{{
    pthread_t t0;
    pthread_t t1;
    pthread_create(&t0, 0, thread0, 0);
    pthread_create(&t1, 0, thread1, 0);
    pthread_join(t0, 0);
    pthread_join(t1, 0);
    int final_x = atomic_load_explicit(&x, memory_order_seq_cst);
    int final_y = atomic_load_explicit(&y, memory_order_seq_cst);
    int final_r0 = atomic_load_explicit(&r0, memory_order_seq_cst);
    int final_r1 = atomic_load_explicit(&r1, memory_order_seq_cst);
    if (final_r0 == {outcome[0]} && final_r1 == {outcome[1]} &&
        final_x == {outcome[2]} && final_y == {outcome[3]})
        assert(0);
    return 0;
}}
"""


def normalized_lines(output, prefixes):
    lines = []
    for line in output.splitlines():
        if line.startswith(prefixes):
            lines.append(line)
    return "\n".join(sorted(set(lines)))


def field(output, pattern, default="0"):
    match = re.search(pattern, output)
    return match.group(1) if match else default


def decoded_output(value):
    """Normalize TimeoutExpired output across Python subprocess implementations."""
    if isinstance(value, bytes):
        return value.decode(errors="replace")
    return value or ""


def run_one(binary, model, program, mode, workers, timeout_seconds, regional_loops=False):
    command = [
        str(binary),
        f"--model-file={model}",
        "--disable-estimation",
        "--cat-stats",
        f"--nthreads={workers}",
    ]
    if mode == "rvf":
        command.append("--sc-rvf-exploration")
        if regional_loops:
            command.append("--sc-rvf-regional")
    command.append(str(program))
    start = time.monotonic_ns()
    try:
        process = subprocess.run(
            command,
            capture_output=True,
            text=True,
            timeout=timeout_seconds,
            check=False,
        )
        status = process.returncode
        output = process.stdout + process.stderr
    except subprocess.TimeoutExpired as error:
        status = 124
        output = decoded_output(error.stdout) + decoded_output(error.stderr)
    elapsed = time.monotonic_ns() - start
    errors = normalized_lines(output, ("Error:", "No errors were detected."))
    warnings = normalized_lines(output, ("Warning:",))
    return {
        "status": status,
        "elapsed_ns": elapsed,
        "gate": field(output, r"SC RVF program gate: ([^\n]+)", ""),
        "complete": field(output, r"Number of complete executions explored: ([0-9]+)"),
        "fail_open": field(output, r"rvf-fail-open=([0-9]+)"),
        "loads_reduced": field(output, r"rvf-loads-reduced=([0-9]+)"),
        "error_sha256": hashlib.sha256(errors.encode()).hexdigest(),
        "warning_sha256": hashlib.sha256(warnings.encode()).hexdigest(),
        "output": output,
    }


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--genmc", required=True, type=Path)
    parser.add_argument("--model", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--timeout", type=float, default=20.0)
    parser.add_argument("--limit-shapes", type=int)
    parser.add_argument("--shape-index", type=int)
    parser.add_argument(
        "--outcome",
        help="restrict the oracle to one four-digit outcome over values 0, 1, and 2",
    )
    parser.add_argument("--worker-counts", default="1,2")
    parser.add_argument("--loop-iterations", type=int, default=0)
    parser.add_argument("--unroll-iterations", type=int, default=0)
    parser.add_argument(
        "--native-prefix",
        action="store_true",
        help="place one fixed non-atomic read before each generated worker suffix",
    )
    args = parser.parse_args()

    if args.loop_iterations < 0 or args.unroll_iterations < 0:
        parser.error("iteration counts must be non-negative")
    if args.loop_iterations and args.unroll_iterations:
        parser.error("--loop-iterations and --unroll-iterations are mutually exclusive")

    if not args.genmc.is_file():
        parser.error(f"missing GenMC binary: {args.genmc}")
    if not args.model.is_file():
        parser.error(f"missing CAT model: {args.model}")

    args.output.mkdir(parents=True, exist_ok=True)
    logs = args.output / "failures"
    logs.mkdir(exist_ok=True)
    shapes = canonical_shapes()
    if args.shape_index is not None:
        if not 0 <= args.shape_index < len(shapes):
            parser.error(f"--shape-index must be in [0, {len(shapes) - 1}]")
        shapes = [shapes[args.shape_index]]
    if args.limit_shapes is not None:
        shapes = shapes[: args.limit_shapes]
    if args.outcome is not None:
        if len(args.outcome) != 4 or any(value not in "012" for value in args.outcome):
            parser.error("--outcome must contain exactly four digits from 0, 1, and 2")
        outcomes = [tuple(int(value) for value in args.outcome)]
    else:
        outcomes = list(itertools.product(range(3), repeat=4))
    worker_counts = tuple(int(value) for value in args.worker_counts.split(","))
    if not worker_counts or any(value not in (1, 2) for value in worker_counts):
        parser.error("--worker-counts must contain 1 and/or 2")
    fields = [
        "shape",
        "outcome",
        "mode",
        "workers",
        "status",
        "elapsed_ns",
        "gate",
        "complete",
        "fail_open",
        "loads_reduced",
        "error_sha256",
        "warning_sha256",
    ]
    violations = []
    rows = []
    reduced_cells = 0

    with tempfile.TemporaryDirectory(prefix="genmc-rvf-generated-") as temp:
        program = Path(temp) / "case.c"
        for shape_index, shape in enumerate(shapes):
            shape_name = "".join(str(value) for value in shape)
            for outcome in outcomes:
                outcome_name = "".join(str(value) for value in outcome)
                program.write_text(
                    source_for(
                        shape,
                        outcome,
                        args.loop_iterations,
                        args.unroll_iterations,
                        args.native_prefix,
                    )
                )
                results = {}
                for mode, workers in itertools.product(("baseline", "rvf"), worker_counts):
                    result = run_one(
                        args.genmc.resolve(),
                        args.model.resolve(),
                        program,
                        mode,
                        workers,
                        args.timeout,
                        bool(args.loop_iterations),
                    )
                    results[(mode, workers)] = result
                    rows.append(
                        {
                            "shape": shape_name,
                            "outcome": outcome_name,
                            "mode": mode,
                            "workers": workers,
                            **{key: result[key] for key in fields[4:]},
                        }
                    )
                for workers in worker_counts:
                    baseline = results[("baseline", workers)]
                    rvf = results[("rvf", workers)]
                    label = f"shape={shape_name}/outcome={outcome_name}/n{workers}"
                    if rvf["status"] != baseline["status"]:
                        violations.append(f"{label}: status mismatch")
                    if rvf["error_sha256"] != baseline["error_sha256"]:
                        violations.append(f"{label}: error-category mismatch")
                    if rvf["status"] not in (0, 42) or baseline["status"] not in (0, 42):
                        violations.append(f"{label}: non-verdict process status")
                    if (
                        rvf["status"] == 0
                        and baseline["status"] == 0
                        and rvf["warning_sha256"] != baseline["warning_sha256"]
                    ):
                        violations.append(f"{label}: safe warning mismatch")
                    expected_gate = "regional" if args.loop_iterations else "enabled"
                    if not rvf["gate"].startswith(expected_gate):
                        violations.append(f"{label}: unexpected RVF gate {rvf['gate']!r}")
                    if rvf["gate"].startswith(expected_gate) and rvf["fail_open"] != "0":
                        violations.append(f"{label}: late fail-open")
                    if int(rvf["loads_reduced"]) > 0:
                        reduced_cells += 1
                if set(worker_counts) == {1, 2}:
                    rvf_one = results[("rvf", 1)]
                    rvf_two = results[("rvf", 2)]
                    label = f"shape={shape_name}/outcome={outcome_name}"
                    if (rvf_one["status"], rvf_one["error_sha256"]) != (
                        rvf_two["status"],
                        rvf_two["error_sha256"],
                    ):
                        violations.append(f"{label}: RVF worker mismatch")
                    if rvf_one["status"] == 0 and rvf_one["complete"] != rvf_two["complete"]:
                        violations.append(f"{label}: RVF safe execution-count mismatch")
                if violations and violations[-1].startswith(
                    f"shape={shape_name}/outcome={outcome_name}"
                ):
                    for key, result in results.items():
                        (logs / f"{shape_name}-{outcome_name}-{key[0]}-n{key[1]}.log").write_text(
                            result["output"]
                        )
            print(f"completed shape {shape_index + 1}/{len(shapes)}: {shape_name}", flush=True)

    with (args.output / "results.tsv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, delimiter="\t", fieldnames=fields)
        writer.writeheader()
        writer.writerows(rows)
    report = {
        "canonical_shapes": len(shapes),
        "states_per_shape": len(outcomes),
        "state_cells": len(shapes) * len(outcomes),
        "invocations": len(rows),
        "worker_counts": worker_counts,
        "loop_iterations": args.loop_iterations,
        "unroll_iterations": args.unroll_iterations,
        "native_prefix": args.native_prefix,
        "reduced_rvf_cells": reduced_cells,
        "violations": violations,
    }
    (args.output / "analysis.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))
    raise SystemExit(bool(violations))


if __name__ == "__main__":
    main()
