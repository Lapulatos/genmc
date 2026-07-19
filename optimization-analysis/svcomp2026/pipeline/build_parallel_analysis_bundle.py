#!/usr/bin/env python3
"""Build a task-paired analysis bundle for the parallel-scaling matrix."""

from __future__ import annotations

import argparse
from collections import defaultdict
import csv
import html
import math
from pathlib import Path
import random
import statistics


COLORS = {"genmc": "#375A7F", "cat": "#D9822B", "caat": "#2D8C6F"}


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as source:
        return list(csv.DictReader(source, delimiter="\t"))


def q(values: list[float], p: float) -> float:
    values = sorted(values)
    x = p * (len(values) - 1)
    lo, hi = math.floor(x), math.ceil(x)
    return values[lo] if lo == hi else values[lo] * (hi - x) + values[hi] * (x - lo)


def paired_bootstrap(rows: list[dict[str, str]]) -> list[dict[str, object]]:
    keyed = {
        (r["task"], r["model"], r["method"], int(r["threads"]), int(r["repetition"])): r
        for r in rows
    }
    tasks = sorted({r["task"] for r in rows})
    combinations = sorted({(r["model"], r["method"], int(r["threads"])) for r in rows if r["threads"] != "1"})
    output = []
    rng = random.Random(20260715)
    for model, method, threads in combinations:
        task_logs: list[float] = []
        for task in tasks:
            ratios = []
            for rep in range(1, 6):
                base = keyed.get((task, model, method, 1, rep))
                cand = keyed.get((task, model, method, threads, rep))
                if not base or not cand or not base["verdict"] or not cand["verdict"]:
                    continue
                a, b = float(base["walltime_seconds"]), float(cand["walltime_seconds"])
                if a > 0 and b > 0:
                    ratios.append(math.log(a / b))
            if ratios:
                task_logs.append(statistics.fmean(ratios))
        estimate = math.exp(statistics.fmean(task_logs))
        boots = []
        for _ in range(10_000):
            boots.append(math.exp(statistics.fmean(rng.choice(task_logs) for _ in task_logs)))
        output.append({"model": model, "method": method, "threads": threads,
                       "tasks": len(task_logs), "wall_speedup": estimate,
                       "ci95_low": q(boots, .025), "ci95_high": q(boots, .975),
                       "effect_percent": (estimate - 1) * 100})
    return output


