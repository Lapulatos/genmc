#!/usr/bin/env python3
"""Run a small, auditable GenMC compatibility probe outside BenchExec."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import subprocess
import time

import yaml

from rewrite_sources import rewrite_tree


def select_source(task_path: Path, task: dict) -> Path:
    inputs = task.get("input_files", [])
    if isinstance(inputs, str):
        inputs = [inputs]
    if len(inputs) != 1:
        raise ValueError(f"expected one input, found {len(inputs)}")
    source = task_path.parent / inputs[0]
    original = source.with_suffix(".c")
    if (
        source.suffix == ".i"
        and original.is_file()
        and "#include <svcomp.h>" in original.read_text(encoding="utf-8", errors="ignore")
    ):
        return source
    return original if source.suffix == ".i" and original.is_file() else source


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("benchmark_root", type=Path)
    parser.add_argument("tasks", type=Path)
    parser.add_argument("binary", type=Path)
    parser.add_argument("compat_header", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--timeout", type=float, default=15.0)
    parser.add_argument("--rewrite-dir", type=Path)
    parser.add_argument("--optimize", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    rows = []
    for raw in args.tasks.read_text(encoding="utf-8").splitlines():
        relative = raw.strip()
        if not relative or relative.startswith("#"):
            continue
        task_path = args.benchmark_root / relative
        task = yaml.safe_load(task_path.read_text(encoding="utf-8"))
        try:
            source = select_source(task_path, task)
        except ValueError as error:
            rows.append({"task": relative, "classification": "multi-input", "detail": str(error)})
            continue
        source_relative = source.relative_to(args.benchmark_root)
        text = source.read_text(encoding="utf-8", errors="ignore")
        rewrite_rules: tuple[str, ...] = ()
        if args.rewrite_dir:
            try:
                tree_rewrite = rewrite_tree(
                    source, args.benchmark_root, args.rewrite_dir
                )
            except ValueError as error:
                rows.append({
                    "task": relative,
                    "source": str(source.relative_to(args.benchmark_root)),
                    "classification": "unsupported-source-semantics",
                    "detail": str(error),
                })
                print(json.dumps(rows[-1], sort_keys=True))
                continue
            if tree_rewrite.unsupported_reason:
                rows.append({
                    "task": relative,
                    "source": str(source_relative),
                    "classification": "unsupported-source-semantics",
                    "detail": tree_rewrite.unsupported_reason,
                })
                print(json.dumps(rows[-1], sort_keys=True))
                continue
            rewritten_source = tree_rewrite.source
            rewrite_rules = tree_rewrite.rules
            compiler_include = rewritten_source.parent
            source = rewritten_source
        else:
            compiler_include = source.parent
        compiler = ["-m32"]
        if args.optimize:
            compiler.append("-O1")
        if "Numerical Integration Method" in text:
            compiler.append("-ffp-contract=off")
        compiler += ["-iquote", str(compiler_include)]
        if (
            "manual-atomic-runtime" not in rewrite_rules
            and (
                "__VERIFIER_atomic_begin" in text
                or "__VERIFIER_atomic_end" in text
                or "PTHREAD_RWLOCK_INITIALIZER" in text
                or "svcomp-atomic-function" in rewrite_rules
            )
        ):
            compiler += ["-include", str(args.compat_header)]
        compiler += ["-DNULL=0", str(source)]
        runtime_options = []
        if (
            "<stdatomic.h>" in text
            or "_Atomic" in text
            or re.search(r"\b__atomic_[A-Za-z0-9_]+\s*\(", text)
        ):
            runtime_options = ["--disable-ipr", "--disable-sr"]
        command = [
            str(args.binary), "--sc", *runtime_options, "--disable-estimation", "--disable-mm-detector",
            "--nthreads=1", "--v1", "--disable-race-detection", "--", *compiler,
        ]
        started = time.monotonic()
        timed_out = False
        try:
            run = subprocess.run(command, text=True, stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, timeout=args.timeout, check=False)
            exit_code = run.returncode
            output = run.stdout
        except subprocess.TimeoutExpired as error:
            timed_out = True
            exit_code = None
            output = error.stdout or ""
            if isinstance(output, bytes):
                output = output.decode(errors="replace")
        elapsed = time.monotonic() - started
        log_name = relative.removeprefix("c/").replace("/", "__").removesuffix(".yml") + ".log"
        (args.output / log_name).write_text(output, encoding="utf-8")
        if timed_out:
            classification = "timeout"
        elif "unknown external function" in output:
            classification = "unsupported-external"
        elif "error generated" in output or "fatal error:" in output or exit_code == 5:
            classification = "compilation"
        elif exit_code == 0 and "Verification complete" in output:
            classification = "verified-true"
        elif exit_code == 42 or "Verification unsuccessful" in output or "Verification unsuccesful" in output:
            classification = "verified-false"
        elif exit_code is not None and exit_code < 0:
            classification = f"signal-{-exit_code}"
        else:
            classification = "other"
        rows.append({
            "task": relative,
            "source": str(source_relative),
            "has_nondet": "__VERIFIER_nondet" in text,
            "has_abort_assume": "assume_abort_if_not" in text,
            "rewrite_rules": rewrite_rules,
            "exit_code": exit_code,
            "seconds": elapsed,
            "classification": classification,
            "log": log_name,
        })
        print(json.dumps(rows[-1], sort_keys=True))
    (args.output / "summary.json").write_text(
        json.dumps(rows, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
