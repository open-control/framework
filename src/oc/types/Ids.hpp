#pragma once

/**
 * @file Ids.hpp
 * @brief Type aliases for identifiers (Level 0 - no dependencies)
 */

#include <cstdint>
#include <type_traits>

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

// ═══════════════════════════════════════════════════════════════════════════
// Type traits for ID validation
// ═══════════════════════════════════════════════════════════════════════════

namespace detail {

/// Helper to detect enum class with uint16_t underlying type
template <typename T, bool IsEnum = std::is_enum_v<T>>
struct is_uint16_enum : std::false_type {};

template <typename T>
struct is_uint16_enum<T, true>
    : std::bool_constant<std::is_same_v<std::underlying_type_t<T>, uint16_t>> {};

}  // namespace detail

/// @brief Type trait to detect enum class with uint16_t underlying type
/// Used to accept both raw uint16_t IDs and enum class IDs in APIs
template <typename T>
inline constexpr bool is_id_v = detail::is_uint16_enum<T>::value;

}  // namespace oc
