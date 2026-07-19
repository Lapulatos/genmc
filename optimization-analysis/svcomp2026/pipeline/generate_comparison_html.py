#!/usr/bin/env python3
"""Generate one-column-per-method BenchExec comparison views."""

from __future__ import annotations

import argparse
import csv
from dataclasses import dataclass
import html
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import zipfile


@dataclass(frozen=True)
class View:
    name: str
    title: str
    note: str
    patterns: tuple[str, ...]
    run_sets: tuple[str, ...]


VIEWS = (
    View(
        "all-tools-methods-once",
        "全部工具与方法（每种一次）",
        "Navigation overview only: limits, hosts, memory models, and data models differ; use the family views for valid comparisons.",
        (
            "remote-results/sc/raw/*.results.genmc-sc.r01.unreach-call.xml.bz2",
            "remote-results/sc/raw/*.results.cat-sc.r01.unreach-call.xml.bz2",
            "remote-results/sc/raw/*.results.caat-sc.r01.unreach-call.xml.bz2",
            "remote-results/tso/raw/*.results.genmc-tso.r01.unreach-call.xml.bz2",
            "remote-results/tso/raw/*.results.cat-tso.r01.unreach-call.xml.bz2",
            "remote-results/tso/raw/*.results.caat-tso.r01.unreach-call.xml.bz2",
            "remote-results/pso/raw/*.results.cat-pso.r01.unreach-call.xml.bz2",
            "remote-results/pso/raw/*.results.caat-pso.r01.unreach-call.xml.bz2",
            "remote-results/new-server/trust-family/raw/*.xml.bz2",
            "remote-results/new-server/deagle/raw/*.xml.bz2",
            "remote-results/new-server/cbmc/raw/*.xml.bz2",
        ),
        (
            "paired-sc-r5.genmc-sc.r01.unreach-call",
            "paired-sc-r5.cat-sc.r01.unreach-call",
            "paired-sc-r5.caat-sc.r01.unreach-call",
            "paired-tso-r5.genmc-tso.r01.unreach-call",
            "paired-tso-r5.cat-tso.r01.unreach-call",
            "paired-tso-r5.caat-tso.r01.unreach-call",
            "paired-pso-r5.cat-pso.r01.unreach-call",
            "paired-pso-r5.caat-pso.r01.unreach-call",
            "awamoche-rc11-census-all.awamoche-rc11-lp64.r01.unreach-call",
            "mixer-rc11-census-all.mixer-rc11-lp64.r01.unreach-call",
            "spore-rc11-census-all.spore-rc11-lp64.r01.unreach-call",
            "trust-rc11-census-all.trust-rc11-lp64.r01.unreach-call",
            "deagle-ilp32-census.deagle.r01.unreach-call",
            "cbmc-ilp32-census-all.cbmc.r01.unreach-call",
        ),
    ),
    View(
        "methods-sc-once",
        "SC: GenMC / GenMC+CAT / GenMC+CAAT",
        "Formal 60 s paired performance run; r01 is shown once per method.",
        (
            "remote-results/sc/raw/*.results.genmc-sc.r01.unreach-call.xml.bz2",
            "remote-results/sc/raw/*.results.cat-sc.r01.unreach-call.xml.bz2",
            "remote-results/sc/raw/*.results.caat-sc.r01.unreach-call.xml.bz2",
        ),
        (
            "genmc-sc.r01.unreach-call",
            "cat-sc.r01.unreach-call",
            "caat-sc.r01.unreach-call",
        ),
    ),
    View(
        "methods-tso-once",
        "TSO: GenMC / GenMC+CAT / GenMC+CAAT",
        "Formal 60 s paired performance run; r01 is shown once per method.",
        (
            "remote-results/tso/raw/*.results.genmc-tso.r01.unreach-call.xml.bz2",
            "remote-results/tso/raw/*.results.cat-tso.r01.unreach-call.xml.bz2",
            "remote-results/tso/raw/*.results.caat-tso.r01.unreach-call.xml.bz2",
        ),
        (
            "genmc-tso.r01.unreach-call",
            "cat-tso.r01.unreach-call",
            "caat-tso.r01.unreach-call",
        ),
    ),
    View(
        "methods-pso-once",
        "PSO: GenMC+CAT / GenMC+CAAT",
        "Formal 60 s paired performance run. Native GenMC has no PSO backend.",
        (
            "remote-results/pso/raw/*.results.cat-pso.r01.unreach-call.xml.bz2",
            "remote-results/pso/raw/*.results.caat-pso.r01.unreach-call.xml.bz2",
        ),
        ("cat-pso.r01.unreach-call", "caat-pso.r01.unreach-call"),
    ),
    View(
        "tools-trust-family-once",
        "TruSt family: RC11 / LP64 compatibility census",
        "10 s compatibility census; semantics and LP64 data model differ from the ILP32 views.",
        ("remote-results/new-server/trust-family/raw/*.xml.bz2",),
        (
            "awamoche-rc11-census-all.awamoche-rc11-lp64.r01.unreach-call",
            "mixer-rc11-census-all.mixer-rc11-lp64.r01.unreach-call",
            "spore-rc11-census-all.spore-rc11-lp64.r01.unreach-call",
            "trust-rc11-census-all.trust-rc11-lp64.r01.unreach-call",
        ),
    ),
    View(
        "tools-svcomp-ilp32-once",
        "SV-COMP tools: Deagle / CBMC (ILP32)",
        "10 s compatibility census; use for support and verdict coverage, not cross-host timing ranks.",
        (
            "remote-results/new-server/deagle/raw/*.xml.bz2",
            "remote-results/new-server/cbmc/raw/*.xml.bz2",
        ),
        (
            "deagle-ilp32-census.deagle.r01.unreach-call",
            "cbmc-ilp32-census-all.cbmc.r01.unreach-call",
        ),
    ),
)


