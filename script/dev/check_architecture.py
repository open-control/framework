#!/usr/bin/env python3

"""Architecture guardrails for the Open Control framework.

Checks:
- Folder <-> namespace coherence (namespace must match src/oc/** path)
- One-way dependencies between modules (no upward / forbidden includes)
- No `using namespace` directives in headers

Run:
  python3 script/dev/check_architecture.py
"""

from __future__ import annotations

import re
import sys
from dataclasses import dataclass
from pathlib import Path


RE_INCLUDE = re.compile(r"^\s*#\s*include\s*([<\"])([^>\"]+)[>\"]")
RE_USING_NAMESPACE = re.compile(r"^\s*using\s+namespace\s+")


def repo_root() -> Path:
    # script/dev/<this file>
    return Path(__file__).resolve().parents[2]


def module_of_oc_path(oc_rel: str) -> str:
    # oc_rel is like "type/Result.hpp" or "Config.hpp"
    parts = Path(oc_rel).parts
    if len(parts) <= 1:
        return "oc"
    return parts[0]


def module_of_file(src_oc: Path, file_path: Path) -> str:
    rel = file_path.relative_to(src_oc)
    if len(rel.parts) == 1:
        return "oc"
    return rel.parts[0]


def expected_namespace(src_oc: Path, file_path: Path) -> str:
    rel = file_path.relative_to(src_oc)
    if len(rel.parts) == 1:
        return "oc"
    # src/oc/<module>/<subdirs...>/<File>
    return "oc::" + "::".join(rel.parts[:-1])


@dataclass(frozen=True)
class Finding:
    kind: str
    file: Path
    line: int
    message: str


def check_using_namespace_in_headers(src_oc: Path) -> list[Finding]:
    findings: list[Finding] = []
    for header in sorted(src_oc.rglob("*.hpp")):
        for idx, line in enumerate(header.read_text(errors="ignore").splitlines(), start=1):
            if RE_USING_NAMESPACE.match(line):
                findings.append(
                    Finding(
                        kind="using-namespace",
                        file=header,
                        line=idx,
                        message="'using namespace' is forbidden in headers",
                    )
                )
    return findings


def check_namespace_matches_path(src_oc: Path) -> list[Finding]:
    findings: list[Finding] = []
    for file_path in sorted(list(src_oc.rglob("*.hpp")) + list(src_oc.rglob("*.cpp"))):
        exp = expected_namespace(src_oc, file_path)
        txt = file_path.read_text(errors="ignore")
        if re.search(r"\bnamespace\s+" + re.escape(exp) + r"\b", txt) is None:
            findings.append(
                Finding(
                    kind="namespace",
                    file=file_path,
                    line=1,
                    message=f"expected namespace '{exp}' (namespace must match folder path)",
                )
            )
    return findings


def resolve_include(src_oc: Path, including_file: Path, delim: str, inc_path: str) -> str | None:
    # Returns included module name if include resolves into src/oc/**, else None.
    if delim == "<" and inc_path.startswith("oc/"):
        return module_of_oc_path(inc_path[len("oc/") :])

    if inc_path.startswith("oc/"):
        return module_of_oc_path(inc_path[len("oc/") :])

    if delim == '"':
        resolved = (including_file.parent / inc_path).resolve()
        if resolved.exists():
            try:
                if resolved.is_relative_to(src_oc):
                    return module_of_file(src_oc, resolved)
            except AttributeError:
                # Python < 3.9 fallback (not expected here)
                try:
                    resolved.relative_to(src_oc)
                    return module_of_file(src_oc, resolved)
                except ValueError:
                    return None

    return None


def check_module_dependencies(src_oc: Path) -> list[Finding]:
    # Explicit allowed dependencies between top-level modules.
    # Rule: a module may include itself + its allowed dependencies only.
    allowed: dict[str, set[str]] = {
        # Foundation
        "oc": set(),
        "type": set(),
        "util": set(),
        "codec": set(),
        "realtime": set(),
        "time": {"type"},
        "log": {"time"},
        "debug": {"log"},

        # Level 1
        "interface": {"type"},

        # Level 2
        "impl": {"interface", "type"},
        "core": {"interface", "type", "oc", "log", "time", "util", "codec", "debug", "realtime"},
        "state": {"interface", "type", "oc", "log", "time", "util", "codec", "debug", "realtime"},

        # Level 3
        "api": {"core", "interface", "type", "oc", "log", "time", "util", "codec", "debug", "realtime"},
        "context": {"api", "core", "interface", "type", "oc", "log", "time", "util", "codec", "debug", "state", "realtime"},

        # Level 4
        "app": {"context", "api", "core", "state", "impl", "interface", "type", "oc", "log", "time", "util", "codec", "debug", "realtime"},
    }

    known_modules = set(allowed)

    findings: list[Finding] = []
    source_files = sorted(list(src_oc.rglob("*.hpp")) + list(src_oc.rglob("*.cpp")))
    for file_path in source_files:
        from_mod = module_of_file(src_oc, file_path)
        if from_mod not in known_modules:
            findings.append(
                Finding(
                    kind="module",
                    file=file_path,
                    line=1,
                    message=f"unknown module '{from_mod}' (update script/dev/check_architecture.py)",
                )
            )
            continue

        txt = file_path.read_text(errors="ignore").splitlines()
        for idx, line in enumerate(txt, start=1):
            m = RE_INCLUDE.match(line)
            if not m:
                continue
            delim, inc_path = m.group(1), m.group(2)
            to_mod = resolve_include(src_oc, file_path, delim, inc_path)
            if to_mod is None:
                continue
            if to_mod not in known_modules:
                findings.append(
                    Finding(
                        kind="module",
                        file=file_path,
                        line=idx,
                        message=f"include targets unknown module '{to_mod}' ({inc_path})",
                    )
                )
                continue

            if to_mod == from_mod:
                continue

            if to_mod not in allowed[from_mod]:
                findings.append(
                    Finding(
                        kind="dependency",
                        file=file_path,
                        line=idx,
                        message=f"forbidden dependency: {from_mod} -> {to_mod} ({inc_path})",
                    )
                )

    return findings


def main() -> int:
    root = repo_root()
    src_oc = root / "src" / "oc"

    if not src_oc.exists():
        print(f"ERROR: expected directory not found: {src_oc}")
        return 2

    findings: list[Finding] = []
    findings.extend(check_using_namespace_in_headers(src_oc))
    findings.extend(check_namespace_matches_path(src_oc))
    findings.extend(check_module_dependencies(src_oc))

    if findings:
        for f in findings:
            rel = f.file.relative_to(root)
            print(f"{f.kind}: {rel}:{f.line}: {f.message}")

        rules_doc = root / "ARCHITECTURE.md"
        print(f"\nFAILED: {len(findings)} issue(s)")
        print("\nHow to fix:")
        print("- dependency: remove forbidden include; move shared types down to type/ or interface/; or depend on an interface instead")
        print("- namespace: update the file's namespace to match its folder path, or move the file to the matching folder")
        print("- using-namespace: replace with explicit symbol re-exports (using alias / inline constexpr), never 'using namespace' in headers")
        print(f"\nRules / include matrix: {rules_doc.relative_to(root)}")
        return 1

    print("OK: architecture checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
