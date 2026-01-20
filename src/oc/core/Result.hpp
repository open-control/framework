#pragma once

/**
 * @file Result.hpp
 * @brief DEPRECATED - Use <oc/types/Result.hpp> instead
 *
 * This file is kept for backwards compatibility during migration.
 * It will be removed in a future version.
 */

#include <oc/types/Result.hpp>

namespace oc::core {

// Re-export from oc:: namespace for backwards compatibility
using oc::ErrorCode;
using oc::errorCodeToString;
using oc::Error;
using oc::Result;

}  // namespace oc::core
