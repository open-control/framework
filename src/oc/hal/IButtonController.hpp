#pragma once

#include <oc/core/Result.hpp>

#include "Types.hpp"

namespace oc::hal {

/**
 * @brief Interface for button hardware abstraction
 */
class IButtonController {
public:
    virtual ~IButtonController() = default;

    /**
     * @brief Initialize button hardware
     * @return Result<void> - ok() on success, err() with ErrorCode on failure
     */
    virtual core::Result<void> init() = 0;

    /**
     * @brief Poll button states and trigger callbacks
     * @param currentTimeMs Current time in milliseconds (from App's TimeProvider)
     */
    virtual void update(uint32_t currentTimeMs) = 0;

    virtual bool isPressed(ButtonID id) const = 0;
    virtual void setCallback(ButtonCallback cb) = 0;
};

}  // namespace oc::hal
