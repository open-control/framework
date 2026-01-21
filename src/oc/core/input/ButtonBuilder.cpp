#include "ButtonBuilder.hpp"

#include "ComboBuilder.hpp"
#include "InputBinding.hpp"
#include <oc/log/Log.hpp>

namespace oc::core::input {

ButtonBuilder::ButtonBuilder(InputBinding* registry, oc::type::ButtonID buttonId)
    : registry_(registry), buttonId_(buttonId) {}

ButtonBuilder::~ButtonBuilder() {
    if (registry_ && !finalized_) {
        OC_LOG_WARN("{}", "[ButtonBuilder] then() was never called - binding discarded");
    }
}

ButtonBuilder::ButtonBuilder(ButtonBuilder&& other) noexcept
    : registry_(other.registry_),
      buttonId_(other.buttonId_),
      type_(other.type_),
      timingMs_(other.timingMs_),
      scope_(other.scope_),
      isActive_(std::move(other.isActive_)),
      latch_(other.latch_),
      gestureSet_(other.gestureSet_),
      finalized_(other.finalized_) {
    other.registry_ = nullptr;
    other.finalized_ = true;  // Prevent warning on moved-from object
}

ButtonBuilder& ButtonBuilder::operator=(ButtonBuilder&& other) noexcept {
    if (this != &other) {
        registry_ = other.registry_;
        buttonId_ = other.buttonId_;
        type_ = other.type_;
        timingMs_ = other.timingMs_;
        scope_ = other.scope_;
        isActive_ = std::move(other.isActive_);
        latch_ = other.latch_;
        gestureSet_ = other.gestureSet_;
        finalized_ = other.finalized_;
        other.registry_ = nullptr;
        other.finalized_ = true;
    }
    return *this;
}

ButtonBuilder& ButtonBuilder::press() {
    if (gestureSet_) {
        OC_LOG_WARN("{}", "[ButtonBuilder] Gesture already set - ignoring press()");
        return *this;
    }
    type_ = ButtonBindingType::PRESS;
    gestureSet_ = true;
    return *this;
}

ButtonBuilder& ButtonBuilder::release() {
    if (gestureSet_) {
        OC_LOG_WARN("{}", "[ButtonBuilder] Gesture already set - ignoring release()");
        return *this;
    }
    type_ = ButtonBindingType::RELEASE;
    gestureSet_ = true;
    return *this;
}

ButtonBuilder& ButtonBuilder::longPress(uint32_t ms) {
    if (gestureSet_) {
        OC_LOG_WARN("{}", "[ButtonBuilder] Gesture already set - ignoring longPress()");
        return *this;
    }
    type_ = ButtonBindingType::LONG_PRESS;
    timingMs_ = ms;
    gestureSet_ = true;
    return *this;
}

ButtonBuilder& ButtonBuilder::doubleTap(uint32_t ms) {
    if (gestureSet_) {
        OC_LOG_WARN("{}", "[ButtonBuilder] Gesture already set - ignoring doubleTap()");
        return *this;
    }
    type_ = ButtonBindingType::DOUBLE_TAP;
    timingMs_ = ms;
    gestureSet_ = true;
    return *this;
}

ComboBuilder ButtonBuilder::combo(oc::type::ButtonID other) {
    finalized_ = true;  // Ownership transferred to ComboBuilder
    return ComboBuilder(registry_, buttonId_, other);
}

ButtonBuilder& ButtonBuilder::scope(oc::type::ScopeID s) {
    scope_ = s;
    return *this;
}

ButtonBuilder& ButtonBuilder::when(oc::type::IsActiveFn fn) {
    isActive_ = std::move(fn);
    return *this;
}

ButtonBuilder& ButtonBuilder::latch() {
    latch_ = true;
    return *this;
}

BindingHandle ButtonBuilder::then(oc::type::ActionCallback cb) {
    finalized_ = true;

    if (!registry_) {
        OC_LOG_WARN("{}", "[ButtonBuilder] Invalid registry - returning invalid handle");
        return BindingHandle::invalid();
    }

    if (!gestureSet_) {
        OC_LOG_WARN("{}", "[ButtonBuilder] No gesture set before then() - returning invalid handle");
        return BindingHandle::invalid();
    }

    if (!cb) {
        OC_LOG_WARN("{}", "[ButtonBuilder] Null callback - returning invalid handle");
        return BindingHandle::invalid();
    }

    // Apply config defaults for timing if needed
    uint32_t timing = timingMs_;
    if (timing == 0) {
        const auto& config = registry_->config();
        if (type_ == ButtonBindingType::LONG_PRESS) {
            timing = config.longPressMs;
        } else if (type_ == ButtonBindingType::DOUBLE_TAP) {
            timing = config.doubleTapWindowMs;
        }
    }

    ButtonBinding binding{
        .id = 0,  // Will be assigned by registerButtonBinding
        .type = type_,
        .buttonId = buttonId_,
        .secondaryButton = std::nullopt,
        .longPressMs = (type_ == ButtonBindingType::LONG_PRESS) ? timing : 0,
        .doubleTapWindowMs = (type_ == ButtonBindingType::DOUBLE_TAP) ? timing : 0,
        .action = std::move(cb),
        .enabled = true,
        .latch = latch_,
        .isActive = std::move(isActive_),
        .scopeId = scope_
    };

    oc::type::BindingID id = registry_->registerButtonBinding(std::move(binding));
    return BindingHandle(registry_, id);
}

}  // namespace oc::core::input
