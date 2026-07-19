"""BenchExec adapter for LP64 comparisons across the TruSt tool family.

These artifact binaries do not accept SV-COMP's ILP32 runtime headers.  This
adapter deliberately compiles the same C source as LP64 and keeps the results
in a separate comparison family; it must not be used for an official SV-COMP
ILP32 score table.
"""

from __future__ import annotations

import os
from pathlib import Path
import subprocess

import benchexec.result as result
import benchexec.tools.template


class Tool(benchexec.tools.template.BaseTool2):
    def name(self):
        return "TruSt-family LP64 adapter"

    def project_url(self):
        return "https://plv.mpi-sws.org/trust/"

    @staticmethod
    def _binaries() -> dict[str, Path]:
        image_binary = os.environ.get("TRUST_FAMILY_BINARY")
        image_tool = os.environ.get("TRUST_FAMILY_TOOL")
        if image_binary or image_tool:
            if not image_binary or image_tool not in {
                "trust",
                "awamoche",
                "mixer",
                "spore",
            }:
                raise benchexec.tools.template.ToolNotFoundException(
                    "TRUST_FAMILY_BINARY and a valid TRUST_FAMILY_TOOL are required"
                )
            binary = Path(image_binary)
            models = ("rc11", "sc") if image_tool in {"mixer", "spore"} else ("rc11",)
            return {
                f"{image_tool}-{model}-lp64": binary
                for model in models
            }

        root = Path(os.environ["GENMC_EXPERIMENT_ROOT"])
        trust_root = Path(
            os.environ.get(
                "TRUST_FAMILY_ROOT", str(root / "packages/trust-family")
            )
        )
        variants = trust_root / "variants"
        return {
            "genmc-rc11-lp64": root / "build/genmc-caat/bin/genmc",
            "genmc-sc-lp64": root / "build/genmc-caat/bin/genmc",
            "trust-rc11-lp64": variants / "trust-0.5.3/src/trust",
            "awamoche-rc11-lp64": variants / "awamoche-0.8/src/awamoche",
            "mixer-rc11-lp64": variants / "mixer-0.10.1/mixer",
            "mixer-sc-lp64": variants / "mixer-0.10.1/mixer",
            "spore-rc11-lp64": variants / "spore-0.10.1/spore",
            "spore-sc-lp64": variants / "spore-0.10.1/spore",
        }

    def executable(self, tool_locator):
        binary = next(iter(self._binaries().values()))
        if not binary.is_file():
            raise benchexec.tools.template.ToolNotFoundException(str(binary))
        return str(binary)

    def version(self, executable):
        versions = []
        seen: set[Path] = set()
        for binary in self._binaries().values():
            if binary in seen or not binary.is_file():
                continue
            seen.add(binary)
            completed = subprocess.run(
                [str(binary), "--version"],
                check=False,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                timeout=10,
            )
            first = next(
                (line.strip() for line in completed.stdout.splitlines() if line.strip()),
                "unknown",
            )
            versions.append(f"{binary.name}:{first}")
        return "; ".join(versions)

    def environment(self, executable):
        if os.environ.get("TRUST_FAMILY_BINARY"):
            return {}
        root = Path(os.environ["GENMC_EXPERIMENT_ROOT"])
        trust_root = Path(
            os.environ.get(
                "TRUST_FAMILY_ROOT", str(root / "packages/trust-family")
            )
        )
        runtime_libs = trust_root / "lib"
        return {"newEnv": {"LD_LIBRARY_PATH": str(runtime_libs)}}

    @staticmethod
    def _source_for(task) -> str:
        source = Path(task.single_input_file)
        if source.suffix == ".i" and source.with_suffix(".c").is_file():
            source = source.with_suffix(".c")
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
                if stripped.startswith('#include "'):
                    dependency = path.parent / stripped.split('"', 2)[1]
                    if dependency.is_file():
                        pending.append(dependency)
        return False

    def cmdline(self, executable, options, task, rlimits):
        backend = None
        forwarded = []
        for option in options:
            if option.startswith("--svcomp-backend="):
                backend = option.split("=", 1)[1]
            elif option not in {
                "--disable-estimation",
                "--disable-mm-detector",
                "--nthreads=1",
                "--v1",
            }:
                forwarded.append(option)
        binaries = self._binaries()
        if backend not in binaries:
            raise benchexec.tools.template.UnsupportedFeatureException(
                f"unknown TruSt-family backend {backend}"
            )
        binary = binaries[backend]
        if not binary.is_file():
            raise benchexec.tools.template.ToolNotFoundException(str(binary))

        model = "--sc" if "-sc-" in backend else "--rc11"
        model_options = [model]
        if backend.startswith(("trust-", "awamoche-")):
            model_options.append("--mo")
        if backend.startswith("mixer-"):
            model_options.append("--mixer")
        runtime_options = ["--disable-race-detection"]
        if not backend.startswith("trust-"):
            runtime_options.append("--nthreads=1")
        if backend.startswith(("genmc-", "mixer-", "spore-")):
            runtime_options += ["--disable-estimation", "--disable-mm-detector"]

        root = Path(os.environ["GENMC_EXPERIMENT_ROOT"])
        source = self._source_for(task)
        compiler_options = ["-m64"]
        if self._needs_atomic_compat(source):
            compiler_options += [
                "-include",
                str(root / "pipeline/include/svcomp_genmc_compat.h"),
            ]
        compiler_options += ["-DNULL=0", source]
        return [str(binary)] + model_options + runtime_options + forwarded + ["--"] + compiler_options

    def determine_result(self, run):
        output = run.output
        if run.was_timeout:
            return result.RESULT_TIMEOUT
        if run.exit_code.value == 42 or output.any_line_contains("Verification unsuccessful"):
            return result.RESULT_FALSE_REACH
        if run.exit_code.value == 0 and output.any_line_contains("No Error detected"):
            return result.RESULT_TRUE_PROP
        if run.exit_code.value == 0 and output.any_line_contains("No errors were detected"):
            return result.RESULT_TRUE_PROP
        if output.any_line_contains("Error detected"):
            return result.RESULT_FALSE_REACH
        if run.exit_code.value == 0 and (
            output.any_line_contains("Verification complete")
        ):
            return result.RESULT_TRUE_PROP
        if run.exit_code.value == 0 and output.any_line_contains(
            "Number of complete executions explored:"
        ):
            return result.RESULT_TRUE_PROP
        if output.any_line_contains("unknown external function"):
            return "ERROR (unsupported external)"
        if output.any_line_contains("error generated") or output.any_line_contains("fatal error:"):
            return "ERROR (compilation)"
        if run.exit_code.value == 5:
            return "ERROR (compilation)"
        if run.exit_code.value == 137:
            return "OUT OF MEMORY"
        return result.RESULT_ERROR
