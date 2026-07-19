# Exact successor-cursor cycle-check report (2026-07-19)

`--cat-fast-cycle-checks` preserves DFS root order, successor order, color, parent, and closed
witness construction while skipping absent adjacency entries through `nextSuccessor()`.

Correctness passes local/server Release (225 + one expected skip), server ASan+UBSan 45/45,
mutation 39 rows / 5,441 independent old-path oracle checks, and broad 852 matches / 12 mutually
unsupported / zero mismatch over 864 pairs.

On fixed-15, baseline/candidate all-task CPU is 475.920/415.295 s (-12.74%), completed CPU
170.957/110.293 s (-35.49%), suite wall 138.93/132.45 s (-4.66%), check 22.090/5.063 s
(-77.08%), and offline evaluation 53.453/8.953 s (-83.25%). Aggregate RSS changes +0.04% and
peak RSS -0.20%. Terminal/TIMEOUT remains 10/5 and search counts match exactly at
18,693 / 197,703 / 0.

The authorized 283-task paired run reduces TIMEOUT from 183 to 175. All eight rescued tasks end
with BenchExec-correct `false(unreach-call)` verdicts; there is no reverse timeout or terminal
verdict mismatch. Common-terminal CPU is 642.146/436.096 s (-32.09%), all-task CPU
11,798.724/11,480.420 s (-2.70%), suite wall 527.46/509.59 s (-3.39%), aggregate RSS -0.02%, and
peak RSS -0.12%. Candidate completed CPU is larger only because it performs 374.767 s of useful
work on the eight newly completed tasks.

The hard-TIMEOUT reduction authorizes the actual 725-task gate. Raw evidence is under
`server-results/fast-cycle-checks-20260719a/`,
`server-results/primitive-fast-compose-cycle-paired-15-20260719a/`, and
`server-results/primitive-fast-compose-cycle-paired-283-20260719a/`.

The actual simultaneous 725-task run confirms a useful but operationally mixed result. Correct
terminal results increase from 394 to 402 and total hard resource failures (TIMEOUT + OOM) fall
from 275 to 267. TIMEOUT falls 244/216 while OOM rises 31/51: the 20 new OOMs are all Goblint
`28-race_reach_*` tasks that previously timed out, so they remain unresolved rather than becoming
correct results. Eight pthread-wmm tasks move from TIMEOUT to BenchExec-correct
`false(unreach-call)`. There is no terminal-to-resource regression and no terminal-verdict
mismatch among the 28 status changes.

Across the 450 tasks terminal in both lanes, CPU is 1,033.224/745.272 s (-27.87%). All-task CPU
is 16,366.595/15,632.316 s (-4.49%) and suite wall is 717.88/690.74 s (-3.78%). Aggregate RSS
rises 0.92%, primarily because the 20 faster Goblint explorations reach the unchanged 4-GB limit;
maximum RSS remains 3,999,997,952 bytes. Candidate CAT consistency is 317.789 s, including
materialization 167.090 s and offline evaluation 96.623 s. The retained candidate therefore
improves solved coverage and total hard-resource count without a resolved-case regression, but
the TIMEOUT-to-OOM shift must remain visible in any promotion decision.

The full 725 evidence is under
`server-results/primitive-fast-compose-cycle-paired-725-20260719a/`. The next exact optimization
audit targets primitive coherence construction (57.518 s in the candidate profile), while all
current switches remain experimental and independently gated.

BenchExec `table-generator` subsequently produced the required 725-row, two-run-set table and the
28-row difference table under that result directory's `html/` subdirectory. Generation used the
authorized endpoint-pipeline tool module; local and server checks find no `cputime-cpux` field in
any generated artifact.
