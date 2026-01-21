#pragma once

/**
 * @file LatchManager.hpp
 * @brief Centralized latch state management for buttons
 *
 * Latch allows a button press to "stick" - the button stays logically pressed
 * until pressed again. This is useful for shift-like modifiers.
 */

#include <array>

#include <oc/Config.hpp>
#include <oc/type/Ids.hpp>

namespace oc::core::input {

/**
 * @brief Manages latch state for buttons
 *
 * A latched button:
 * - Blocks its owning scope from receiving new press events
 * - Releases on the next press (toggle behavior)
 * - Is owned by a specific scope
 */
class LatchManager {
public:
    /// Check if button is latched by any scope
    bool isLatched(oc::type::ButtonID btn) const {
        if (btn >= MAX_BUTTONS) return false;
        return owner_[btn] != 0;
    }

    /// Get the scope that owns the latch (0 = not latched)
    oc::type::ScopeID owner(oc::type::ButtonID btn) const {
        if (btn >= MAX_BUTTONS) return 0;
        return owner_[btn];
    }

    /// Activate latch for a button with given scope
    void activate(oc::type::ButtonID btn, oc::type::ScopeID scope) {
        if (btn < MAX_BUTTONS) {
            owner_[btn] = scope;
        }
    }

    /// Release latch for a button
    void release(oc::type::ButtonID btn) {
        if (btn < MAX_BUTTONS) {
            owner_[btn] = 0;
        }
    }

    /// Release all latches owned by a scope
    void releaseForScope(oc::type::ScopeID scope) {
        for (size_t i = 0; i < MAX_BUTTONS; ++i) {
            if (owner_[i] == scope) {
                owner_[i] = 0;
            }
        }
    }

    /// Clear all latches
    void reset() {
        owner_.fill(0);
    }

private:
    std::array<oc::type::ScopeID, MAX_BUTTONS> owner_{};
};

}  // namespace oc::core::input
