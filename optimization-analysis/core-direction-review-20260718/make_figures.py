#!/usr/bin/env python3
"""Generate decision figures from frozen local evidence tables."""

from __future__ import annotations

import csv
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


ROOT = Path(__file__).resolve().parent
FIGURES = ROOT / "figures"
FIGURES.mkdir(exist_ok=True)


with (ROOT / "evidence.tsv").open() as handle:
    evidence = list(csv.DictReader(handle, delimiter="\t"))
measured = [row for row in evidence if row["cpu_ratio"]]
labels = [row["family"] for row in measured]
ratios = np.array([float(row["cpu_ratio"]) for row in measured])
low = np.array([float(row["ci_low"]) for row in measured])
high = np.array([float(row["ci_high"]) for row in measured])
colors = ["#15803d" if row["search_space_changed"] == "yes" else "#64748b"
          for row in measured]

fig, ax = plt.subplots(figsize=(9.2, 4.8))
y = np.arange(len(labels))
for index, color in enumerate(colors):
    ax.errorbar(ratios[index], y[index],
                xerr=[[ratios[index] - low[index]], [high[index] - ratios[index]]],
                fmt="none", ecolor=color, elinewidth=2, capsize=4)
ax.scatter(ratios, y, c=colors, s=58, zorder=3)
ax.axvline(1.0, color="#991b1b", linestyle="--", linewidth=1.2)
ax.set_yticks(y, labels)
ax.invert_yaxis()
ax.set_xlabel("CPU ratio (candidate / baseline; lower is better)")
ax.set_title("End-to-end evidence: search-space reduction vs evaluator-only changes")
ax.grid(axis="x", alpha=0.22)
fig.tight_layout()
fig.savefig(FIGURES / "figure-01-mechanism-cpu.pdf")
fig.savefig(FIGURES / "figure-01-mechanism-cpu.png", dpi=180)
plt.close(fig)


with (ROOT / "finite-rvf-sc-order-evidence.tsv").open() as handle:
    sc_order = list(csv.DictReader(handle, delimiter="\t"))
control, candidate = sc_order
resource_names = ["CPU", "Wall", "RSS"]
resource_fields = ["cpu_seconds", "wall_seconds", "rss_sum_bytes"]
resource_ratios = [float(candidate[field]) / float(control[field])
                   for field in resource_fields]

fig, (resource_ax, coverage_ax) = plt.subplots(1, 2, figsize=(10.2, 4.5))
bars = resource_ax.bar(resource_names, resource_ratios,
                       color=["#2563eb", "#0f766e", "#7c3aed"])
resource_ax.axhline(1.0, color="#991b1b", linestyle="--", linewidth=1.2)
resource_ax.set_ylim(0, 1.08)
resource_ax.set_ylabel("Candidate / exact concrete control")
resource_ax.set_title("Resources (lower is better)")
resource_ax.grid(axis="y", alpha=0.2)
for bar, ratio in zip(bars, resource_ratios):
    resource_ax.text(bar.get_x() + bar.get_width() / 2, ratio + 0.025,
                     f"{ratio:.3f}", ha="center", va="bottom")

modes = [row["mode"] for row in sc_order]
sat = np.array([int(row["sat"]) for row in sc_order])
unsat = np.array([int(row["unsat"]) for row in sc_order])
unknown = np.array([int(row["unknown"]) for row in sc_order])
x = np.arange(len(modes))
coverage_ax.bar(x, sat, label="SAT", color="#b91c1c")
coverage_ax.bar(x, unsat, bottom=sat, label="UNSAT", color="#15803d")
coverage_ax.bar(x, unknown, bottom=sat + unsat, label="Unknown", color="#94a3b8")
coverage_ax.set_xticks(x, ["Concrete RF\n+ exact SC", "Value class\n+ SC order"])
coverage_ax.set_ylabel("Tasks (283 total)")
coverage_ax.set_title("Terminal coverage")
coverage_ax.legend(frameon=False, ncol=3, loc="upper center")
coverage_ax.grid(axis="y", alpha=0.2)
for index, row in enumerate(sc_order):
    coverage_ax.text(index, int(row["classified"]) + 4,
                     f"{row['classified']} classified", ha="center", va="bottom")

fig.suptitle("Finite SC RVF quotient: exact-control broad gate")
fig.tight_layout()
fig.savefig(FIGURES / "figure-05-rvf-sc-order-broad-gate.pdf")
fig.savefig(FIGURES / "figure-05-rvf-sc-order-broad-gate.png", dpi=180)
plt.close(fig)


with (ROOT / "opportunity.tsv").open() as handle:
    opportunity = list(csv.DictReader(handle, delimiter="\t"))
models = [row["model"] for row in opportunity]
offered = np.array([int(row["rf_offered"]) for row in opportunity])
same = np.array([int(row["same_value_upper_bound"]) for row in opportunity])
realized = np.array([int(row["realized_prefixes"]) for row in opportunity])
inconsistent = np.array([int(row["inconsistent_prefixes"]) for row in opportunity])
x = np.arange(len(models))
width = 0.19
fig, ax = plt.subplots(figsize=(9.2, 4.9))
for offset, values, label, color in (
    (-1.5 * width, offered, "RF offered", "#1d4ed8"),
    (-0.5 * width, same, "same-value upper bound", "#0f766e"),
    (0.5 * width, realized, "realized revisit prefixes", "#7c3aed"),
    (1.5 * width, inconsistent, "post-generation inconsistent", "#b91c1c"),
):
    ax.bar(x + offset, values, width, label=label, color=color)
ax.set_yscale("log")
ax.set_xticks(x, models)
ax.set_ylabel("Count (log scale)")
ax.set_title("725-task search funnel: opportunity exists before candidate generation")
ax.legend(frameon=False, ncol=2)
ax.grid(axis="y", which="both", alpha=0.2)
fig.tight_layout()
fig.savefig(FIGURES / "figure-02-search-funnel.pdf")
fig.savefig(FIGURES / "figure-02-search-funnel.png", dpi=180)
plt.close(fig)
