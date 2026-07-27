#include "ButtonAPI.hpp"

#include <oc/core/input/InputBinding.hpp>

namespace oc::api {

ButtonAPI::AuthorityResolverHandle::AuthorityResolverHandle(AuthorityResolverHandle&& other) noexcept
    : binding_(other.binding_), token_(other.token_) {
    other.binding_ = nullptr;
    other.token_ = 0;
}

ButtonAPI::AuthorityResolverHandle& ButtonAPI::AuthorityResolverHandle::operator=(AuthorityResolverHandle&& other) noexcept {
    if (this == &other) return *this;
    reset();
    binding_ = other.binding_;
    token_ = other.token_;
    other.binding_ = nullptr;
    other.token_ = 0;
    return *this;
}

ButtonAPI::AuthorityResolverHandle::~AuthorityResolverHandle() {
    reset();
}

void ButtonAPI::AuthorityResolverHandle::reset() {
    if (binding_ && token_ != 0) {
        binding_->clearAuthorityResolver(token_);
    }
    binding_ = nullptr;
    token_ = 0;
}

ButtonAPI::ButtonAPI(core::input::InputBinding& binding, interface::IButton& hw)
    : binding_(binding), hw_(hw) {}

core::input::ButtonBuilder ButtonAPI::button(oc::type::ButtonID id) {
    return core::input::ButtonBuilder(&binding_, id);
}

void ButtonAPI::clearBindings() {
    binding_.clearButtonBindings();
}

void ButtonAPI::clearScope(oc::type::ScopeID scope) {
    binding_.clearButtonScope(scope);
}

size_t ButtonAPI::bindingCount() const {
    return binding_.buttonBindingCount();
}

size_t ButtonAPI::bindingCapacity() const {
    return binding_.buttonBindingCapacity();
}

void ButtonAPI::setAuthorityResolver(const core::input::AuthorityResolver* resolver) {
    binding_.setAuthorityResolver(resolver);
}

ButtonAPI::AuthorityResolverHandle ButtonAPI::setAuthorityResolverScoped(const core::input::AuthorityResolver* resolver) {
    uint32_t token = binding_.setAuthorityResolverScoped(resolver);
    return AuthorityResolverHandle(&binding_, token);
}

bool ButtonAPI::isPressed(oc::type::ButtonID id) const {
    return hw_.isPressed(id);
}

void ButtonAPI::handoffPress(oc::type::ButtonID id, oc::type::ScopeID scope) {
    binding_.handoffPress(id, scope);
}

void ButtonAPI::consumePress(oc::type::ButtonID id) {
    binding_.consumePress(id);
}

void ButtonAPI::quarantinePressedButtons() {
    binding_.quarantinePressedButtons();
}

oc::type::IsActiveFn ButtonAPI::pressed(oc::type::ButtonID id) const {
    return [this, id]() { return hw_.isPressed(id); };
}

bool ButtonAPI::isLatched(oc::type::ButtonID id) const {
    return binding_.isLatched(id);
}

void ButtonAPI::clearLatch(oc::type::ButtonID id) {
    binding_.clearLatch(id);
}

}  // namespace oc::api
