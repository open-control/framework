#pragma once

#include <oc/types/Result.hpp>
#include <oc/types/Ids.hpp>
#include <oc/types/Callbacks.hpp>

namespace oc::interface {

/**
 * @brief Interface for button hardware abstraction
 */
class IButton {
public:
    virtual ~IButton() = default;

    /**
     * @brief Initialize button hardware
     * @return Result<void> - ok() on success, err() with ErrorCode on failure
     */
    virtual oc::Result<void> init() = 0;

    /**
     * @brief Poll button states and trigger callbacks
     * @param currentTimeMs Current time in milliseconds (from App's TimeProvider)
     */
    virtual void update(uint32_t currentTimeMs) = 0;

    virtual bool isPressed(oc::ButtonID id) const = 0;
    virtual void setCallback(oc::ButtonCallback cb) = 0;
};

}  // namespace oc::interface
