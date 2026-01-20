#pragma once

/**
 * @file Ids.hpp
 * @brief Type aliases for identifiers (Level 0 - no dependencies)
 */

#include <cstdint>

namespace oc {

// ═══════════════════════════════════════════════════════════════════════════
// Input identifiers
// ═══════════════════════════════════════════════════════════════════════════

using ButtonID = uint16_t;
using EncoderID = uint16_t;

// ═══════════════════════════════════════════════════════════════════════════
// Binding system identifiers
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief Unique identifier for a registered binding
 *
 * Used internally to track and remove bindings.
 * Value 0 indicates an invalid/unregistered binding.
 */
using BindingID = uint32_t;

/**
 * @brief Unique scope identifier for binding grouping
 *
 * Purposes:
 * 1. Batch removal: clearScope(id) removes all bindings with this scope
 * 2. Priority: scoped bindings trigger before global (scope=0) bindings
 *
 * Typical values:
 * - 0: Global binding (no scope, lowest priority)
 * - Pointer cast to uintptr_t (e.g., view/screen instance)
 * - Enum value for application modes
 */
using ScopeID = uintptr_t;

}  // namespace oc
