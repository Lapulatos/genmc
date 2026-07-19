# Implementation Plan: SV-COMP 2026 C.Concurrency

## Goal

Implement and verify a two-stage experiment pipeline: first classify all C.Concurrency tasks by compatibility and failure mode, then run paired BenchExec comparisons for GenMC, GenMC+CAT, GenMC+CAAT and comparable external tools, including TruSt and its descendants.

## Phases

- [x] Read the requested two-stage specification and required skills.
- [x] Inventory local builds, benchmark sources, existing scripts, BenchExec, and tool availability.
- [x] Acquire and pin the official SV-COMP 2026 benchmark definition and task files.
- [x] Implement stage-1 census with property/frontend/feature/status classification.
- [x] Implement stage-2 paired BenchExec definitions for the three GenMC modes.
- [x] Add opt-in active/stable event, primitive-relation density, and base/history
  byte diagnostics for the post-performance profiling pass.
- [x] Identify TruSt descendants and add adapters for locally available comparable tools.
- [x] Add Deagle and CBMC adapters with explicit semantic-comparability metadata.
- [x] Run smoke tests and validate parsing, limits, outputs, and result pairing.
- [x] Regenerate the formal SC strict-analysis bundle with a failure matrix,
  mismatch quarantine, two validated SVG figures, and a figure catalog.
- [x] Pull all finalized BenchExec XML and logs available before the active PSO
  run, and generate linked `table-generator` HTML views for all runs, census,
  SC, TSO, and smoke experiments.
- [ ] Run the largest feasible experiment batch and generate a strict analysis bundle.

## Correctness Gates

- Unsupported, timeout, OOM, wrong answer, and internal error remain distinct.
- A comparison cell uses the same task, property, architecture, memory model, resource limits, and repetitions.
- External-tool rankings are split when explored semantics or properties are not comparable.
- Raw BenchExec XML/logs remain immutable inputs to analysis.
- No significance claim is emitted without a valid repeated-measure unit and enough repetitions.

## Decisions

- TruSt means the POPL 2022 tool associated with “Truly Stateless, Optimal Dynamic Partial Order Reduction,” not Tracer.
- TruSt descendants will be identified from primary papers and official repositories before adapters are written.
- The first performance subset is expected to be supported `unreach-call` tasks; other properties remain in the compatibility census unless equivalent checking is verified.
- Stage 1 selected 725 `unreach-call` tasks. The corrected census classified 96 as correct, 18 as wrong, and 611 as errors or resource failures.
- The transparent full-category assembly contains all 3,520 property rows from 1,060 YAML files. Only 725 `unreach-call` rows were dynamically executed; the remaining 2,795 rows are explicitly marked `not_executed_property_adapter_unverified` and are not claimed as dynamically compatible.
- Stage 2 uses five paired repetitions and rotates backend order each repetition.
- SC exploration equivalence is verified from the finalized log ZIP: 425
  common solved task-repetition cells, zero missing execution counts, and zero
  verdict/complete/blocked-count mismatches.
- Post-core compatibility censuses are scripted to run with eight workers but
  hard-refuse while the timing queue is active. The overlap gate was exercised
  during TSO and returned the intended exit code 2 without starting a run.

## Errors

- Native BenchExec 3.25 cannot run on macOS: `runexec` loads Linux `libc.so.6`.
  Resolution: execute BenchExec in Linux Docker or on a Linux host.
- Docker CLI is installed, but the local Docker daemon is not running on 2026-07-14.
  This blocks actual BenchExec runs, but not benchmark acquisition, definitions,
  adapters, static validation, or analysis implementation.
- First source `rsync` failed because remote parent `source/` did not exist.
  Resolution: create the isolated parent directory and repeat the same sync.
- The first pipeline `rsync` omitted remote port 2222 and failed without changing remote files.
  Resolution: repeat with `rsync -e 'ssh -p 2222'`.
- The first CBMC archive transfer stopped before the ZIP central directory.
  Resolution: resume only after the timing-sensitive SC batch, then verify the
  official 14,615,005-byte size and MD5 before extraction.
- The local benchmark cache retains only Git metadata, so the enriched
  `.c`/local-header size census cannot be regenerated locally. It will be run
  against the authoritative sparse checkout on the remote server after the
  timing queue, without altering already generated experiment definitions.
- The first ad-hoc TSO consistency check referenced a non-existent normalized
  column named `task`; the parser schema uses `task_yaml`. Resolution: rerun the
  read-only check with `task_yaml`; all two-repetition failure sets and verdicts
  are stable.
