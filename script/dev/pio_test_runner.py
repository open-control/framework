#!/usr/bin/env python3

"""PlatformIO native test runner wrapper.

Goal:
- Run architecture guardrails before executing the compiled test binary.
- Print actionable errors (and link to ARCHITECTURE.md) when guardrails fail.

This script is invoked via `test_testing_command` in platformio.ini.
"""

from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: pio_test_runner.py <test-program> [args...]")
        return 2

    program = sys.argv[1]
    program_args = sys.argv[2:]

    project_dir = Path(__file__).resolve().parents[2]
    check_script = project_dir / "script" / "dev" / "check_architecture.py"
    rules_doc = project_dir / "ARCHITECTURE.md"

    # Capture output to keep successful runs quiet. On failure, print full context.
    check = subprocess.run(
        [sys.executable, str(check_script)],
        cwd=project_dir,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
    )

    if check.returncode != 0:
        sys.stdout.write(check.stdout)
        sys.stdout.write("\n[oc] architecture: blocked test execution\n")
        sys.stdout.write(f"[oc] Rules / include matrix: {rules_doc}\n")
        return check.returncode

    # Run the actual test program (Unity / native runner)
    return subprocess.call([program, *program_args], cwd=project_dir)


if __name__ == "__main__":
    raise SystemExit(main())
