# Implementation Notes

## Environment

- Repository: `/Users/sujie/Documents/Codes/C++/GenMC/genmc`
- BenchExec: `/opt/homebrew/bin/benchexec`, version 3.25
- Existing GenMC build: `RelWithDebInfo/`
- Related local trees found: `../trust-clion-debug`, `../genmc-tool-bak/src/trust`
- Official benchmark tag: `svcomp26`, peeled commit
  `7efe28dd29576b46927b7a34e8f742bd90966a75`.
- Local host is Apple arm64 macOS; execution was moved to the authorized Ubuntu
  24.04 x86-64 server at `/home/lapulatos/svcomp2026-caat`.
- Remote host: 16 CPUs, 46 GiB RAM, cgroup v2. BenchExec 3.25 runs through a
  delegated user systemd scope in the `benchexec` slice.
- `libc6-dev-i386` is installed, so current GenMC compiles SV-COMP ILP32 tasks
  with plain `-m32`.
- `../genmc-tool-bak/src/trust` is an x86-64 Linux ELF and cannot execute natively.

## Evidence collected

- Official C.Concurrency definition selects 1,060 task YAML files and 3,520
  property rows; the verified-results headline count excludes unsupported/void
  cells and is not the stage-1 input count.
- Remote GenMC: v0.17.0, LLVM 15.0.7. Source is mirrored into an isolated build
  directory; the rsync copy has no `.git`, hence the binary reports commit
  `unknown`.
- Official Deagle archive: 6,338,434 bytes, MD5
  `ee6cc0f6f37e661b04945732af1233a8`, ZIP integrity verified.
- Official CBMC archive expected size: 14,615,005 bytes, MD5
  `66d24472f543c9f157a4fb65804b039c`; download is incomplete and must not be
  installed until both size and checksum match.

## Comparability warning

SV-COMP verification tools and stateless model checkers may target different properties, memory models, and execution semantics. Tool results will only share a performance table after these fields match; otherwise they will appear as separate coverage/compatibility results.

## Stage-1 scope checkpoint

- The transparent category manifest contains 3,520 property rows from 1,060
  YAML files. The dynamic GenMC census currently covers the 725 `unreach-call`
  rows only: 96 correct, 18 wrong, and 611 resource/frontend/feature failures.
- The other 2,795 rows are retained as
  `not_executed_property_adapter_unverified`; they are not silently counted as
  unsupported and cannot support a full-category compatibility claim yet.

## SC repeated-run result

- All five repetitions have identical coverage: GenMC 96/96 solved, CAT 85/96,
  CAAT 90/96.
- CAT and CAAT both OOM on the same five `goblint-regression/28-race_reach`
  tasks: `01-simple_racing`, `06-cond_racing1`, `11-ptr_racing`,
  `24-sound_lock_racing`, and `37-indirect_racing`.
- Both time out on `pthread/queue_longer`. CAT additionally times out on
  `pthread-deagle/circular_buffer_ok`, `pthread/fib_safe-{5,6}`, and
  `pthread/fib_unsafe-{6,7}`; CAAT completes those five.
- Across all five repetitions, the 85 tasks solved by all three methods have
  exactly matching verdict, complete-execution count, and blocked-execution
  count: 425 common task-repetition cells, zero missing counts, and zero
  mismatches. Counts were recovered from the validated 6,460,557-byte SC log
  ZIP because the initial XML custom-column callback was absent.

## Core queue checkpoint

- At 2026-07-14 19:45 CST, TSO had completed 3/15 run sets: repetition 1 for
  GenMC, CAT, and CAAT. PSO is queued automatically after TSO.
- The core experiment intentionally uses one BenchExec worker. Changing
  concurrency midway would invalidate timing comparability across repetitions.
- At 2026-07-14 20:04 CST, TSO had completed three full repetitions and the
  first run set of repetition 4. The three full repetitions are stable:
  GenMC solves 96/96, CAT 82/96, and CAAT 89/96 each time, with zero verdict
  mismatches. Mean observed wall time per repetition is approximately 38.24 s,
  712.12 s, and 325.04 s, respectively. These are interim checkpoints, not the
  final five-repetition estimates.
