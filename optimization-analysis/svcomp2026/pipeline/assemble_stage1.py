#!/usr/bin/env python3
"""Assemble a full property-level stage-1 census without hiding unexecuted rows."""

from __future__ import annotations

import argparse
from collections import Counter
import csv
import json
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path)
    parser.add_argument("dynamic", type=Path, nargs="+")
    parser.add_argument(
        "--unsupported-property",
        action="append",
        default=[],
        help="property kind that the adapter cannot semantically check",
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--summary", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    with args.manifest.open(encoding="utf-8", newline="") as source:
        manifest = list(csv.DictReader(source, delimiter="\t"))
    dynamic = {}
    for dynamic_path in args.dynamic:
        with dynamic_path.open(encoding="utf-8", newline="") as source:
            for row in csv.DictReader(source, delimiter="\t"):
                key = (row["task_yaml"], row["property"])
                if key in dynamic:
                    raise SystemExit(f"duplicate dynamic result for {key}")
                dynamic[key] = row
    unsupported_properties = set(args.unsupported_property)

    output_rows = []
    classifications = Counter()
    properties = Counter()
    dynamic_yaml = set()
    all_yaml = {row["task_yaml"] for row in manifest}
    for row in manifest:
        key = (row["task_yaml"], row["property"])
        measured = dynamic.get(key)
        if measured:
            dynamic_yaml.add(row["task_yaml"])
            status = measured["status"]
            category = measured["category"]
            if category == "correct":
                classification = "executed_correct"
            elif category == "wrong":
                classification = "executed_wrong"
            elif status.startswith("TIMEOUT"):
                classification = "executed_timeout"
            elif status.startswith("OUT OF MEMORY"):
                classification = "executed_oom"
            elif status.startswith("ERROR (unsupported"):
                classification = "executed_unsupported_feature"
            elif status.startswith("ERROR (compilation"):
                classification = "executed_compilation_error"
            elif status == "ABORTED":
                classification = "executed_aborted"
            elif status == "SEGMENTATION FAULT":
                classification = "executed_crash"
            else:
                classification = "executed_other_error"
            dynamic_status = status
            dynamic_category = category
        elif row["property"] in unsupported_properties:
            classification = "property_adapter_unsupported"
            dynamic_status = ""
            dynamic_category = ""
        else:
            classification = "not_executed_property_adapter_unverified"
            dynamic_status = ""
            dynamic_category = ""
        classifications[classification] += 1
        properties[row["property"]] += 1
        output_rows.append(
            {
                **row,
                "stage1_classification": classification,
                "dynamic_status": dynamic_status,
                "dynamic_category": dynamic_category,
            }
        )

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=list(output_rows[0]), delimiter="\t")
        writer.writeheader()
        writer.writerows(output_rows)
    summary = args.summary or args.output.with_suffix(".summary.json")
    summary.write_text(
        json.dumps(
            {
                "property_rows": len(output_rows),
                "yaml_files": len(all_yaml),
                "dynamically_executed_yaml_files": len(dynamic_yaml),
                "not_dynamically_executed_yaml_files": len(all_yaml - dynamic_yaml),
                "by_property": dict(sorted(properties.items())),
                "by_stage1_classification": dict(sorted(classifications.items())),
            },
            indent=2,
            sort_keys=True,
        ) + "\n",
        encoding="utf-8",
    )
    print(f"assembled {len(output_rows)} property rows from {len(all_yaml)} YAML files")
    print(args.output)
    print(summary)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
