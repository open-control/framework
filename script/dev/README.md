# Development Scripts

Utility scripts for development workflow.

## Scripts

| Script | Purpose |
|--------|---------|
| `format.sh` | Format all C/C++ files in `src/` with clang-format |
| `rebuild-compiledb.sh` | Regenerate `compile_commands.json` for clangd |
| `check_architecture.py` | Enforce namespace/module dependency rules |

## Usage

From anywhere in the project:

```bash
# Format entire codebase
./script/dev/format.sh

# Rebuild compile_commands.json (after changing includes, build flags, etc.)
./script/dev/rebuild-compiledb.sh

# Architecture guardrails (namespaces + module dependencies)
python3 script/dev/check_architecture.py

# Unit tests run this check automatically (and fail fast)
uv run ms test open-control-framework
```

## Requirements

- **clang-format**: Must be in PATH (installed with LLVM or clangd extension)
- **ms-dev-env**: For the supported CMake/CTest unit-test workflow

## Shared Library

These scripts use `script/lib/common.sh` for shared utilities (colors, logging, `find_root()`).
