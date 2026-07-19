#!/usr/bin/env python3
"""Prepare a server-local fair-coverage BenchExec definition and task set."""

from __future__ import annotations

import argparse
from pathlib import Path


DEFINITION = """<?xml version="1.0"?>
<!DOCTYPE benchmark PUBLIC "+//IDN sosy-lab.org//DTD BenchExec benchmark 3.0//EN" "https://www.sosy-lab.org/benchexec/benchmark-3.0.dtd">
<benchmark tool="tools.genmc_svcomp_fair" timelimit="{timelimit} s" memlimit="4 GB" cpuCores="1">
  <option name="--disable-estimation"/>
  <option name="--disable-mm-detector"/>
  <option name="--nthreads=1"/>
  <option name="--v1"/>
  <column title="executions">Number of complete executions explored: (.*)</column>
  <column title="blocked">Number of blocked executions seen: (.*)</column>
  <rundefinition name="genmc-fair-sc">
    <option name="--svcomp-backend=genmc-sc"/>
    <tasks name="unreach-call">
      <includesfile>{task_set}</includesfile>
      <propertyfile>{property_file}</propertyfile>
    </tasks>
  </rundefinition>
</benchmark>
"""


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("original_set", type=Path)
    parser.add_argument("benchmark_root", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--timelimit", type=int, default=10)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    tasks = []
    for raw in args.original_set.read_text(encoding="utf-8").splitlines():
        if not raw.strip():
            continue
        marker = "/sv-benchmarks/"
        if marker not in raw:
            raise ValueError(f"cannot map task path: {raw}")
        tasks.append(str(args.benchmark_root / raw.split(marker, 1)[1]))
    definitions = []
    for shard_index in range(2):
        shard_tasks = tasks[shard_index::2]
        task_set = args.output / f"fair-census-shard{shard_index + 1}.set"
        task_set.write_text("\n".join(shard_tasks) + "\n", encoding="utf-8")
        definition = DEFINITION.format(
            task_set=task_set,
            property_file=args.benchmark_root / "c/properties/unreach-call.prp",
            timelimit=args.timelimit,
        )
        definition_path = args.output / f"fair-census-shard{shard_index + 1}.xml"
        definition_path.write_text(definition, encoding="utf-8")
        definitions.append(definition_path)
        print(f"shard{shard_index + 1}_tasks={len(shard_tasks)}")
    print(f"tasks={len(tasks)}")
    print(f"definitions={','.join(map(str, definitions))}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
