# External Tool Lineage and Comparability

## Classification

| Tool | Evidence-backed relationship | Available artifact | Exposed models | Experiment family |
|---|---|---:|---|---|
| TruSt | POPL 2022 graph-based, exploration-optimal DPOR with linear memory | 0.5.3 | RC11, IMM in this binary | LP64 RC11 |
| Awamoche | CAV 2023 successor evaluated directly against TruSt; adds stale reads, in-place revisits, and speculative revisits | 0.8 | RC11, IMM, LKMM | LP64 RC11 |
| Spore | PLDI 2024 implementation explicitly built on the TruSt DPOR algorithm and combines it with symmetry reduction | 0.10.1 | SC, TSO, RA, RC11, IMM | LP64 SC and RC11 |
| Mixer | GenMC mixed-size-access extension found on the server; local source identifies MIXER algorithms but does not establish a direct TruSt lineage | 0.10.1 with explicit `--mixer` | SC, TSO, RA, RC11, IMM | separate related-extension table |
| Deagle | SMT/SAT-based bounded concurrent verifier, not DPOR | 4.1.0 | SC default in official wrapper; engine also documents TSO/PSO | official ILP32 SC wrapper |
| CBMC | bounded model checker, not DPOR | official SV-COMP archive | bounded C semantics | official ILP32 wrapper |

## Primary evidence

- TruSt paper: <https://plv.mpi-sws.org/genmc/popl2022-trust-full.pdf>. The
  algorithm is parametric in SC, TSO, PSO, and RC11, but this does not imply
  that every released executable exposes every model.
- Awamoche paper: <https://doi.org/10.1007/978-3-031-37706-8_12>. Its evaluation
  and algorithm are explicitly framed as improvements over TruSt.
- Spore paper: <https://people.mpi-sws.org/~viktor/papers/pldi2024-spore.pdf>.
  The artifact implements the TruSt DPOR algorithm plus symmetry reduction.
- Deagle SV-COMP 2026 archive: <https://zenodo.org/records/17636587>.
- CBMC SV-COMP archive: <https://zenodo.org/records/10396159>.

## Non-negotiable table splits

1. Official SV-COMP task metadata is ILP32. Current GenMC, Deagle, and CBMC can
   enter that table after adapter correctness is verified.
2. The available old TruSt-family binaries conflict with their bundled runtime
   headers under `-m32`; their runs use the same C entries under LP64 and must
   stay in a separate table.
3. SC, TSO, PSO, and RC11 results are separate comparison families. The paper's
   algorithmic model support cannot be used to invent a missing command-line
   mode in an artifact.
4. Timeout, OOM, unsupported feature, inconclusive unwind, wrong verdict, and
   completed verification remain separate. Solved-only speed ratios never
   replace coverage results.

Spore enables symmetry reduction by default (`--disable-sr` turns it off).
Mixer requires the explicit `--mixer` switch; running the binary name alone
would benchmark only its inherited GenMC path and is rejected by the adapter.
