#pragma once

#include <array>
#include <cstddef>

#include <oc/Config.hpp>
#include <oc/type/Ids.hpp>

namespace oc::core::input {

/**
 * @brief Immutable-by-default route captured for one physical button gesture
 *
 * A route may only change through an explicit handoff. Overlay transitions can
 * quarantine it so the opening/closing release cannot leak into another scope.
 */
struct GestureRoute {
    bool active = false;
    bool consumed = false;
    bool explicitHandoff = false;
    bool globalPassThrough = false;
    oc::type::ScopeID originScope = 0;
    oc::type::ScopeID ownerScope = 0;
    oc::type::BindingID pressBinding = 0;
    oc::type::BindingID releaseBinding = 0;
    oc::type::BindingID longPressBinding = 0;
    oc::type::BindingID doubleTapBinding = 0;
    oc::type::BindingID comboBinding = 0;
};

class GestureRouteTracker {
public:
    GestureRoute& route(oc::type::ButtonID button) {
        return routes_[button];
    }

    const GestureRoute& route(oc::type::ButtonID button) const {
        return routes_[button];
    }

    void clear(oc::type::ButtonID button) {
        if (button < MAX_BUTTONS) routes_[button] = {};
    }

    void clearForScope(oc::type::ScopeID scope) {
        for (auto& route : routes_) {
            if (route.originScope == scope || route.ownerScope == scope) {
                route = {};
            }
        }
    }

    void reset() {
        routes_.fill({});
    }

private:
    std::array<GestureRoute, MAX_BUTTONS> routes_{};
};

}  // namespace oc::core::input
