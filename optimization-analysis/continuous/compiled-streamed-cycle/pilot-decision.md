# P0.7g ordered compiled stream program: pilot decision

Date: 2026-07-17  
Baseline: `d963c49e45b82066ca9dcd036232c7384f6a95fe`  
Decision: **reject; do not run the formal matrix; do not commit production code**

## Correctness and mechanism gates

- Release unit/property tests: 159/159 passed.
- Focused GCC 13 ASan+UBSan tests: 18/18 passed.
- Mutation oracle: 39 rows and 5,441 full recomputations, zero mismatch.
- Broad differential: 852 comparable matches, 12 mutually unsupported cases and zero
  mismatch over 864 SC/TSO/PSO pairs.
- Pilot: 12/12 paired cells completed with zero status or category mismatch.
- SC correctly executed zero compiled-stream checks. TSO and PSO representatives
  executed the compiled path; for example, TSO queue recorded 4 checks / 26 terms and
  PSO queue recorded 34 checks / 221 terms.
- Root candidate counts were exact before/after in every terminal paired cell. PSO
  queue, for example, emitted 49,930,431 candidates in both versions.
- Snapshot-equivalent bytes and maximum current-base bytes were exact before/after.

## Time result

The candidate removed meaningful interpreter overhead on affected representatives:

- PSO queue median CPU ratio: 0.89724.
- TSO fib median CPU ratio: 0.92975.
- TSO queue median CPU ratio: 0.93435.
- SC queue median CPU ratio: 1.02348, within the frozen 1.03 guard.

These results satisfy the pilot's time and activation conditions. They do not override
the independent memory condition.

## Rejecting memory result

The frozen protocol requires every observed RSS ratio to be at most 1.02. PSO queue
violated this condition in both repetitions:

| Repetition | Baseline RSS | Candidate RSS | Ratio | Delta |
|---|---:|---:|---:|---:|
| r01 | 117,391,360 B | 124,887,040 B | 1.06385 | +7,495,680 B |
| r02 | 117,370,880 B | 119,992,320 B | 1.02233 | +2,621,440 B |

The binary grew only 21,136 bytes on disk and about 23,844 bytes in its text segment.
CAT snapshot-equivalent state remained exactly 44,133,960 bytes at peak and maximum
current-base state remained exactly 5,872,240 bytes. Thus the result does not indicate
execution-graph state growth, but it is still a reproducible process peak-RSS regression
under the predeclared acceptance metric. Relaxing the threshold after observing the
result would invalidate the experiment.

## Scope consequence

P0.7g is not a retained general optimization. Its exact patch and raw evidence are
preserved for diagnosis. A later candidate may reuse the semantic idea only under a
new frozen protocol and must first remove the additional production instrumentation and
object-layout changes, then re-establish the memory result from a clean baseline.

Archived patch:
`optimization-analysis/continuous/compiled-streamed-cycle/rejected-source.patch`  
SHA-256: `0e183b001af8540a547780ca09e58c24e85f4a39a29f046561b8c10de4635294`

After archival, every production and test path under `genmc/` and `tests/` was restored
to the retained baseline; `git diff --exit-code -- genmc tests` and
`git apply --check` on the archived patch both succeeded. No experiment container with
a `compiled-stream`, `caat-selector` or `fused-lazy` name remains on the server.

This decision does not use Optimization 6.1, model names, benchmark names, expected
verdicts or a built-in checker as an authority.
