#pragma once

#include <oc/api/ButtonAPI.hpp>

namespace oc::api {

/**
 * @brief Lightweight proxy for single button state access
 *
 * Created by IContext::button(id), provides fluent access to
 * a specific button's state without repeating the ID.
 *
 * @code
 * // Via IContext:
 * button(BTN_1).clearLatch();
 * if (button(BTN_1).isPressed()) { ... }
 *
 * // For when() predicates:
 * onEncoder(ENC_1).turn()
 *     .when(button(BTN_SHIFT).pressed())
 *     .then([](float v){ ... });
 * @endcode
 */
class ButtonProxy {
public:
    ButtonProxy(ButtonAPI& api, oc::type::ButtonID id) : api_(api), id_(id) {}

    /// Check if button is in latched state
    bool isLatched() const { return api_.isLatched(id_); }

    /// Clear button latch state
    void clearLatch() { api_.clearLatch(id_); }

    /// Check if button is currently pressed
    bool isPressed() const { return api_.isPressed(id_); }

    /// Get predicate for use with when()
    oc::type::IsActiveFn pressed() const { return api_.pressed(id_); }

private:
    ButtonAPI& api_;
    oc::type::ButtonID id_;
};

}  // namespace oc::api
