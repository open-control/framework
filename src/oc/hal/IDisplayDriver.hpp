#pragma once

#include <oc/core/Result.hpp>

#include "Types.hpp"

namespace oc::hal {

/**
 * @brief Interface for display hardware abstraction
 */
class IDisplayDriver {
public:
    virtual ~IDisplayDriver() = default;

    /**
     * @brief Initialize display hardware
     * @return Result<void> - ok() on success, err() with ErrorCode on failure
     */
    virtual core::Result<void> init() = 0;

    virtual void flush(const void* buffer, const Rect& area) = 0;
    virtual uint16_t width() const = 0;
    virtual uint16_t height() const = 0;
};

}  // namespace oc::hal
