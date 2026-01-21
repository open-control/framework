#pragma once

/**
 * @file OwnershipTracker.hpp
 * @brief Tracks which scope owns each button's current press
 *
 * When a scoped binding handles a button press, that scope "owns" the press
 * and will receive the corresponding release event.
 */

#include <array>

#include <oc/Config.hpp>
#include <oc/type/Ids.hpp>

namespace oc::core::input {

/**
 * @brief Tracks press ownership for buttons
 *
 * Ensures press/release events are paired to the same scope.
 */
class OwnershipTracker {
public:
    /// Get the scope that owns this button's press (0 = no owner)
    oc::type::ScopeID owner(oc::type::ButtonID btn) const {
        if (btn >= MAX_BUTTONS) return 0;
        return owner_[btn];
    }

    /// Set ownership for a button press
    void setOwner(oc::type::ButtonID btn, oc::type::ScopeID scope) {
        if (btn < MAX_BUTTONS) {
            owner_[btn] = scope;
        }
    }

    /// Clear ownership (typically on release)
    void clear(oc::type::ButtonID btn) {
        if (btn < MAX_BUTTONS) {
            owner_[btn] = 0;
        }
    }

    /// Clear ownership for all buttons owned by a scope
    void clearForScope(oc::type::ScopeID scope) {
        for (size_t i = 0; i < MAX_BUTTONS; ++i) {
            if (owner_[i] == scope) {
                owner_[i] = 0;
            }
        }
    }

    /// Clear all ownership
    void reset() {
        owner_.fill(0);
    }

private:
    std::array<oc::type::ScopeID, MAX_BUTTONS> owner_{};
};

}  // namespace oc::core::input
