"""BenchExec adapter for GenMC on SV-Benchmarks C tasks."""

from __future__ import annotations

import os
from pathlib import Path
import re

import benchexec.result as result
import benchexec.tools.template
from benchexec.tools.sv_benchmarks_util import get_data_model_from_task, ILP32, LP64


class Tool(benchexec.tools.template.BaseTool2):
    def name(self):
        return "GenMC SV-COMP adapter"

    def project_url(self):
        return "https://github.com/MPI-SWS/genmc"

    def executable(self, tool_locator):
        explicit = os.environ.get("GENMC_BINARY")
        if explicit:
            path = Path(explicit)
            if not path.is_file():
                raise benchexec.tools.template.ToolNotFoundException(str(path))
            return str(path)
        return tool_locator.find_executable("genmc")

    def version(self, executable):
        return self._version_from_tool(executable)

    @staticmethod
    def _source_for(task) -> str:
        source = Path(task.single_input_file)
        if source.suffix == ".i":
            original = source.with_suffix(".c")
            if original.is_file():
                return str(original)
        return str(source)

    @staticmethod
    def _needs_atomic_compat(source: str) -> bool:
        pending = [Path(source)]
        seen: set[Path] = set()
        while pending:
            path = pending.pop()
            if path in seen:
                continue
            seen.add(path)
            try:
                text = path.read_text(encoding="utf-8", errors="ignore")
            except OSError:
                continue
            if "__VERIFIER_atomic_begin" in text or "__VERIFIER_atomic_end" in text:
                return True
            for line in text.splitlines():
                stripped = line.strip()
                if not stripped.startswith('#include "'):
                    continue
                name = stripped.split('"', 2)[1]
                dependency = path.parent / name
                if dependency.is_file():
                    pending.append(dependency)
        return False

    def cmdline(self, executable, options, task, rlimits):
        backend = None
        forwarded = []
        for option in options:
            if option.startswith("--svcomp-backend="):
                backend = option.split("=", 1)[1]
            else:
                forwarded.append(option)
        if backend is None:
            raise benchexec.tools.template.UnsupportedFeatureException(
                "missing --svcomp-backend"
            )

        root = Path(os.environ["GENMC_EXPERIMENT_ROOT"])
        source_root = root / "source" / "genmc-caat"
        model_options = {
            "genmc-sc": ["--sc"],
            "genmc-tso": ["--tso"],
            "cat-sc": [f"--model-file={source_root}/models/cat/sc.cat"],
            "cat-tso": [f"--model-file={source_root}/models/cat/tso.cat"],
            "cat-pso": [f"--model-file={source_root}/models/cat/pso.cat"],
            "caat-sc": [f"--model-file={source_root}/models/cat/recursive-sc.cat"],
            "caat-tso": [f"--model-file={source_root}/models/cat/recursive-tso.cat"],
            "caat-pso": [f"--model-file={source_root}/models/cat/recursive-pso.cat"],
        }
        if backend not in model_options:
            raise benchexec.tools.template.UnsupportedFeatureException(
                f"unknown backend {backend}"
            )

        data_model = get_data_model_from_task(task, {ILP32: "-m32", LP64: "-m64"})
        if data_model is None:
            raise benchexec.tools.template.UnsupportedFeatureException(
                "task has no supported data model"
            )
        compiler_options = [data_model]
        source = self._source_for(task)
        if self._needs_atomic_compat(source):
            compiler_options += [
                "-include",
                str(root / "pipeline" / "include" / "svcomp_genmc_compat.h"),
            ]
        compiler_options += ["-DNULL=0", source]
        return (
            [executable]
            + model_options[backend]
            + forwarded
            + (["--disable-race-detection"] if Path(task.property_file).stem == "unreach-call" else [])
            + ["--"]
            + compiler_options
        )

    def determine_result(self, run):
        output = run.output
        if run.was_timeout:
            return result.RESULT_TIMEOUT
        # BenchExec 3.25 has no named constant for this SV-COMP property, but
        # generic false(<subproperty>) results are supported.  Do not report a
        # race as false(unreach-call): that would make the no-data-race census
        # look wrong even when GenMC found exactly the requested violation.
        if output.any_line_contains("Non-atomic race") or output.any_line_contains(
            "Malloc-free race"
        ):
            return "false(no-data-race)"
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
        ):
            return "ERROR (compilation)"
        if run.exit_code.value == 5:
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
        if identifier in patterns:
            pattern = patterns[identifier]
        else:
            match = re.fullmatch(r"([a-z][a-z0-9-]*)=\(\[0-9\]\+\)", identifier)
            if not match:
                return None
            pattern = re.compile(rf"\b{re.escape(match.group(1))}=([0-9]+)\b")
        for line in reversed(output):
            match = pattern.search(line)
            if match:
                return match.group(1)
        return None