TASK_LINK_PREFIX = re.compile(
    r"\.\./\.\./\.\./remote-results/"
    r"(?:sc|tso|pso|new-server(?:/(?:trust-family|deagle|cbmc))?)/"
    r"sv-benchmarks/c/"
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "root",
        nargs="?",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="SV-COMP experiment root",
    )
    return parser.parse_args()


def inputs_for(root: Path, view: View) -> list[Path]:
    result: list[Path] = []
    for pattern in view.patterns:
        matches = sorted(root.glob(pattern))
        if not matches:
            raise SystemExit(f"no input matches {pattern!r}")
        result.extend(matches)
    if len(result) != len(view.run_sets):
        raise SystemExit(
            f"{view.name}: expected {len(view.run_sets)} XML files, found {len(result)}"
        )
    return result


def run_sets_from_csv(path: Path) -> tuple[str, ...]:
    with path.open(encoding="utf-8", newline="") as source:
        rows = list(csv.reader(source, delimiter="\t"))
    values: list[str] = []
    for value in rows[1][2:]:
        if value and (not values or value != values[-1]):
            values.append(value)
    return tuple(values)


def extract_log_archives(root: Path) -> None:
    for archive in root.glob("remote-results/**/*.logfiles.zip"):
        destination = (
            archive.parent.parent / "raw"
            if archive.parent.name == "logs"
            else archive.parent
        )
        destination.mkdir(parents=True, exist_ok=True)
        destination_root = destination.resolve()
        with zipfile.ZipFile(archive) as source:
            for member in source.infolist():
                target = (destination / member.filename).resolve()
                if not target.is_relative_to(destination_root):
                    raise SystemExit(f"unsafe ZIP member in {archive}: {member.filename}")
            source.extractall(destination)


def make_links_offline(html_path: Path) -> None:
    text = html_path.read_text(encoding="utf-8")
    text = TASK_LINK_PREFIX.sub(
        "../../../remote-results/sv-benchmarks/c/", text
    )
    html_path.write_text(text, encoding="utf-8")


def canonical_task_name(name: str) -> str:
    marker = "sv-benchmarks/c/"
    position = name.find(marker)
    if position < 0:
        raise SystemExit(f"cannot canonicalize task name: {name}")
    return name[position:]


def normalize_and_merge_rows(html_path: Path) -> tuple[int, int]:
    """Merge table rows that refer to the same shared SV-Benchmarks task."""
    text = html_path.read_text(encoding="utf-8")
    marker = '"rows": '
    start = text.find(marker)
    if start < 0:
        raise SystemExit(f"cannot find table rows in {html_path}")
    rows_start = start + len(marker)
    rows, consumed = json.JSONDecoder().raw_decode(text[rows_start:])
    merged: dict[tuple[str, ...], dict] = {}
    order: list[tuple[str, ...]] = []
    for row in rows:
        row_id = list(row["id"])
        row_id[0] = canonical_task_name(row_id[0])
        row["id"] = row_id
        key = tuple(row_id)
        if key not in merged:
            merged[key] = row
            order.append(key)
            continue
        target = merged[key]
        if len(target["results"]) != len(row["results"]):
            raise SystemExit(f"incompatible result widths for {key[0]}")
        for index, candidate in enumerate(row["results"]):
            existing = target["results"][index]
            candidate_empty = candidate.get("category") == "empty"
            existing_empty = existing.get("category") == "empty"
            if existing_empty and not candidate_empty:
                target["results"][index] = candidate
            elif not existing_empty and not candidate_empty and existing != candidate:
                raise SystemExit(
                    f"overlapping results while merging {key[0]} column {index}"
                )
    normalized = [merged[key] for key in order]
    replacement = json.dumps(normalized, ensure_ascii=False, separators=(",", ":"))
    end = rows_start + consumed
    html_path.write_text(text[:rows_start] + replacement + text[end:], encoding="utf-8")
    return len(rows), len(normalized)


