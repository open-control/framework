#pragma once

#include <oc/type/Result.hpp>
#include <oc/type/Ids.hpp>
#include <oc/type/Callbacks.hpp>

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
    virtual oc::type::Result<void> init() = 0;

    /**
     * @brief Poll button states and trigger callbacks
     * @param currentTimeMs Current time in milliseconds (from App's oc::type::TimeProvider)
     */
    virtual void update(uint32_t currentTimeMs) = 0;

    virtual bool isPressed(oc::type::ButtonID id) const = 0;
    virtual void setCallback(oc::type::ButtonCallback cb) = 0;
};

}  // namespace oc::interface
