# P1 dynamic preheader-seed admission rejection (2026-07-20)

## Decision

Reject arbitrary dynamic outside-loop PHI seeds and keep the retained constant-only admission.
The candidate compiled, activated on a dynamic `atomicrmw` seed, and passed the intended infinite
polling positive case, but failed the predeclared completeness oracle before any resource
benchmark. No 4-task, OOM31, or 725 performance result is claimed.

## Candidate

The candidate ignored every PHI incoming value whose incoming block was outside the loop. Inside
the loop, it continued to require a same-address, same-order polling load or an admitted PHI chain,
rejected constants and unsupported instructions, and required at least one loop-carried load.

This looked sufficient to distinguish the one-time seed from the recurrence, but it was not. The
outside value can decide whether the first body iteration occurs. `SpinAssumePass` inserts the
spin-end assumption on the latch; admitting the PHI can therefore suppress a finite first
iteration whose body load makes the next header condition false.

## Correctness result

The GCC 13 Release production build succeeded in `genmc15noble:sujie`. A dynamic positive fixture
with a nonzero ticket seed and permanently nonzero polling load produced the intended `0` complete
and `1` blocked execution. The decisive finite oracle was:

```c
int observed = atomic_fetch_add_explicit(&ticket, 0, memory_order_relaxed);
while (observed != 0)
	observed = atomic_load_explicit(&flag, memory_order_relaxed);
assert(0);
```

Here `ticket` starts at one and `flag` at zero. The retained constant-only implementation reaches
the assertion with safety status 42 after the body load supplies the next header value. The
dynamic-seed candidate instead reported no error, `0` complete executions, and `1` blocked
execution. This is a completeness regression, so the production change and its positive fixture
were removed immediately.

## Infrastructure notes

- The local Clang 14 Debug tree had drifted to libstdc++ 14 and failed in C++23
  `<format>/<ranges>`; the local GCC 14 tree failed in LLVM 14 `bit_cast` overload resolution.
  Neither is candidate evidence. The established server GCC 13 Release build succeeded.
- The first server oracle invocation exited 17 because the clean candidate source inherited the
  prior production file but not its three test fixtures. After syncing the missing fixtures with
  `rsync -R` and verifying all four paths, the real semantic failure above was reproduced with an
  explicit diagnostic.

Immutable diagnostic root:
`/data3/sujie/experiments/caat-optimization/p1-spin-dynamic-phi-clean-20260720-r1`.

## Consequence

Do not generalize the retained constant-seed rule to arbitrary dynamic preheader values without a
different transformation that explicitly preserves the finite first-iteration exit. Per the
optimization policy, no third admission variant is attempted in this line. `rec_ticketlock` and
`ticketlock` remain unsolved rather than trading completeness for lower resource use.
