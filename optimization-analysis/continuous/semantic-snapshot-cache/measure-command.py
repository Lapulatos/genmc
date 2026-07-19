#!/usr/bin/env python3
"""Measure one child without depending on GNU time inside the experiment image."""

from __future__ import annotations

import resource
import subprocess
import sys
import time


def main() -> int:
    timing, stdout_path, stderr_path, separator, *command = sys.argv[1:]
    if separator != "--" or not command:
        raise SystemExit("usage: measure-command.py TIMING STDOUT STDERR -- COMMAND...")
    started = time.monotonic()
    with open(stdout_path, "wb") as stdout, open(stderr_path, "wb") as stderr:
        completed = subprocess.run(command, stdout=stdout, stderr=stderr, check=False)
    elapsed = time.monotonic() - started
    usage = resource.getrusage(resource.RUSAGE_CHILDREN)
    with open(timing, "w", encoding="utf-8") as output:
        output.write(
            f"{elapsed:.9f}\t{usage.ru_utime:.9f}\t{usage.ru_stime:.9f}"
            f"\t{usage.ru_maxrss}\t{completed.returncode}\n"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