def verify_local_links(html_path: Path) -> int:
    text = html_path.read_text(encoding="utf-8")
    links = re.findall(r'"href": "([^"]+\.(?:yml|log))"', text)
    missing = [
        link
        for link in links
        if not (html_path.parent / link).resolve().is_file()
    ]
    if missing:
        preview = "\n".join(missing[:10])
        raise SystemExit(
            f"{html_path}: {len(missing)}/{len(links)} offline links are missing:\n{preview}"
        )
    return len(links)


def main() -> int:
    args = parse_args()
    root = args.root.resolve()
    table_generator = shutil.which("table-generator")
    if not table_generator:
        raise SystemExit("table-generator is not installed")
    output_root = root / "benchexec-view" / "html"
    benchmark_snapshot = root / "remote-results" / "sv-benchmarks" / "c"
    if not benchmark_snapshot.is_dir():
        raise SystemExit(
            f"offline benchmark snapshot is missing: {benchmark_snapshot}"
        )
    extract_log_archives(root)
    env = os.environ.copy()
    env["PYTHONPATH"] = str(root) + (
        os.pathsep + env["PYTHONPATH"] if env.get("PYTHONPATH") else ""
    )

    merge_counts: tuple[int, int] | None = None
    for view in VIEWS:
        output = output_root / view.name
        if output.exists():
            shutil.rmtree(output)
        output.mkdir(parents=True)
        subprocess.run(
            [
                table_generator,
                "-q",
                "-f",
                "html",
                "-f",
                "csv",
                "-o",
                str(output),
                "-n",
                view.name,
                *(str(path) for path in inputs_for(root, view)),
            ],
            check=True,
            env=env,
        )
        table_csv = output / f"{view.name}.table.csv"
        actual = run_sets_from_csv(table_csv)
        if actual != view.run_sets:
            raise SystemExit(
                f"{view.name}: duplicate or reordered methods: {actual!r} != {view.run_sets!r}"
            )
        make_links_offline(output / f"{view.name}.table.html")
        if view.name == "all-tools-methods-once":
            merge_counts = normalize_and_merge_rows(
                output / f"{view.name}.table.html"
            )
        diff_html = output / f"{view.name}.diff.html"
        if diff_html.is_file():
            make_links_offline(diff_html)

    cards = "\n".join(
        f'''<li><a href="{html.escape(view.name)}/{html.escape(view.name)}.table.html">{html.escape(view.title)}</a><p>{html.escape(view.note)}</p></li>'''
        for view in VIEWS
    )
    (output_root / "comparison-index.html").write_text(
        f'''<!doctype html>
<html lang="zh-CN"><head><meta charset="utf-8"><title>SV-COMP 2026 对比结果</title>
<style>body{{font:16px/1.55 system-ui,sans-serif;max-width:980px;margin:48px auto;padding:0 24px;color:#18202a}}li{{margin:20px 0}}a{{font-size:1.1rem;font-weight:650}}p{{margin:4px 0;color:#53606f}}</style></head>
<body><h1>SV-COMP 2026 对比结果</h1>
<p>每个页面中每个方法只出现一次。性能统计仍使用完整五次重复；这里固定展示 r01 以避免重复列。</p>
<ul>{cards}</ul>
<p><strong>注意：</strong>不同主机、时间上限、数据模型或内存模型的页面不能直接用于时间排名。</p>
</body></html>''',
        encoding="utf-8",
    )
    verified = sum(
        verify_local_links(path)
        for path in output_root.glob("*-once/*.html")
    )
    print(output_root / "comparison-index.html")
    if merge_counts:
        print(f"normalized combined-table rows {merge_counts[0]} -> {merge_counts[1]}")
    print(f"verified {verified} offline task/log links")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
