"""Sound-lane BenchExec adapter for GenMC on SV-Benchmarks C tasks."""

from __future__ import annotations

import os
from pathlib import Path
import re

import benchexec.result as result
import benchexec.tools.template
from benchexec.tools.sv_benchmarks_util import get_data_model_from_task, ILP32, LP64

from .rewrite_sources import local_include_closure, rewrite_tree


class Tool(benchexec.tools.template.BaseTool2):
    def name(self):
        return "GenMC SV-COMP fair-coverage PSO experiment adapter"

    def project_url(self):
        return "https://github.com/MPI-SWS/genmc"

    def executable(self, tool_locator):
        path = Path(os.environ["GENMC_BINARY"])
        if not path.is_file():
            raise benchexec.tools.template.ToolNotFoundException(str(path))
        return str(path)

    def version(self, executable):
        return self._version_from_tool(executable)

    @staticmethod
    def _source_for(path: str) -> Path:
        source = Path(path)
        original = source.with_suffix(".c")
        if (
            source.suffix == ".i"
            and original.is_file()
            and "#include <svcomp.h>" in original.read_text(
                encoding="utf-8", errors="ignore"
            )
        ):
            return source
        return original if source.suffix == ".i" and original.is_file() else source

    def cmdline(self, executable, options, task, rlimits):
        benchmark_root = Path(os.environ["GENMC_BENCHMARK_ROOT"])
        rewrite_root = Path(os.environ["GENMC_REWRITE_ROOT"])
        compat_header = Path(os.environ["GENMC_COMPAT_HEADER"])
        backend = None
        forwarded = []
        for option in options:
            if option.startswith("--svcomp-backend="):
                backend = option.split("=", 1)[1]
            else:
                forwarded.append(option)
        model_root = Path(os.environ.get("GENMC_MODEL_ROOT", "."))
        model_options = {
            "genmc-sc": ["--sc"],
            "cat-sc": [f"--model-file={model_root / 'sc.cat'}"],
            "caat-sc": [f"--model-file={model_root / 'recursive-sc.cat'}"],
            "caat-pso": [f"--model-file={model_root / 'recursive-pso.cat'}"],
        }
        if backend not in model_options:
            raise benchexec.tools.template.UnsupportedFeatureException(
                f"unsupported fair-coverage backend {backend!r}"
            )
        inputs = [self._source_for(path) for path in task.input_files_or_identifier]
        if len(inputs) != 1:
            raise benchexec.tools.template.UnsupportedFeatureException(
                f"fair adapter currently requires one input file, found {len(inputs)}"
            )
        original_source = inputs[0]
        rewritten = rewrite_tree(original_source, benchmark_root, rewrite_root)
        if rewritten.unsupported_reason == "data-nondeterminism":
            source = rewrite_root / "unsupported" / original_source.relative_to(benchmark_root)
            source = source.with_suffix(".c")
            source.parent.mkdir(parents=True, exist_ok=True)
            source.write_text(
                "#error GENMC_SVCOMP_UNSUPPORTED_DATA_NONDETERMINISM\n",
                encoding="utf-8",
            )
        else:
            source = rewritten.source
        closure_text = "\n".join(
            path.read_text(encoding="utf-8", errors="ignore")
            for path in local_include_closure(original_source)
        )

        data_model = get_data_model_from_task(task, {ILP32: "-m32", LP64: "-m64"})
        if data_model is None:
            raise benchexec.tools.template.UnsupportedFeatureException(
                "task has no supported data model"
            )
        compiler_options = [data_model]
        # Use optimization only where it materializes a source-level inline
        # body or a standard string builtin that otherwise remains external.
        # Global -O1 creates unsupported LLVM intrinsics and libc calls.
        if "inline void push" in closure_text or "strcpy(" in closure_text:
            compiler_options.append("-O1")
        if "Numerical Integration Method" in closure_text:
            compiler_options.append("-ffp-contract=off")
        if source != original_source:
            compiler_options += ["-iquote", str(source.parent)]
        if (
            "manual-atomic-runtime" not in rewritten.rules
            and (
                "__VERIFIER_atomic_begin" in closure_text
                or "__VERIFIER_atomic_end" in closure_text
                or "PTHREAD_RWLOCK_INITIALIZER" in closure_text
                or "svcomp-atomic-function" in rewritten.rules
            )
        ):
            compiler_options += ["-include", str(compat_header)]
        compiler_options += ["-DNULL=0", str(source)]
        atomic_runtime_options = []
        if (
            "<stdatomic.h>" in closure_text
            or "_Atomic" in closure_text
            or re.search(
                r"\b__atomic_(?:thread_fence|load|load_n|store|store_n|exchange|"
                r"exchange_n|compare_exchange|compare_exchange_n|fetch_[a-z]+)\s*\(",
                closure_text,
            )
        ):
            atomic_runtime_options += ["--disable-ipr", "--disable-sr"]
        return [
            executable,
            *model_options[backend],
            *atomic_runtime_options,
            *forwarded,
            "--disable-race-detection",
            "--",
            *compiler_options,
        ]

    def determine_result(self, run):
        output = run.output
        if run.was_timeout:
            return result.RESULT_TIMEOUT
        if output.any_line_contains("GENMC_SVCOMP_UNSUPPORTED_DATA_NONDETERMINISM"):
            return "ERROR (unsupported data nondeterminism)"
        if output.any_line_contains("Attempt to read from uninitialized memory"):
            return "ERROR (unsupported property: uninitialized memory)"
        if output.any_line_contains("Attempt to access non-allocated memory"):
            return "ERROR (unsupported property: invalid memory access)"
        if output.any_line_contains("Unordered writes do not constitute a bug"):
            return "ERROR (unsupported reduction)"
        if run.exit_code.value == 42 or output.any_line_contains(
            "Verification unsuccessful"
        ) or output.any_line_contains("Verification unsuccesful"):
            return result.RESULT_FALSE_REACH
        if run.exit_code.value == 0 and output.any_line_contains("Verification complete"):
            return result.RESULT_TRUE_PROP
        if output.any_line_contains("unknown external function"):
            return "ERROR (unsupported external)"
        if output.any_line_contains("error generated") or output.any_line_contains(
            "fatal error:"
        ) or run.exit_code.value == 5:
            return "ERROR (compilation)"
        if run.exit_code.value == 137:
            return "OUT OF MEMORY"
        return result.RESULT_ERROR

    def get_value_from_output(self, output, identifier):
        patterns = {
            "Number of complete executions explored: (.*)": re.compile(
                r"Number of complete executions explored:\s*([0-9]+)"
            ),
            "Number of blocked executions seen: (.*)": re.compile(
                r"Number of blocked executions seen:\s*([0-9]+)"
            ),
        }
        pattern = patterns.get(identifier)
        if pattern is None:
            return None
        for line in reversed(output):
            match = pattern.search(line)
            if match:
                return match.group(1)
        return None