- A partial-analysis dry run rendered the denominator of an additional planned
  contrast as the global baseline, although its TSV value and ratio were
  correct. Resolution: render `row['baseline']` and regression-check that
  `caat-tso/cat-tso` is labeled exactly once. The appendix now also reports the
  configured bootstrap count instead of hard-coding 10,000.
- The analyzer originally treated any stable true/false result as solved
  without first enforcing cross-backend verdict agreement. Resolution: reject
  duplicate or incomplete repetition grids, quarantine cross-backend verdict
  mismatches in `verdict-mismatches.tsv`, and exclude them from all timing and
  memory ratios. SC and two-round TSO have zero such mismatches; a synthetic
  mismatch regression is detected and excluded.
- The first generated failure matrix joined manifest metadata by `task_yaml`
  alone, but a YAML may contain multiple properties, so an `unreach-call` row
  could display the `no-data-race` expected verdict. Resolution: require a
  single-property analysis input and filter manifest metadata by both task and
  the analyzed property. All failure-matrix expected verdicts now match the
  normalized result input; a mixed-property regression is rejected.
- One status command accidentally prefixed the remote root twice and printed a
  spurious `TSO=0`; the same read-only command immediately used the authoritative
  path and confirmed `TSO=11`. No files or processes were affected.
- Generated benchmark XML placed custom `<column>` elements directly under
  `<benchmark>`, but BenchExec only loads them from a `<columns>` wrapper. This
  explains why even PSO omitted execution/blocked values despite the corrected
  callback. Resolution: fix the generator and regression-check all 11 nested
  diagnostic columns. Existing core results remain recoverable from logs; all
  future diagnostic definitions must be regenerated before execution.
- The continuation shell did not contain `REMOTE_SSH_PASSWORD`, so the first
  password-environment status probe stopped before opening a connection.
  Resolution: use the already-authorized SSH public key in `BatchMode`; the
  read-only probe succeeded and no credential was copied into files or commands.
- The first non-executing BenchExec definition-load check used the non-existent
  long option `--selected-run-definitions`; the corrected BenchExec 3.25 option
  is `-r`. A second attempt omitted `GENMC_EXPERIMENT_ROOT` and correctly exposed
  the adapter's required environment variable. Resolution: rerun with `-r` and
  the experiment root set. Both TruSt-family definitions load successfully;
  Deagle/CBMC received XML-structure validation and remain gated on installation.
- A local artifact-list command used GNU `find -printf`, which macOS `find`
  does not implement. It was a read-only convenience command; use `find ...
  -print` or `rg --files` locally instead.
- The first new-server image build could not resolve Docker Hub over the
  server's network path and timed out before creating a layer or image.
  Resolution: keep the same Ubuntu 24.04 base digest source but use the
  reachable public DaoCloud registry endpoint; do not reuse or retag another
  user's local image.
- The first new-container build command embedded `$root` inside an outer remote
  double-quoted shell command, so the host expanded it to an empty string and
  CMake saw `/source/genmc`. Resolution: use explicit container-absolute paths;
  the Release build then completed successfully.
- The first source rsync reached a missing `source/` parent and transferred no
  files. Resolution: create `/data3/sujie/svcomp2026-caat/source/genmc` as the
  authorized login user and rerun; 6,226 source files are present.
- Enabling `BUILD_TESTS` produced 152 CTest entries: 147 pass and five legacy
  integration drivers fail because the supplied worktree lacks their fixed
  `scripts/run-parallel.sh`, trace, or in-source `RelWithDebInfo` paths. All 141
  unit-test entries pass. These environmental integration failures are not
  counted as a full green suite and remain recorded for final provenance.
- The first TruSt-family image attempt copied runtime libraries from the old
  server. This violates the required image provenance and was discarded; its
  temporary staging directory and unified base tag were removed without
  touching other users' images or containers.
- The next four tool images installed LLVM dependencies from Ubuntu 22.04
  packages but copied the old Ubuntu 24-built executables. Validation failed
  before tool startup because those executables require `GLIBC_2.38` and
  `GLIBCXX_3.4.31/32`, newer than Ubuntu 22.04 provides. Resolution: use only
  source snapshots from the old server and compile each tool inside its own
  package-installed image; no old system library, header, or runtime is reused.
- A source-inspection probe initially looked under the experiment package root,
  but the authoritative variant sources are under
  `/home/lapulatos/Documents/Codes/C++/GenMC/variants`. The corrected read-only
  probe found all four sources and their release archives.
