#pragma once

#include "Types.hpp"

namespace oc::hal {

/**
 * @brief Interface for button hardware abstraction
 */
class IButtonController {
public:
    virtual ~IButtonController() = default;

    virtual void init() = 0;
    virtual void update() = 0;

    virtual bool isPressed(ButtonID id) const = 0;
    virtual void setCallback(ButtonCallback cb) = 0;
};

}  // namespace oc::hal
