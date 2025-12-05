#pragma once

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
     * @return true if initialization succeeded, false on failure
     */
    virtual bool init() = 0;

    virtual void flush(const void* buffer, const Rect& area) = 0;
    virtual uint16_t width() const = 0;
    virtual uint16_t height() const = 0;
};

}  // namespace oc::hal