- The first source-built images passed `--version` but all four exited 139 on a
  real C task. The multi-stage final images omitted the source-provided GenMC
  headers at the compile-time embedded `/src/include` path. Resolution: copy
  only those versioned source headers from the builder stage into the final
  image and repeat real-task validation; this is source content, not an old
  server environment.
- Two remote validation loops used zsh-incompatible `$t:sujie` expansion.
  Resolution: use `${t}:sujie`; no image or container state was changed by the
  failed read-only loops.
- The first adapter sync to the new experiment root was rejected because files
  created through the mounted build container are root-owned. No file was
  overwritten. Resolution: upload into `/data3/sujie/incoming-bx-smoke` and use
  the existing owner-labeled container to copy and validate the files inside
  the same personal mount.
- The first four-tool BenchExec smoke selected only the false task because the
  proposed true YAML did not declare `unreach-call`. Resolution: replace it
  with `13-privatized_01-priv_nr_true.yml`, which is a confirmed true
  `unreach-call` task, and rerun all four images. Each then classified one true
  and one false task correctly with CPU, wall-time, memory, XML, and log output.
- The first Deagle source build from commit `980fe9e6757e...` failed after a
  clean rebuild because MiniSat's `ParseUtils.h` includes `zlib.h`. Resolution:
  add the missing Ubuntu `zlib1g-dev` build dependency and rebuild the same
  pinned source; the prebuilt-artifact image is not accepted for experiments.
- The first successful Deagle source image reported v4.1.0 but its runtime
  smoke failed with `GCC preprocessing failed` because `deagle_exe` invokes
  `gcc` dynamically. Resolution: add apt-installed `gcc`, `gcc-multilib`, and
  `libc6-dev-i386` to the final runtime stage before accepting the image tag.
- The first Deagle BenchExec smoke ran two tasks concurrently from the shared
  `/opt/deagle` working directory. Both returned `unknown` because the upstream
  launcher uses fixed `deagle_tmp_output` and `deagle_tmp_unwind_suggest`
  names. Resolution: retain the upstream launcher and clean source-built
  executable, but invoke them through a minimal wrapper that creates and
  removes one private temporary directory per process. The repeated two-worker
  smoke then classified one true and one false task correctly.
- The first fixed-smoke setup attempted to create a result directory directly
  under a root-owned mounted experiment tree and was denied before BenchExec
  started. Resolution: create the directory through the existing owner-labeled
  personal container; no result was overwritten and the subsequent run passed.
- The first one-column-per-method `table-generator` command used a broad
  `*cat-*` shell pattern that also matched `caat`. Resolution: discard those
  pages, use exact `.results.cat-*` and `.results.caat-*` patterns, and add a
  generator-side assertion over the CSV run-set row. The final views contain
  exactly 3 SC, 3 TSO, 2 PSO, 4 TruSt-family, and 2 ILP32-tool columns.
- The in-app browser could not open the local comparison index because its
  current tool request omitted required sandbox metadata. Resolution: validate
  all five links and target HTML structures with a local HTML parser and retain
  browser visual inspection as an environment limitation, not an experiment
  failure.
- A shell-only report preview used unquoted zsh text beginning with `===` and
  triggered glob parsing before reading any result. Resolution: use `printf`
  with a quoted value; no artifact was changed by the failed read-only command.
- A Deagle temporary-directory audit traversed a stale `/tmp/fuse` entry and
  printed a harmless `find` warning. The exact `deagle-run.*` count was still
  zero, and XML/log checksums and counts were independently verified.

## New-server TruSt-family image provenance

- `trust:sujie`: Ubuntu 22.04, apt LLVM/Clang 11.1.0 dependencies, source-built
  TruSt v0.5.3 at commit `a0e90a8d59047b4ea47ab352ff08948bc85b81cc`.
- `awamoche:sujie`: Ubuntu 22.04, apt LLVM/Clang 11.1.0 dependencies,
  source-built Awamoche v0.8 release snapshot.
- `mixer:sujie`: Ubuntu 22.04, apt LLVM/Clang 14.0.0 dependencies, source-built
  Mixer v0.10.1 release snapshot.
- `spore:sujie`: Ubuntu 22.04, apt LLVM/Clang 14.0.0 dependencies, source-built
  Spore v0.10.1 release snapshot.
- All four final images pass a real SV-COMP concurrency task,
  `c/goblint-regression/04-mutex_02-simple_nr.c`, with exit code 0. No image
  copies an old-server system library, header, sysroot, or executable.
