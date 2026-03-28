#include "EncoderBuilder.hpp"

#include "InputBinding.hpp"
#include <config/PlatformCompat.hpp>
#include <oc/log/Log.hpp>

namespace oc::core::input {

FLASHMEM EncoderBuilder::EncoderBuilder(InputBinding* registry, oc::type::EncoderID encoderId)
    : registry_(registry), encoderId_(encoderId) {}

FLASHMEM EncoderBuilder::~EncoderBuilder() {
    if (registry_ && !finalized_) {
        OC_LOG_WARN("{}", "[EncoderBuilder] then() was never called - binding discarded");
    }
}

FLASHMEM EncoderBuilder::EncoderBuilder(EncoderBuilder&& other) noexcept
    : registry_(other.registry_),
      encoderId_(other.encoderId_),
      scope_(other.scope_),
      isActive_(std::move(other.isActive_)),
      gestureSet_(other.gestureSet_),
      finalized_(other.finalized_) {
    other.registry_ = nullptr;
    other.finalized_ = true;
}

FLASHMEM EncoderBuilder& EncoderBuilder::operator=(EncoderBuilder&& other) noexcept {
    if (this != &other) {
        registry_ = other.registry_;
        encoderId_ = other.encoderId_;
        scope_ = other.scope_;
        isActive_ = std::move(other.isActive_);
        gestureSet_ = other.gestureSet_;
        finalized_ = other.finalized_;
        other.registry_ = nullptr;
        other.finalized_ = true;
    }
    return *this;
}

FLASHMEM EncoderBuilder& EncoderBuilder::turn() {
    if (gestureSet_) {
        OC_LOG_WARN("{}", "[EncoderBuilder] Gesture already set - ignoring turn()");
        return *this;
    }
    gestureSet_ = true;
    return *this;
}

FLASHMEM EncoderBuilder& EncoderBuilder::scope(oc::type::ScopeID s) {
    scope_ = s;
    return *this;
}

FLASHMEM EncoderBuilder& EncoderBuilder::when(oc::type::IsActiveFn fn) {
    isActive_ = std::move(fn);
    return *this;
}

FLASHMEM BindingHandle EncoderBuilder::then(oc::type::EncoderActionCallback cb) {
    finalized_ = true;

    if (!registry_) {
        OC_LOG_WARN("{}", "[EncoderBuilder] Invalid registry - returning invalid handle");
        return BindingHandle::invalid();
    }

    if (!gestureSet_) {
        OC_LOG_WARN("{}", "[EncoderBuilder] No gesture set before then() - returning invalid handle");
        return BindingHandle::invalid();
    }

    if (!cb) {
        OC_LOG_WARN("{}", "[EncoderBuilder] Null callback - returning invalid handle");
        return BindingHandle::invalid();
    }

    EncoderBinding binding{
        .id = 0,  // Will be assigned by registerEncoderBinding
        .type = EncoderBindingType::TURN,
        .encoderId = encoderId_,
        .requiredButton = std::nullopt,
        .action = std::move(cb),
        .enabled = true,
        .isActive = std::move(isActive_),
        .scopeId = scope_
    };

    oc::type::BindingID id = registry_->registerEncoderBinding(std::move(binding));
    return BindingHandle(registry_, id);
}

}  // namespace oc::core::input