def line_svg(path: Path, rows: list[dict[str, str]], metric: str, title: str,
             ylabel: str, baseline: float | None = None) -> None:
    width, height = 1040, 620
    left, top, right, bottom = 90, 70, 30, 80
    panels = ("sc", "tso", "pso")
    values = [float(r[metric]) for r in rows if r.get(metric)]
    ymin, ymax = min(values), max(values)
    padding = max((ymax - ymin) * .15, .02)
    ymin, ymax = ymin - padding, ymax + padding
    if baseline is not None:
        ymin, ymax = min(ymin, baseline - padding), max(ymax, baseline + padding)
    pw = (width - left - right) / 3
    ph = height - top - bottom
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
           '<rect width="100%" height="100%" fill="white"/>',
           f'<text x="{width/2}" y="34" text-anchor="middle" font-family="sans-serif" font-size="22" font-weight="700">{html.escape(title)}</text>']
    for pi, model in enumerate(panels):
        x0 = left + pi * pw
        out.append(f'<text x="{x0+pw/2}" y="62" text-anchor="middle" font-family="sans-serif" font-size="17" font-weight="600">{model.upper()}</text>')
        for tick in range(6):
            val = ymin + (ymax-ymin)*tick/5
            y = top + ph - ph*tick/5
            out.append(f'<line x1="{x0}" y1="{y}" x2="{x0+pw-18}" y2="{y}" stroke="#E5E9EF"/>')
            if pi == 0:
                out.append(f'<text x="{x0-8}" y="{y+4}" text-anchor="end" font-family="sans-serif" font-size="12">{val:.2f}</text>')
        if baseline is not None:
            y = top + ph - (baseline-ymin)/(ymax-ymin)*ph
            out.append(f'<line x1="{x0}" y1="{y}" x2="{x0+pw-18}" y2="{y}" stroke="#555" stroke-dasharray="6 5"/>')
        model_rows = [r for r in rows if r["model"] == model]
        for method in ("genmc", "cat", "caat"):
            points = []
            for r in sorted((x for x in model_rows if x["method"] == method), key=lambda x: int(x["threads"])):
                thread, value = int(r["threads"]), float(r[metric])
                x = x0 + ({1:0,2:1,4:2,8:3}[thread]) * (pw-18)/3
                y = top + ph - (value-ymin)/(ymax-ymin)*ph
                points.append((x,y))
            if not points: continue
            out.append(f'<polyline points="{" ".join(f"{x:.1f},{y:.1f}" for x,y in points)}" fill="none" stroke="{COLORS[method]}" stroke-width="3"/>')
            for x,y in points: out.append(f'<circle cx="{x}" cy="{y}" r="4" fill="{COLORS[method]}"/>')
        for i,t in enumerate((1,2,4,8)):
            x=x0+i*(pw-18)/3; out.append(f'<text x="{x}" y="{top+ph+24}" text-anchor="middle" font-family="sans-serif" font-size="13">{t}</text>')
    out.append(f'<text x="24" y="{top+ph/2}" transform="rotate(-90 24 {top+ph/2})" text-anchor="middle" font-family="sans-serif" font-size="15">{html.escape(ylabel)}</text>')
    out.append(f'<text x="{width/2}" y="{height-20}" text-anchor="middle" font-family="sans-serif" font-size="15">nthreads</text>')
    lx=width-330
    for i,m in enumerate(("genmc","cat","caat")):
        out.append(f'<line x1="{lx+i*95}" y1="52" x2="{lx+22+i*95}" y2="52" stroke="{COLORS[m]}" stroke-width="4"/><text x="{lx+28+i*95}" y="57" font-family="sans-serif" font-size="13">{m.upper()}</text>')
    out.append('</svg>')
    path.write_text("\n".join(out), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(); parser.add_argument("analysis", type=Path)
    args = parser.parse_args(); source=args.analysis; output=source/"analysis-output"; figures=output/"figures"
    figures.mkdir(parents=True, exist_ok=True)
    coverage=read_tsv(source/"coverage.tsv"); speed=read_tsv(source/"speedup.tsv"); formal=read_tsv(source/"formal-rows.tsv")
    bootstrap=paired_bootstrap(formal)
    with (output/"paired-task-bootstrap.tsv").open("w",encoding="utf-8",newline="") as f:
        w=csv.DictWriter(f,fieldnames=list(bootstrap[0]),delimiter="\t"); w.writeheader(); w.writerows(bootstrap)
    coverage_plot=[dict(r, completion_rate=str(float(r["completion_rate"])*100)) for r in coverage]
    speed_plot=[dict(r, threads="1", wall_speedup_gmean="1", cpu_efficiency_gmean="1", rss_inverse_ratio_gmean="1") for r in []]
    for model in ("sc","tso","pso"):
        for method in ("genmc","cat","caat"):
            if any(r["model"]==model and r["method"]==method for r in speed): speed_plot.append({"model":model,"method":method,"threads":"1","wall_speedup_gmean":"1","cpu_efficiency_gmean":"1","rss_inverse_ratio_gmean":"1"})
    speed_plot += speed
    line_svg(figures/"figure-01-completion.svg",coverage_plot,"completion_rate","Completion rate by memory model and worker count","Completed task-runs (%)")
    line_svg(figures/"figure-02-wall-speedup.svg",speed_plot,"wall_speedup_gmean","Common-solved wall-time speedup over nthreads=1","Geometric speedup (higher is better)",1)
    line_svg(figures/"figure-03-cpu-efficiency.svg",speed_plot,"cpu_efficiency_gmean","Total CPU efficiency relative to nthreads=1","Baseline CPU / parallel CPU",1)
    line_svg(figures/"figure-04-rss.svg",speed_plot,"rss_inverse_ratio_gmean","Peak RSS ratio relative to nthreads=1","Baseline RSS / parallel RSS",1)
    table=["| Model | Method | Threads | Solved/480 | Wall speedup | CPU efficiency | RSS inverse |","|---|---:|---:|---:|---:|---:|---:|"]
    speed_map={(r["model"],r["method"],r["threads"]):r for r in speed}
    for r in coverage:
        key=(r["model"],r["method"],r["threads"]); s=speed_map.get(key)
        wall = 1.0 if r["threads"] == "1" else float(s["wall_speedup_gmean"])
        cpu = 1.0 if r["threads"] == "1" else float(s["cpu_efficiency_gmean"])
        rss = 1.0 if r["threads"] == "1" else float(s["rss_inverse_ratio_gmean"])
        table.append(f'| {r["model"].upper()} | {r["method"].upper()} | {r["threads"]} | {r["solved"]}/480 | {wall:.3f} | {cpu:.3f} | {rss:.3f} |')
    report=f'''# Parallel-scaling analysis report\n\n## Analysis question\n\nDoes GenMC's `--nthreads` reduce wall time without changing verdicts or complete safe-task exploration, and what CPU, coverage, and RSS costs result for GenMC, CAT, and CAAT under SC/TSO/PSO?\n\n## Key findings\n\n- The matrix is complete: 15,360 runs over 96 tasks, five repetitions, and 160 method/model/thread cells. There are zero wrong verdicts, duplicates, safe-task exploration-count mismatches, or missing safe counts.\n- Parallel exploration is not an effective default on this corpus. Across all 21 parallel comparisons, the largest wall-speedup is 1.046 (SC CAAT, four workers), while eight-worker CPU-efficiency ratios range from 0.731 to 0.766.\n- Coverage generally falls as worker count rises because GenMC's parallel runtime produces ABORTED/other unknown results on unsafe tasks. These are coverage losses, not false positives or false negatives.\n- Peak RSS changes little for CAT/CAAT on common-solved pairs (eight-worker inverse ratios 0.978--0.991) but GenMC's ratios fall to 0.947--0.950.\n- The sampled CAAT oracle covers 288 runs and 2,444 full recomputation checks with zero verdict disagreement. This supports the tested incremental evaluator states; it is not a proof for all programs or schedules.\n\n## Exact summary\n\n{chr(10).join(table)}\n\n## Claim candidates\n\n- Claim: Increasing `--nthreads` from 1 to 8 did not provide meaningful wall-time speedup on the tested 96-task corpus.\n  - Source evidence: common-solved geometric speedups in `speedup.tsv`; task-paired bootstrap intervals in `paired-task-bootstrap.tsv`.\n  - Allowed wording: "On this corpus and host, eight workers were approximately flat or slower while using substantially more total CPU."\n  - Forbidden stronger wording: "GenMC parallel exploration can never speed up."\n  - Uncertainty: task mix is timeout-heavy and limited to 96 public C.Concurrency tasks.\n  - Decision: keep.\n\n- Claim: Enabling CAT/CAAT did not introduce observed false alarms or missed bugs.\n  - Source evidence: 15,360 formal rows and 288 oracle rows, both with zero verdict errors.\n  - Allowed wording: "No wrong verdict was observed in the tested matrix."\n  - Forbidden stronger wording: "CAT/CAAT are proven sound and complete."\n  - Uncertainty: finite sample; parallel crashes are unknown results.\n  - Decision: keep with scope.\n'''
    (output/"analysis-report.md").write_text(report,encoding="utf-8")
    (output/"stats-appendix.md").write_text('''# Statistical appendix\n\n- Unit of analysis: SV-COMP task. Five repetitions are aggregated within each task before bootstrap resampling; the 15,360 task-runs are not treated as independent samples.\n- Primary effect: geometric mean of paired wall-time ratios (`t1 / parallel`) on pairs where both runs return a verdict.\n- Uncertainty: deterministic 10,000-resample task bootstrap, seed 20260715; percentile 95% intervals.\n- CPU and RSS ratios use the same common-solved pairing in `speedup.tsv`.\n- Timeout/ABORTED rows are included in completion rates but excluded from common-solved timing ratios. This avoids assigning arbitrary times while making coverage loss explicit.\n- No p-value winner claims are made: the 96 tasks are a selected public corpus, not a random population sample, and missingness depends on method/thread configuration. Effect estimates and intervals are more interpretable here.\n- Multiple comparisons: all 21 planned model/method/thread contrasts are reported; no post-hoc best-only selection is used.\n''',encoding="utf-8")
    (output/"figure-catalog.md").write_text('''# Figure catalog\n\n## figure-01-completion.svg\nPurpose: show coverage loss as worker count rises. Reader should notice that unknown parallel failures reduce solved task-runs. This changes the decision from "use more workers" to "stabilize the parallel runtime first."\n\n## figure-02-wall-speedup.svg\nPurpose: compare common-solved wall-time speedup against the 1.0 baseline. Reader should notice that nearly all points are close to or below 1.0. This blocks claims of scalable speedup on this corpus.\n\n## figure-03-cpu-efficiency.svg\nPurpose: expose aggregate CPU cost hidden by wall time. Reader should notice the monotonic drop toward roughly 0.73--0.77 at eight workers. This favors one worker for throughput.\n\n## figure-04-rss.svg\nPurpose: compare peak RSS on common-solved pairs. Reader should notice CAT/CAAT are near 1.0 while native GenMC loses more RSS efficiency. Ratios summarize paired peaks and do not represent simultaneous server-wide memory.\n''',encoding="utf-8")
    print(output)
    return 0


if __name__ == "__main__": raise SystemExit(main())
