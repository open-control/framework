#!/bin/bash
# Rebuild compile_commands.json for clangd

source "$(dirname "${BASH_SOURCE[0]}")/../lib/common.sh"

PROJECT_ROOT="$(find_root)"

# --- Main ---
echo -e "\n${BLUE}═══ Rebuild compile_commands.json ═══${NC}"
echo -e "${DIM}Root: $PROJECT_ROOT${NC}"

cd "$PROJECT_ROOT"

require_cmd pio

log "Cleaning build artifacts..."
pio run -t clean 2>/dev/null || true

log "Rebuilding to generate compile_commands.json..."
pio run

echo ""
success "Done! Now restart clangd in VS Code:"
echo -e "${DIM}  Ctrl+Shift+P → 'clangd: Restart language server'${NC}"
