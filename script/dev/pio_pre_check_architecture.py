Import("env")

import os
import subprocess
import sys


def run_architecture_check() -> None:
    project_dir = env.subst("$PROJECT_DIR")
    check_script = os.path.join(project_dir, "script", "dev", "check_architecture.py")
    rules_doc = os.path.join(project_dir, "ARCHITECTURE.md")

    print("[oc] architecture: running guardrails...")
    result = subprocess.run([sys.executable, check_script], cwd=project_dir)

    if result.returncode != 0:
        print("\n[oc] architecture: FAILED")
        print("[oc] Fix the issues above, then re-run: uv run ms test open-control-framework")
        print(f"[oc] Rules / include matrix: {rules_doc}")
        env.Exit(1)


run_architecture_check()
