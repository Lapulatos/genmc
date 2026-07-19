#!/usr/bin/env python3
"""Generate offline BenchExec views for the parallel-scaling experiment."""

from __future__ import annotations

import argparse
import csv
import html
import os
from pathlib import Path
import re
import shutil
import subprocess
import zipfile


MODELS = ("sc", "tso", "pso")
THREADS = (1, 2, 4, 8)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path, help="downloaded experiment root")
    return parser.parse_args()


def methods(model: str) -> tuple[str, ...]:
    return ("genmc", "cat", "caat") if model != "pso" else ("cat", "caat")


def run_sets(path: Path) -> tuple[str, ...]:
    with path.open(encoding="utf-8", newline="") as source:
        rows = list(csv.reader(source, delimiter="\t"))
    values: list[str] = []
    for value in rows[1][2:]:
        if value and (not values or value != values[-1]):
            values.append(value)
    return tuple(values)


def extract_logs(root: Path) -> None:
    destination = root / "raw"
    destination.mkdir(exist_ok=True)
    destination_root = destination.resolve()
    for archive in root.glob("raw/*.logfiles.zip"):
        with zipfile.ZipFile(archive) as source:
            for member in source.infolist():
                target = (destination / member.filename).resolve()
                if not target.is_relative_to(destination_root):
                    raise SystemExit(f"unsafe ZIP member in {archive}: {member.filename}")
            source.extractall(destination)


def verify_links(path: Path) -> int:
    text = path.read_text(encoding="utf-8")
    links = re.findall(r'"href": "([^"]+\.(?:yml|log))"', text)
    missing = [link for link in links if not (path.parent / link).resolve().is_file()]
    if missing:
        raise SystemExit(f"{path}: {len(missing)}/{len(links)} local links missing")
    return len(links)


def make_task_links_self_contained(path: Path) -> None:
    text = path.read_text(encoding="utf-8")
    path.write_text(
        text.replace('"../../../sv-benchmarks/', '"../../sv-benchmarks/'),
        encoding="utf-8",
    )


def main() -> int:
    root = parse_args().root.resolve()
    generator = shutil.which("table-generator")
    if not generator:
        raise SystemExit("table-generator is not installed")
    extract_logs(root)
    output_root = root / "html"
    if output_root.exists():
        shutil.rmtree(output_root)
    output_root.mkdir(parents=True)
    env = os.environ.copy()
    pipeline_root = Path(__file__).resolve().parents[1]
    env["PYTHONPATH"] = str(pipeline_root) + (
        os.pathsep + env["PYTHONPATH"] if env.get("PYTHONPATH") else ""
    )
    method_cards: list[str] = []
    scaling_cards: list[str] = []
    checked_links = 0
    for model in MODELS:
        for nthreads in THREADS:
            view = f"{model}-t{nthreads}-methods-once"
            inputs = [
                next(root.glob(
                    f"raw/*.results.{method}-{model}.t{nthreads}.r01.unreach-call.xml.bz2"
                ))
                for method in methods(model)
            ]
            destination = output_root / view
            destination.mkdir()
            subprocess.run(
                [generator, "-q", "-f", "html", "-f", "csv", "-o",
                 str(destination), "-n", view, *(str(path) for path in inputs)],
                check=True,
                env=env,
            )
            expected = tuple(
                f"{method}-{model}.t{nthreads}.r01.unreach-call"
                for method in methods(model)
            )
            actual = run_sets(destination / f"{view}.table.csv")
            if actual != expected:
                raise SystemExit(f"{view}: columns {actual!r}, expected {expected!r}")
            table = destination / f"{view}.table.html"
            make_task_links_self_contained(table)
            checked_links += verify_links(table)
            method_cards.append(
                f'<li><a href="{view}/{view}.table.html">'
                f'{model.upper()}, nthreads={nthreads}</a>'
                f'<p>{len(expected)} methods; formal repetition r01.</p></li>'
            )
    for model in MODELS:
        for method in methods(model):
            view = f"{model}-{method}-threads-1-2-4-8"
            inputs = [
                next(root.glob(
                    f"raw/*.results.{method}-{model}.t{nthreads}.r01.unreach-call.xml.bz2"
                ))
                for nthreads in THREADS
            ]
            destination = output_root / view
            destination.mkdir()
            subprocess.run(
                [generator, "-q", "-f", "html", "-f", "csv", "-o",
                 str(destination), "-n", view, *(str(path) for path in inputs)],
                check=True,
                env=env,
            )
            expected = tuple(
                f"parallel-{model}-t{nthreads}-r5."
                f"{method}-{model}.t{nthreads}.r01.unreach-call"
                for nthreads in THREADS
            )
            actual = run_sets(destination / f"{view}.table.csv")
            if actual != expected:
                raise SystemExit(f"{view}: columns {actual!r}, expected {expected!r}")
            table = destination / f"{view}.table.html"
            make_task_links_self_contained(table)
            checked_links += verify_links(table)
            scaling_cards.append(
                f'<li><a href="{view}/{view}.table.html">'
                f'{model.upper()} — {method.upper()}</a>'
                '<p>nthreads=1, 2, 4, 8; formal repetition r01.</p></li>'
            )
    (output_root / "parallel-comparison-index.html").write_text(
        "<!doctype html><html lang=\"zh-CN\"><head><meta charset=\"utf-8\">"
        "<title>GenMC 并行扩展对比</title><style>"
        "body{font:16px/1.55 system-ui,sans-serif;max-width:920px;margin:48px auto;"
        "padding:0 24px;color:#18202a}li{margin:18px 0}a{font-weight:650}"
        "p{margin:3px 0;color:#53606f}</style></head><body>"
        "<h1>GenMC 并行扩展对比</h1>"
        "<p>网页固定展示 r01；完整统计使用全部五次重复。</p>"
        "<h2>同一方法的线程数对比</h2>"
        "<p>每页横向比较同一方法在 nthreads=1、2、4、8 下的结果。</p>"
        f"<ul>{''.join(scaling_cards)}</ul>"
        "<h2>同一线程数的不同方法对比</h2>"
        "<p>每页每种方法只出现一次。</p>"
        f"<ul>{''.join(method_cards)}</ul></body></html>",
        encoding="utf-8",
    )
    print(output_root / "parallel-comparison-index.html")
    print(f"verified {checked_links} local task/log links")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
