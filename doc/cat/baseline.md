# Phase 1.0 Baseline

## Repository and host

- Date: 2026-07-14 (Asia/Shanghai)
- Branch: `genmc-caat`
- Starting commit: `5f71a10d617bb6ca99ac77b7a522a428535a381a`
- GenMC: v0.17.0, reported commit `5f71a10`
- Host: Apple arm64 macOS
- CMake: 4.0.3
- Configure compiler: AppleClang 17
- LLVM: Homebrew LLVM 20.1.7 at `/opt/homebrew/opt/llvm`
- Build directory: `RelWithDebInfo/` (ignored, not committed)

## Build baseline

Successful local command:

```bash
cmake -S . -B RelWithDebInfo \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DLLVM_DIR=/opt/homebrew/opt/llvm/lib/cmake/llvm \
  -DCMAKE_EXE_LINKER_FLAGS=-L/opt/homebrew/opt/hwloc/lib \
  -DBUILD_TESTS=OFF
cmake --build RelWithDebInfo -j4
RelWithDebInfo/bin/genmc --version
```

Result: configure, generation, and all build targets succeeded. The resulting
binary exited 0 and reported GenMC v0.17.0 with LLVM 20.1.7.

The explicit linker path is an environment workaround. `find_library(HWLOC
hwloc)` cached `/opt/homebrew/lib/libhwloc.dylib`, but `genmc_lib` links the
bare name `hwloc`; the final executable initially failed with `ld: library
'hwloc' not found`. No repository source was changed for the workaround.

Existing compiler warnings from generated checkers, LLVM headers, and legacy
formatting calls were observed. They predate CAT changes and are not Phase 1.0
scope.

## Regression baseline

The repository driver was run with:

```bash
GenMC="$PWD/RelWithDebInfo/bin/genmc" ./scripts/fast-driver.sh
```

Result: exit 0, `Testing proceeded as expected`, reported aggregate test time
50.65 seconds.

Two pre-existing harness observations were retained:

- the script attempted to read `RelWithDebInfo/include/config.h`, which did not
  exist in this build layout;
- a few wrong-test variants emitted `diff` messages for missing expected trace
  files, while the driver still classified results as expected and exited 0.

These are baseline harness issues, not CAT regressions. They must be compared
unchanged after implementation.

## SC/TSO smoke baseline

Each command used `--disable-estimation --disable-mm-detector`. Counts are the
`Number of complete executions explored` reported by GenMC.

| Variant | Built-in SC | Built-in TSO | Wall-clock range |
|---|---:|---:|---:|
| `SB/variants/sb0.c` | 3 | 4 | 0.04s |
| `LB+ctrl/variants/lb+ctrl0.c` | 3 | 3 | 0.03s |
| `WWR+2WR/variants/wwr+2wr0.c` | 0, unsuccessful | 0, unsuccessful | 0.03-0.05s |

The full driver reports WWR+2WR as safe with 0 complete and 8 blocked
executions. Phase 1 differential tests must compare blocked executions as well
as complete executions, rather than treating zero complete executions alone
as failure.

## Unit and CTest baseline

The first `BUILD_TESTS=ON` configure cloned RapidCheck successfully, then
stalled while recursively cloning RapidCheck's unused historical Catch
submodule. The build was completed without a repository change by directing
CMake at the already cloned RapidCheck source:

```bash
cmake -S . -B RelWithDebInfo \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DLLVM_DIR=/opt/homebrew/opt/llvm/lib/cmake/llvm \
  -DCMAKE_EXE_LINKER_FLAGS=-L/opt/homebrew/opt/hwloc/lib \
  -DBUILD_TESTS=ON \
  -DFETCHCONTENT_SOURCE_DIR_RAPIDCHECK="$PWD/RelWithDebInfo/_deps/rapidcheck-src"
cmake --build RelWithDebInfo -j4
```

Focused CTest results:

- all 40 registered IntervalMap, View, Error, and Cast unit/property tests
  passed; 0 failed; 0.21 seconds total;
- `ctest --test-dir RelWithDebInfo --output-on-failure -R '^fast-driver$'`
  passed; 1/1; 76.72 seconds total.

The source override is a repeatable network workaround, not a product
dependency or committed build change. A fresh machine still needs GoogleTest
and RapidCheck access once.

## Reuse and license baseline

| Source | Inspected artifact | License | Phase 1 decision |
|---|---|---|---|
| GenMC | `ExecutionGraph`, generated SC/TSO checkers, CMake/tests | Apache-2.0/MIT | reuse graph APIs, checkers as oracle, and test harness |
| herdtools7 | `lib/modelLexer.mll`, `lib/modelParser.mly`, SC/TSO/x86tso models | CeCILL-B | syntax/semantics and oracle only; do not copy source |
| CAT paper | local 2016 PDF | publication | normative typing and relational semantics |
| CAAT paper | local OOPSLA 2022 PDF | publication | later offline fixed-point architecture; no Phase 1 solver |
| Kater | public paper, artifact instructions, generated GenMC checkers | paper/artifact terms; artifact code outside GenMC GPLv2 | reuse generated behavior and algorithmic ideas; no large runtime/source import |

The Kater artifact is distributed as an approximately 3.6 GB Docker image and
places Kater source and a modified GenMC inside the image. This is not a small,
reviewable Phase 1 dependency. Current generated checkers remain the closest
in-repository oracle.

## Specification freeze result

- Frozen language: `doc/cat/supported-cat.md`
- Feature/test mapping: `doc/cat/phase-1-fixtures.md`
- Acceptance models: `models/cat/sc.cat`, `models/cat/tso.cat`, and
  `models/cat/pso.cat`
- Model source policy: clean-room expressions derived from published axiomatic
  definitions; no herdtools7 implementation/model text copied.
