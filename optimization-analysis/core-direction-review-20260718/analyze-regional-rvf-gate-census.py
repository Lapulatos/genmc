#!/usr/bin/env python3
"""Summarize transformed RVF gates and materialize the actual regional cohort."""

import argparse
import csv
import json
import re
import zipfile
from collections import Counter
from pathlib import Path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("result_dir", type=Path)
    parser.add_argument("--benchmark-root", required=True, type=Path)
    parser.add_argument(
        "--set-root", type=Path, default=Path("/workspace/svcomp2026-caat/sv-benchmarks")
    )
    args = parser.parse_args()

    archives = list(args.result_dir.glob("*.logfiles.zip"))
    if len(archives) != 1:
        parser.error(f"expected one log archive, found {len(archives)}")

    rows = []
    gates = Counter()
    with zipfile.ZipFile(archives[0]) as archive:
        for name in sorted(archive.namelist()):
            output = archive.read(name).decode(errors="replace")
            gate_match = re.search(r"SC RVF program gate: ([^\n]+)", output)
            command = output.splitlines()[0] if output else ""
            source_match = re.search(
                r"/workspace/experiments/caat-optimization/[^ ]+-rewrite/c/([^ ]+)\.[ci](?: |$)",
                command,
            )
            gate = gate_match.group(1).strip() if gate_match else "missing"
            source = source_match.group(1) if source_match else ""
            host_yaml = args.benchmark_root / "c" / f"{source}.yml" if source else None
            yaml = args.set_root / "c" / f"{source}.yml" if source else None
            yaml_exists = bool(host_yaml and host_yaml.is_file())
            rows.append(
                {
                    "log": Path(name).name,
                    "source": source,
                    "gate": gate,
                    "yaml": str(yaml) if yaml else "",
                    "yaml_exists": int(yaml_exists),
                }
            )
            gates[gate] += 1

    regional = [row for row in rows if row["gate"] == "regional"]
    missing_yaml = [row for row in regional if not row["yaml_exists"]]
    if missing_yaml:
        raise SystemExit(f"regional rows without an exact YAML mapping: {missing_yaml[:3]}")

    with (args.result_dir / "gate-census.tsv").open("w", newline="") as stream:
        writer = csv.DictWriter(stream, delimiter="\t", fieldnames=rows[0].keys())
        writer.writeheader()
        writer.writerows(rows)
    (args.result_dir / "regional-activation.set").write_text(
        "\n".join(row["yaml"] for row in regional) + "\n"
    )
    report = {
        "logs": len(rows),
        "gate_records": len(rows) - gates["missing"],
        "missing_gate_records": gates["missing"],
        "regional_tasks": len(regional),
        "enabled_tasks": gates["enabled"],
        "gate_counts": dict(gates.most_common()),
    }
    (args.result_dir / "analysis.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
