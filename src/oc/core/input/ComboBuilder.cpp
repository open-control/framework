#include "ComboBuilder.hpp"

#include "InputBinding.hpp"

namespace oc::core::input {

ComboBuilder::ComboBuilder(InputBinding* registry, ButtonID btn1, ButtonID btn2)
    : registry_(registry), btn1_(btn1), btn2_(btn2) {
    // Validate: combo with same button is invalid
    if (btn1 == btn2) {
        warn("[ComboBuilder] Combo with same button is invalid");
        registry_ = nullptr;  // Mark as invalid
    }
}

ComboBuilder::~ComboBuilder() {
    if (registry_ && !finalized_) {
        warn("[ComboBuilder] then() was never called - binding discarded");
    }
}

ComboBuilder::ComboBuilder(ComboBuilder&& other) noexcept
    : registry_(other.registry_),
      btn1_(other.btn1_),
      btn2_(other.btn2_),
      scope_(other.scope_),
      isActive_(std::move(other.isActive_)),
      finalized_(other.finalized_) {
    other.registry_ = nullptr;
    other.finalized_ = true;
}

ComboBuilder& ComboBuilder::operator=(ComboBuilder&& other) noexcept {
    if (this != &other) {
        registry_ = other.registry_;
        btn1_ = other.btn1_;
        btn2_ = other.btn2_;
        scope_ = other.scope_;
        isActive_ = std::move(other.isActive_);
        finalized_ = other.finalized_;
        other.registry_ = nullptr;
        other.finalized_ = true;
    }
    return *this;
}

ComboBuilder& ComboBuilder::scope(ScopeID s) {
    scope_ = s;
    return *this;
}

ComboBuilder& ComboBuilder::when(IsActiveFn fn) {
    isActive_ = std::move(fn);
    return *this;
}

BindingHandle ComboBuilder::then(ActionCallback cb) {
    finalized_ = true;

    if (!registry_) {
        warn("[ComboBuilder] Invalid registry - returning invalid handle");
        return BindingHandle::invalid();
    }

    if (!cb) {
        warn("[ComboBuilder] Null callback - returning invalid handle");
        return BindingHandle::invalid();
    }

    ButtonBinding binding{
        .id = 0,  // Will be assigned by registerButtonBinding
        .type = ButtonBindingType::COMBO,
        .buttonId = btn1_,
        .secondaryButton = btn2_,
        .longPressMs = 0,
        .doubleTapWindowMs = 0,
        .action = std::move(cb),
        .enabled = true,
        .latch = false,
        .isActive = std::move(isActive_),
        .scopeId = scope_
    };

    BindingID id = registry_->registerButtonBinding(std::move(binding));
    return BindingHandle(registry_, id);
}

}  // namespace oc::core::input
