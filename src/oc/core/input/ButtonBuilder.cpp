#include "ButtonBuilder.hpp"

#include "ComboBuilder.hpp"
#include "InputBinding.hpp"
#include <config/PlatformCompat.hpp>
#include <oc/log/Log.hpp>

namespace oc::core::input {

FLASHMEM ButtonBuilder::ButtonBuilder(InputBinding* registry, oc::type::ButtonID buttonId)
    : registry_(registry), buttonId_(buttonId) {}

FLASHMEM ButtonBuilder::~ButtonBuilder() {
    if (registry_ && !finalized_) {
        OC_LOG_WARN("{}", "[ButtonBuilder] then() was never called - binding discarded");
    }
}

FLASHMEM ButtonBuilder::ButtonBuilder(ButtonBuilder&& other) noexcept
    : registry_(other.registry_),
      buttonId_(other.buttonId_),
      type_(other.type_),
      timingMs_(other.timingMs_),
      scope_(other.scope_),
      isActive_(std::move(other.isActive_)),
      latch_(other.latch_),
      globalPassThrough_(other.globalPassThrough_),
      priority_(other.priority_),
      gestureSet_(other.gestureSet_),
      finalized_(other.finalized_) {
    other.registry_ = nullptr;
    other.finalized_ = true;  // Prevent warning on moved-from object
}

FLASHMEM ButtonBuilder& ButtonBuilder::operator=(ButtonBuilder&& other) noexcept {
    if (this != &other) {
        registry_ = other.registry_;
        buttonId_ = other.buttonId_;
        type_ = other.type_;
        timingMs_ = other.timingMs_;
        scope_ = other.scope_;
        isActive_ = std::move(other.isActive_);
        latch_ = other.latch_;
        globalPassThrough_ = other.globalPassThrough_;
        priority_ = other.priority_;
        gestureSet_ = other.gestureSet_;
        finalized_ = other.finalized_;
        other.registry_ = nullptr;
        other.finalized_ = true;
    }
    return *this;
}

FLASHMEM ButtonBuilder& ButtonBuilder::press() {
    if (gestureSet_) {
        OC_LOG_WARN("{}", "[ButtonBuilder] Gesture already set - ignoring press()");
        return *this;
    }
    type_ = ButtonBindingType::PRESS;
    gestureSet_ = true;
    return *this;
}

FLASHMEM ButtonBuilder& ButtonBuilder::release() {
    if (gestureSet_) {
        OC_LOG_WARN("{}", "[ButtonBuilder] Gesture already set - ignoring release()");
        return *this;
    }
    type_ = ButtonBindingType::RELEASE;
    gestureSet_ = true;
    return *this;
}

FLASHMEM ButtonBuilder& ButtonBuilder::longPress(uint32_t ms) {
    if (gestureSet_) {
        OC_LOG_WARN("{}", "[ButtonBuilder] Gesture already set - ignoring longPress()");
        return *this;
    }
    type_ = ButtonBindingType::LONG_PRESS;
    timingMs_ = ms;
    gestureSet_ = true;
    return *this;
}

FLASHMEM ButtonBuilder& ButtonBuilder::doubleTap(uint32_t ms) {
    if (gestureSet_) {
        OC_LOG_WARN("{}", "[ButtonBuilder] Gesture already set - ignoring doubleTap()");
        return *this;
    }
    type_ = ButtonBindingType::DOUBLE_TAP;
    timingMs_ = ms;
    gestureSet_ = true;
    return *this;
}

FLASHMEM ComboBuilder ButtonBuilder::combo(oc::type::ButtonID other) {
    finalized_ = true;  // Ownership transferred to ComboBuilder
    return ComboBuilder(registry_, buttonId_, other);
}

FLASHMEM ButtonBuilder& ButtonBuilder::scope(oc::type::ScopeID s) {
    scope_ = s;
    return *this;
}

FLASHMEM ButtonBuilder& ButtonBuilder::when(oc::type::IsActiveFn fn) {
    isActive_ = std::move(fn);
    return *this;
}

FLASHMEM ButtonBuilder& ButtonBuilder::latch() {
    latch_ = true;
    return *this;
}

FLASHMEM ButtonBuilder& ButtonBuilder::globalPassThrough() {
    globalPassThrough_ = true;
    return *this;
}

FLASHMEM ButtonBuilder& ButtonBuilder::priority(int8_t value) {
    priority_ = value;
    return *this;
}

FLASHMEM BindingHandle ButtonBuilder::then(oc::type::ActionCallback cb) {
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

    if (globalPassThrough_ && scope_ != 0) {
        OC_LOG_ERROR("[ButtonBuilder] globalPassThrough requires global scope button={} scope={}",
                     static_cast<unsigned>(buttonId_),
                     static_cast<unsigned>(scope_));
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
        .globalPassThrough = globalPassThrough_,
        .isActive = std::move(isActive_),
        .scopeId = scope_,
        .priority = priority_
    };

    oc::type::BindingID id = registry_->registerButtonBinding(std::move(binding));
    if (id == 0) {
        OC_LOG_ERROR("[ButtonBuilder] failed to register binding button={} type={} scope={} latch={} used={}/{}",
                     static_cast<unsigned>(buttonId_),
                     static_cast<unsigned>(type_),
                     static_cast<unsigned>(scope_),
                     latch_ ? 1U : 0U,
                     static_cast<unsigned>(registry_->buttonBindingCount()),
                     static_cast<unsigned>(registry_->buttonBindingCapacity()));
    }
    return BindingHandle(registry_, id);
}

}  // namespace oc::core::input
