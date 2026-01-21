#include "ComboBuilder.hpp"

#include "InputBinding.hpp"
#include <oc/log/Log.hpp>

namespace oc::core::input {

ComboBuilder::ComboBuilder(InputBinding* registry, oc::type::ButtonID btn1, oc::type::ButtonID btn2)
    : registry_(registry), btn1_(btn1), btn2_(btn2) {
    // Validate: combo with same button is invalid
    if (btn1 == btn2) {
        OC_LOG_WARN("{}", "[ComboBuilder] Combo with same button is invalid");
        registry_ = nullptr;  // Mark as invalid
    }
}

ComboBuilder::~ComboBuilder() {
    if (registry_ && !finalized_) {
        OC_LOG_WARN("{}", "[ComboBuilder] then() was never called - binding discarded");
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

ComboBuilder& ComboBuilder::scope(oc::type::ScopeID s) {
    scope_ = s;
    return *this;
}

ComboBuilder& ComboBuilder::when(oc::type::IsActiveFn fn) {
    isActive_ = std::move(fn);
    return *this;
}

BindingHandle ComboBuilder::then(oc::type::ActionCallback cb) {
    finalized_ = true;

    if (!registry_) {
        OC_LOG_WARN("{}", "[ComboBuilder] Invalid registry - returning invalid handle");
        return BindingHandle::invalid();
    }

    if (!cb) {
        OC_LOG_WARN("{}", "[ComboBuilder] Null callback - returning invalid handle");
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

    oc::type::BindingID id = registry_->registerButtonBinding(std::move(binding));
    return BindingHandle(registry_, id);
}

}  // namespace oc::core::input