- The image-aware BenchExec adapter accepts exactly one tool binary through
  `TRUST_FAMILY_TOOL` and `TRUST_FAMILY_BINARY`; it no longer needs copied
  runtime libraries. A 725-task RC11/LP64 `unreach-call` compatibility census
  is active for each of TruSt, Awamoche, Mixer, and Spore with 10 s, 4 GB, one
  core per task, and eight workers per isolated container.

## New-server Deagle image provenance

- `deagle:sujie`: Deagle v4.1.0 from the repository's only current branch at
  commit `980fe9e6757e967d12454c7d16e2ec4de1a1cae4`; the codeload archive has
  SHA-256 `0f87debf2c90b59dc0e94759b4d7f412a22636a2271972bd72991e96ec4bd9ae`.
- The image performs `make clean` and rebuilds `cbmc.dir` from source with apt
  build dependencies. It does not copy an executable or runtime environment
  from the old server.
- A per-process temporary-directory launcher isolates the upstream script's
  fixed output names. A two-worker BenchExec smoke correctly classifies one
  safe and one unsafe `unreach-call` task.
- Final image ID: `1aae6358d8bb6aa764fb55f99419aac33c325385ad7a539229f6093d9710136f`.

## Status

Stage-1 `unreach-call` census and full-property transparent assembly are complete.
The five-repetition paired SC and TSO runs are complete and have strict analysis
bundles. TSO contains 1,440 runs; all 410 common solved task-repetition cells
match in verdict, complete executions, and blocked executions. PSO started
automatically at 2026-07-14 20:49 CST and remains active. A
1,030-task SC `no-data-race` census definition is prepared but will only run
after the core queue. External-tool and
diagnostic runs intentionally wait until the timing-sensitive SC/TSO/PSO queue
finishes. The diagnostic counters build locally and all 141 unit tests pass.

At 2026-07-14 22:07 CST, PSO had sealed 6/10 run sets, i.e. three complete
paired repetitions. The first two complete repetitions already parsed are
stable: CAT solves 82/96 (9 timeout, 5 OOM), CAAT solves 79/96 (12 timeout,
5 OOM), and the 79 commonly solved tasks have zero verdict mismatch. These are
an interim correctness/health checkpoint, not a formal performance analysis.
Only finalized `.xml.bz2` files enter parsing or HTML snapshots.

Before post-core execution, the four older TruSt-family, Deagle, and CBMC XML
definitions were copied into `benchexec-view/definitions/`, changed from invalid
root-level custom columns to a two-column `<columns>` container, synced back to
the server, and validated. This does not touch the active PSO definition or
timing binary.

On the new server, all four TruSt-family and CBMC 725-task compatibility
censuses are complete, and their raw XML/log archives and normalized summaries
have been pulled locally. The final source-built `deagle:sujie` image has
passed direct and concurrent BenchExec smoke checks; its 725-task compatibility
census is the next external-tool run.

After the CBMC census artifacts were pulled and verified locally, the user
explicitly requested removal of `cbmc:sujie`; image
`e6cb215a501d2e6edb206317e9b27c1707c3240d5dd52fef8b74d41fdf842258`
was unreferenced by containers and deleted. The obsolete dangling Deagle image
`030eccd419be...` had already been deleted. No other user's image or container
was changed.

The Deagle census is complete: 725 XML runs and 725 archived logs passed
compression checks and were pulled locally. It reports 596 correct, 2 wrong,
20 unknown, and 107 error/resource rows. The completed container was removed;
the reproducible `deagle:sujie` image remains.

The final PSO performance batch is also complete and local: ten XML files and
one complete log archive contain 96 tasks, two backends, and five repetitions
(960 normalized rows). There are 395 common solved task-repetition cells and
zero verdict/execution/blocked-count mismatch.

The separate CAT/CAAT scale-diagnostic batch completed 576 runs (96 tasks x
six CAT/CAAT SC/TSO/PSO backends). Six XML files and 576 archived logs passed
integrity checks and were pulled. CAAT diagnostics were available for 83 SC,
80 TSO, and 77 PSO task cells that also have fully paired formal timing data.
The combined report and one-column-per-method HTML views are generated locally;
final artifact and Git-scope audit remains.

The final audit refreshed the full Stage-1 assembly with both completed dynamic
censuses. It now contains all 3,520 property rows: 1,755 executed
`unreach-call`/`no-data-race` rows and 1,765 explicit
`property_adapter_unsupported` rows. All primary XML and log counts match,
all six HTML entry targets return HTTP 200 locally, the one-method-per-column
assertions pass, no server password is stored in the worktree, and the Git
staging area is empty. The last owner-labeled experiment container was removed;
no `owner=sujie` containers remain on the new server.
