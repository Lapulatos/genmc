#!/usr/bin/env python3
"""Aggregate preventive-checkpoint provenance from one BenchExec log archive."""

from __future__ import annotations

import argparse
import csv
import json
import re
import statistics
import zipfile
from pathlib import Path


KEYS = (
	"preventive-syncs",
	"preventive-retentions",
	"preventive-adaptive-retentions",
	"preventive-used-checkpoints",
	"preventive-adaptive-used",
	"preventive-rollback-hits",
	"preventive-unused-discards",
	"preventive-live-checkpoints",
	"preventive-unused-live",
	"max-history-base-bytes",
	"max-preventive-history-base-bytes",
	"retained-undo-bytes",
	"peak-undo-bytes",
	"retained-snapshot-equivalent-bytes",
	"peak-snapshot-equivalent-bytes",
	"offline-evals",
	"sync-ns",
	"history-ns",
	"checkpoint-ns",
	"rollback-ns",
)


def parse_archive(path: Path) -> list[dict[str, int | str]]:
	records: list[dict[str, int | str]] = []
	with zipfile.ZipFile(path) as archive:
		for member in archive.namelist():
			text = archive.read(member).decode(errors="replace")
			lines = [
				line
				for line in text.splitlines()
				if "CAT incremental statistics:" in line
			]
			if not lines:
				continue
			line = lines[-1]
			record: dict[str, int | str] = {"log": Path(member).name}
			for key in KEYS:
				match = re.search(rf"(?:^| ){re.escape(key)}=([0-9]+)", line)
				record[key] = int(match.group(1)) if match else 0
			records.append(record)
	return records


def main() -> None:
	parser = argparse.ArgumentParser()
	parser.add_argument("archive", type=Path)
	parser.add_argument("--output", type=Path, required=True)
	args = parser.parse_args()
	args.output.mkdir(parents=True, exist_ok=True)
	records = parse_archive(args.archive)
	if not records:
		raise RuntimeError("no completed CAT-stat records found")

	totals = {
		key: sum(int(record[key]) for record in records)
		for key in KEYS
		if not key.startswith("max-") and not key.startswith("peak-")
	}
	retentions = totals["preventive-retentions"]
	used = totals["preventive-used-checkpoints"]
	adaptive = totals["preventive-adaptive-retentions"]
	adaptive_used = totals["preventive-adaptive-used"]
	unused_by_lifetime = totals["preventive-unused-discards"] + totals["preventive-unused-live"]
	max_history = sum(int(record["max-history-base-bytes"]) for record in records)
	max_preventive = sum(
		int(record["max-preventive-history-base-bytes"]) for record in records
	)
	shares = [
		int(record["max-preventive-history-base-bytes"])
		/ int(record["max-history-base-bytes"])
		for record in records
		if int(record["max-history-base-bytes"]) > 0
	]
	summary = {
		"records": len(records),
		"totals": totals,
		"retention_identity_check": {
			"retentions_minus_unique_used": retentions - used,
			"unused_discarded_plus_unused_live": unused_by_lifetime,
			"matches": retentions - used == unused_by_lifetime,
		},
		"all_checkpoint_use_rate": used / retentions if retentions else 0,
		"adaptive_checkpoint_use_rate": adaptive_used / adaptive if adaptive else 0,
		"tasks_with_preventive_retention": sum(
			int(record["preventive-retentions"]) > 0 for record in records
		),
		"tasks_with_preventive_use": sum(
			int(record["preventive-used-checkpoints"]) > 0 for record in records
		),
		"tasks_with_adaptive_use": sum(
			int(record["preventive-adaptive-used"]) > 0 for record in records
		),
		"summed_max_history_base_bytes": max_history,
		"summed_max_preventive_history_base_bytes": max_preventive,
		"summed_preventive_history_share": max_preventive / max_history
		if max_history
		else 0,
		"median_task_preventive_history_share": statistics.median(shares) if shares else 0,
	}

	with (args.output / "records.tsv").open("w", newline="", encoding="utf-8") as sink:
		writer = csv.DictWriter(sink, fieldnames=["log", *KEYS], delimiter="\t")
		writer.writeheader()
		writer.writerows(records)
	(args.output / "summary.json").write_text(
		json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8"
	)
	decision = (
		"prototype suppression of adaptive-offline preventive retention"
		if adaptive and adaptive_used == 0
		else "retain adaptive-offline preventive checkpoints"
	)
	(args.output / "report.md").write_text(
		f"""# Preventive-history census

## Conclusion

Decision: **{decision}**. The archive contains {len(records)} completed CAT-stat logs.
Preventive synchronization retained {retentions:,} checkpoints; {used:,} distinct
checkpoints were later restored ({100 * summary['all_checkpoint_use_rate']:.3f}%).

Adaptive-offline preventive retention created {adaptive:,} checkpoints and later used
{adaptive_used:,} ({100 * summary['adaptive_checkpoint_use_rate']:.3f}%). Never-used
retentions computed from lifetime accounting are {unused_by_lifetime:,}; the identity
`retentions - unique_used == unused_discards + unused_live` is
{summary['retention_identity_check']['matches']}.

Summed per-task maximum preventive history is {max_preventive:,} of {max_history:,}
base bytes ({100 * summary['summed_preventive_history_share']:.2f}%).
""",
		encoding="utf-8",
	)


if __name__ == "__main__":
	main()
