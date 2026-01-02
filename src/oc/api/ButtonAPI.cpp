#include "ButtonAPI.hpp"

#include <oc/core/input/InputBinding.hpp>

namespace oc::api {

ButtonAPI::ButtonAPI(core::input::InputBinding& binding, hal::IButtonController& hw)
    : binding_(binding), hw_(hw) {}

core::input::ButtonBuilder ButtonAPI::button(hal::ButtonID id) {
    return core::input::ButtonBuilder(&binding_, id);
}

void ButtonAPI::clearBindings() {
    binding_.clearButtonBindings();
}

void ButtonAPI::clearScope(core::ScopeID scope) {
    binding_.clearButtonScope(scope);
}

void ButtonAPI::setAuthorityResolver(const core::input::AuthorityResolver* resolver) {
    binding_.setAuthorityResolver(resolver);
}

bool ButtonAPI::isPressed(hal::ButtonID id) const {
    return hw_.isPressed(id);
}

core::IsActiveFn ButtonAPI::pressed(hal::ButtonID id) const {
    return [this, id]() { return hw_.isPressed(id); };
}

bool ButtonAPI::isLatched(hal::ButtonID id) const {
    return binding_.isLatched(id);
}

void ButtonAPI::clearLatch(hal::ButtonID id) {
    binding_.clearLatch(id);
}

}  // namespace oc::api
