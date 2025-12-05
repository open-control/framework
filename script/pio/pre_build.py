# pyright: reportUndefinedVariable=false, reportMissingImports=false
"""
PlatformIO pre-build script for Open Control Framework.

It handles:
1. compile_commands.json generation (for clangd IDE support)
"""
import os
import sys

Import("env")

# Add script directory to path for imports
script_dir = os.path.join(env.subst("$PROJECT_DIR"), "script/pio")
sys.path.insert(0, script_dir)

from compiledb_utils import setup_compile_commands

# Execute
setup_compile_commands(env)
