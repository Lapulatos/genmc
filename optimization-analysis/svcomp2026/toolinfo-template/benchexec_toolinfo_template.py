"""Template for an SV-COMP BenchExec ToolInfo module.

This file belongs in BenchExec's ``benchexec/tools/<tool>.py``.  It must stay
side-effect free: per-task preprocessing belongs in the archive-bundled runner.
"""

from __future__ import annotations

import re

import benchexec.result as result
import benchexec.tools.template
from benchexec.tools.sv_benchmarks_util import (
    ILP32,
    LP64,
    get_data_model_from_task,
)


_RESULT_PREFIX = "SVCOMP_RESULT="
_STATUS_PREFIX = "SVCOMP_STATUS="
_STAT_PREFIX = "SVCOMP_STAT "
_VALID_RESULTS = {
    result.RESULT_TRUE_PROP,
    result.RESULT_UNKNOWN,
    result.RESULT_FALSE_REACH,
    result.RESULT_FALSE_DATARACE,
    result.RESULT_FALSE_DEREF,
    result.RESULT_FALSE_FREE,
    result.RESULT_FALSE_MEMTRACK,
    result.RESULT_FALSE_MEMCLEANUP,
    result.RESULT_FALSE_OVERFLOW,
}


class Tool(benchexec.tools.template.BaseTool2):
    """ToolInfo for a verifier with an archive-bundled SV-COMP runner."""

    # Paths are relative to the directory containing the runner executable.
    # Replace these with the exact archive layout and include every executable,
    # model file, shared library, header, and Python module needed at run time.
    REQUIRED_PATHS = ["../lib/**", "../models/**", "../share/**"]

    def name(self):
        return "REPLACE_WITH_TOOL_NAME"

    def project_url(self):
        return "https://example.invalid/replace-me"

    def executable(self, tool_locator):
        return tool_locator.find_executable("tool-svcomp.py", subdir="bin")

    def version(self, executable):
        # The runner should report both adapter and underlying verifier versions.
        return self._version_from_tool(executable)

    def cmdline(self, executable, options, task, rlimits):
        task.require_input_files()
        task.require_single_input_file()
        if not task.property_file:
            raise benchexec.tools.template.UnsupportedFeatureException(
                "SV-COMP runner requires a property file"
            )

        data_model = get_data_model_from_task(
            task, {ILP32: "ILP32", LP64: "LP64"}
        )
        if data_model is None:
            raise benchexec.tools.template.UnsupportedFeatureException(
                "C task has no supported data model"
            )

        cmd = [
            executable,
            "--property",
            task.property_file,
            "--data-model",
            data_model,
            "--input",
            task.single_input_file,
        ]
        if rlimits.cpu_cores is not None:
            cmd += ["--cpu-cores", str(rlimits.cpu_cores)]
        if rlimits.cputime is not None:
            cmd += ["--cpu-limit", str(rlimits.cputime)]
        if rlimits.memory is not None:
            cmd += ["--memory-limit", str(rlimits.memory)]
        for option in options:
            cmd += ["--tool-option", option]
        return cmd

    @staticmethod
    def _unique_marker(output, prefix):
        values = [line[len(prefix) :] for line in output if line.startswith(prefix)]
        return values[0] if len(values) == 1 else None

    def determine_result(self, run):
        # BenchExec-enforced resource limits take precedence over partial tool output.
        if run.was_timeout:
            return result.RESULT_TIMEOUT
        if run.termination_reason in {"memory", "memory-threshold"}:
            return "OUT OF MEMORY"
        if run.was_terminated or run.exit_code.signal is not None:
            return result.RESULT_ERROR

        verdict = self._unique_marker(run.output, _RESULT_PREFIX)
        status = self._unique_marker(run.output, _STATUS_PREFIX)
        if run.exit_code.value == 0 and verdict in _VALID_RESULTS:
            return verdict
        if status:
            return f"ERROR ({status})"
        return result.RESULT_ERROR

    def get_value_from_output(self, output, identifier):
        # Table definitions may request, for example, adapter.transforms or
        # tool.executions.  Only exact machine-readable statistic lines count.
        pattern = re.compile(
            rf"^{re.escape(_STAT_PREFIX + identifier)}=(.*)$"
        )
        values = []
        for line in output:
            match = pattern.fullmatch(line)
            if match:
                values.append(match.group(1))
        return values[-1] if values else None

