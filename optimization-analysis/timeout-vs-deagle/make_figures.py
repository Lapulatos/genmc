#!/usr/bin/env python3
"""Create dependency-free SVG figures for the timeout cohort."""

from __future__ import annotations

import csv
import html
import math
from collections import Counter, defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parent
ROWS = ROOT / "analysis-output/primary-cohort.tsv"
OUT = ROOT / "analysis-output/figures"
FONT = "font-family='Arial,Helvetica,sans-serif'"


def load_rows() -> list[dict[str, str]]:
    with ROWS.open(newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def write_svg(stem: str, width: int, height: int, body: list[str]) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    svg = [
        f"<svg xmlns='http://www.w3.org/2000/svg' width='{width}' height='{height}' viewBox='0 0 {width} {height}'>",
        "<rect width='100%' height='100%' fill='white'/>",
        *body,
        "</svg>",
    ]
    (OUT / f"{stem}.svg").write_text("\n".join(svg) + "\n")


def text(x: float, y: float, value: str, size: int = 13, anchor: str = "start", weight: str = "normal") -> str:
    return (
        f"<text x='{x:.1f}' y='{y:.1f}' font-size='{size}' text-anchor='{anchor}' "
        f"font-weight='{weight}' fill='#222' {FONT}>{html.escape(value)}</text>"
    )


def family_figure(rows: list[dict[str, str]]) -> None:
    values: dict[str, Counter] = defaultdict(Counter)
    for row in rows:
        values[row["family"]][row["expected"]] += 1
    families = sorted(values, key=lambda family: sum(values[family].values()), reverse=True)
    width, height = 860, 370
    left, top, plot_w, row_h = 160, 70, 620, 38
    scale = plot_w / max(sum(values[f].values()) for f in families)
    body = [text(width / 2, 30, "GenMC+CAAT-SC TIMEOUT, Deagle correct (n=200)", 18, "middle", "bold")]
    for index, family in enumerate(families):
        y = top + index * row_h
        false = values[family]["false"]
        true = values[family]["true"]
        body.append(text(left - 10, y + 17, family, 12, "end"))
        body.append(f"<rect x='{left}' y='{y}' width='{false * scale:.1f}' height='22' fill='#C44E52'/>")
        body.append(f"<rect x='{left + false * scale:.1f}' y='{y}' width='{true * scale:.1f}' height='22' fill='#4C72B0'/>")
        body.append(text(left + (false + true) * scale + 7, y + 17, str(false + true), 12))
    body.extend([
        f"<rect x='260' y='328' width='14' height='14' fill='#C44E52'/>", text(280, 340, "expected FALSE", 12),
        f"<rect x='430' y='328' width='14' height='14' fill='#4C72B0'/>", text(450, 340, "expected TRUE", 12),
    ])
    write_svg("01-family-composition", width, height, body)


def evidence_figure(rows: list[dict[str, str]]) -> None:
    categories = [
        ("nondet semantics differ", sum(r["has_nondet"] == "True" for r in rows), "#8172B3"),
        (
            "bounded TRUE caveat only",
            sum(r["has_nondet"] != "True" and r["deagle_true_incomplete_bound"] == "True" for r in rows),
            "#DD8452",
        ),
        ("strictly comparable", sum(r["strict_semantics_comparable"] == "True" for r in rows), "#55A868"),
    ]
    width, height = 860, 260
    left, top, plot_w = 220, 70, 570
    scale = plot_w / 200
    body = [text(width / 2, 30, "Evidence classes are not all fair performance pairs", 18, "middle", "bold")]
    for index, (label, count, color) in enumerate(categories):
        y = top + index * 52
        body.append(text(left - 12, y + 18, label, 13, "end"))
        body.append(f"<rect x='{left}' y='{y}' width='{count * scale:.1f}' height='24' fill='{color}'/>")
        body.append(text(left + count * scale + 8, y + 18, str(count), 13, weight="bold"))
    write_svg("02-evidence-classes", width, height, body)


def runtime_figure(rows: list[dict[str, str]]) -> None:
    groups = [
        ("nondet semantics differ", lambda r: r["has_nondet"] == "True", "#8172B3"),
        (
            "bounded TRUE caveat only",
            lambda r: r["has_nondet"] != "True" and r["deagle_true_incomplete_bound"] == "True",
            "#DD8452",
        ),
        ("strictly comparable", lambda r: r["strict_semantics_comparable"] == "True", "#55A868"),
    ]
    width, height = 860, 460
    left, top, plot_w, plot_h = 85, 65, 700, 310
    xmin, xmax = math.log10(.2), math.log10(60)
    ymin, ymax = 24, 115
    xmap = lambda value: left + (math.log10(value) - xmin) / (xmax - xmin) * plot_w
    ymap = lambda value: top + plot_h - (value - ymin) / (ymax - ymin) * plot_h
    body = [
        text(width / 2, 28, "Timeout cohort: mostly low-memory exploration pressure", 18, "middle", "bold"),
        f"<line x1='{left}' y1='{top + plot_h}' x2='{left + plot_w}' y2='{top + plot_h}' stroke='#333'/>",
        f"<line x1='{left}' y1='{top}' x2='{left}' y2='{top + plot_h}' stroke='#333'/>",
    ]
    for tick in [.2, 1, 10, 60]:
        x = xmap(tick)
        body.extend([f"<line x1='{x:.1f}' y1='{top + plot_h}' x2='{x:.1f}' y2='{top + plot_h + 5}' stroke='#333'/>", text(x, top + plot_h + 22, str(tick), 11, "middle")])
    for tick in [25, 50, 75, 100]:
        y = ymap(tick)
        body.extend([f"<line x1='{left - 5}' y1='{y:.1f}' x2='{left}' y2='{y:.1f}' stroke='#333'/>", text(left - 10, y + 4, str(tick), 11, "end")])
    for label, predicate, color in groups:
        group = [row for row in rows if predicate(row)]
        for row in group:
            x = xmap(float(row["deagle_cpu_s"]))
            y = ymap(float(row["caat_sc_rss_mb"]))
            body.append(f"<circle cx='{x:.1f}' cy='{y:.1f}' r='4' fill='{color}' fill-opacity='.72'/>")
    body.extend([
        text(left + plot_w / 2, height - 42, "Deagle CPU time (s, log scale)", 13, "middle"),
        f"<text x='18' y='{top + plot_h / 2}' transform='rotate(-90 18 {top + plot_h / 2})' font-size='13' text-anchor='middle' fill='#222' {FONT}>GenMC+CAAT-SC peak RSS (MB)</text>",
    ])
    for index, (label, predicate, color) in enumerate(groups):
        y = 402 + index * 18
        count = sum(predicate(row) for row in rows)
        body.extend([f"<circle cx='475' cy='{y - 4}' r='4' fill='{color}'/>", text(486, y, f"{label} (n={count})", 11)])
    write_svg("03-runtime-memory-evidence", width, height, body)


def main() -> None:
    rows = load_rows()
    family_figure(rows)
    evidence_figure(rows)
    runtime_figure(rows)


if __name__ == "__main__":
    main()
