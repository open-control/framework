#include "InputBinding.hpp"

#include <config/PlatformCompat.hpp>
#include <oc/core/event/Events.hpp>
#include <oc/log/Log.hpp>

namespace oc::core::input {

using namespace event;

namespace {

#ifdef OC_INPUT_BINDING_TRACE_NAV
constexpr bool INPUT_BINDING_TRACE_NAV = true;
#else
constexpr bool INPUT_BINDING_TRACE_NAV = false;
#endif

}  // namespace

// ═══════════════════════════════════════════════════════════════════════════
// Construction / Destruction
// ═══════════════════════════════════════════════════════════════════════════

FLASHMEM InputBinding::InputBinding(interface::IEventBus& eventBus,
                                    oc::type::TimeProvider timeProvider,
                                    const InputConfig& config)
    : button_registry_(MAX_BUTTON_BINDINGS, next_binding_id_),
      encoder_registry_(MAX_ENCODER_BINDINGS, next_binding_id_),
      gesture_(config),
      event_bus_(eventBus),
      time_provider_(timeProvider),
      config_(config) {

    if (!time_provider_) {
        OC_LOG_WARN("{}", "[InputBinding] No oc::type::TimeProvider - long press and double tap detection disabled");
    }

    encoder_sub_ = event_bus_.on(oc::type::EventCategory::USER_INPUT, InputEvent::ENCODER_CHANGED,
                                 [this](const oc::type::Event& e) { onEncoderChanged(e); });

    button_press_sub_ = event_bus_.on(oc::type::EventCategory::USER_INPUT, InputEvent::BUTTON_PRESS,
                                      [this](const oc::type::Event& e) { onButtonPress(e); });

    button_release_sub_ = event_bus_.on(oc::type::EventCategory::USER_INPUT, InputEvent::BUTTON_RELEASE,
                                        [this](const oc::type::Event& e) { onButtonRelease(e); });
}

FLASHMEM InputBinding::~InputBinding() {
    event_bus_.off(encoder_sub_);
    event_bus_.off(button_press_sub_);
    event_bus_.off(button_release_sub_);
}

// ═══════════════════════════════════════════════════════════════════════════
// Public API - Scope Management
// ═══════════════════════════════════════════════════════════════════════════

FLASHMEM void InputBinding::clearScope(oc::type::ScopeID scope) {
    button_registry_.clearScope(scope);
    encoder_registry_.clearScope(scope);
    ownership_.clearForScope(scope);
    latch_.releaseForScope(scope);
}

FLASHMEM void InputBinding::clearBindings() {
    button_registry_.clear();
    encoder_registry_.clear();
    ownership_.reset();
    latch_.reset();
    gesture_.reset();
}

FLASHMEM void InputBinding::clearButtonBindings() {
    button_registry_.clear();
    ownership_.reset();
    latch_.reset();
    gesture_.reset();
}

FLASHMEM void InputBinding::clearEncoderBindings() {
    encoder_registry_.clear();
}

FLASHMEM void InputBinding::clearButtonScope(oc::type::ScopeID scope) {
    button_registry_.clearScope(scope);
    ownership_.clearForScope(scope);
    latch_.releaseForScope(scope);
}

FLASHMEM void InputBinding::clearEncoderScope(oc::type::ScopeID scope) {
    encoder_registry_.clearScope(scope);
}

// ═══════════════════════════════════════════════════════════════════════════
// Public API - Latch
// ═══════════════════════════════════════════════════════════════════════════

bool InputBinding::isLatched(oc::type::ButtonID btn) const {
    return latch_.isLatched(btn);
}

void InputBinding::clearLatch(oc::type::ButtonID btn) {
    latch_.release(btn);
}

void InputBinding::clearLatchesForScope(oc::type::ScopeID scope) {
    latch_.releaseForScope(scope);
}

// ═══════════════════════════════════════════════════════════════════════════
// Public API - State
// ═══════════════════════════════════════════════════════════════════════════

bool InputBinding::isButtonPressed(oc::type::ButtonID id) const {
    return gesture_.isPressed(id);
}

void InputBinding::setPressOwner(oc::type::ButtonID id, oc::type::ScopeID scope) {
    if (id >= MAX_BUTTONS) return;
    ownership_.setOwner(id, scope);
}

FLASHMEM void InputBinding::setBindingsEnabled(bool enabled) {
    bindings_enabled_ = enabled;
}

FLASHMEM void InputBinding::setAuthorityResolver(const AuthorityResolver* resolver) {
    authority_resolver_ = resolver;
    // Invalidate any outstanding tokens
    authority_token_++;
    if (authority_token_ == 0) authority_token_ = 1;
}

FLASHMEM InputBinding::AuthorityToken InputBinding::setAuthorityResolverScoped(const AuthorityResolver* resolver) {
    authority_resolver_ = resolver;
    authority_token_++;
    if (authority_token_ == 0) authority_token_ = 1;
    return authority_token_;
}

FLASHMEM void InputBinding::clearAuthorityResolver(AuthorityToken token) {
    if (token != 0 && token == authority_token_) {
        authority_resolver_ = nullptr;
    }
}

void InputBinding::processTick() {
    if (time_provider_) {
        current_time_ = time_provider_();
    }
    for (size_t i = 0; i < MAX_BUTTONS; ++i) {
        if (gesture_.isPressed(static_cast<oc::type::ButtonID>(i))) {
            checkLongPress(static_cast<oc::type::ButtonID>(i), current_time_);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Public API - Registration
// ═══════════════════════════════════════════════════════════════════════════

FLASHMEM oc::type::BindingID InputBinding::registerButtonBinding(ButtonBinding binding) {
    return button_registry_.add(std::move(binding));
}

FLASHMEM oc::type::BindingID InputBinding::registerEncoderBinding(EncoderBinding binding) {
    return encoder_registry_.add(std::move(binding));
}

FLASHMEM bool InputBinding::removeById(oc::type::BindingID id) {
    if (id == 0) return false;
    if (button_registry_.removeById(id)) return true;
    return encoder_registry_.removeById(id);
}

// ═══════════════════════════════════════════════════════════════════════════
// oc::type::Event Handlers
// ═══════════════════════════════════════════════════════════════════════════

void InputBinding::onEncoderChanged(const oc::type::Event& event) {
    auto& evt = static_cast<const EncoderChangedEvent&>(event);
    if (INPUT_BINDING_TRACE_NAV && evt.encoderId == 400) {
        const oc::type::ScopeID authority =
            authority_resolver_ ? authority_resolver_->getAuthority() : 0;
        OC_LOG_DEBUG("[InputBinding] encoder event id={} value={} authority={}",
                     static_cast<unsigned>(evt.encoderId),
                     evt.normalizedValue,
                     static_cast<unsigned>(authority));
    }
    dispatchEncoderEvent(evt.encoderId, evt.normalizedValue);
}

void InputBinding::onButtonPress(const oc::type::Event& event) {
    auto& evt = static_cast<const ButtonPressEvent&>(event);
    oc::type::ButtonID id = evt.buttonId;
    if (id >= MAX_BUTTONS) return;

    gesture_.onButtonPress(id, current_time_);

    // Dispatch to scopes, excluding the one that owns the latch (if any)
    oc::type::ScopeID newOwner = dispatchPress(id, latch_.owner(id));

    // Allow press handlers to transfer ownership (e.g. when opening a stacked
    // overlay on press and routing the paired release to that new scope).
    const oc::type::ScopeID overriddenOwner = ownership_.owner(id);
    if (overriddenOwner == 0 || overriddenOwner == newOwner) {
        ownership_.setOwner(id, newOwner);
    }
}

void InputBinding::onButtonRelease(const oc::type::Event& event) {
    auto& evt = static_cast<const ButtonReleaseEvent&>(event);
    oc::type::ButtonID id = evt.buttonId;
    if (id >= MAX_BUTTONS) return;

    const uint32_t now = current_time_;
    const uint32_t pressDuration = now - gesture_.pressTime(id);
    const oc::type::ScopeID latchOwner = latch_.owner(id);
    const oc::type::ScopeID pressOwner = ownership_.owner(id);

    // Combo detection (only if not latched)
    if (latchOwner == 0) {
        checkCombo(id);
    }

    gesture_.onButtonRelease(id, now);

    // Handle release based on ownership state
    if (pressOwner != 0) {
        handleScopedRelease(id, pressOwner, pressDuration);
    } else if (latchOwner != 0) {
        handleLatchedRelease(id, latchOwner);
    } else {
        dispatchButtonEvent(id, ButtonBindingType::RELEASE);
    }

    ownership_.clear(id);
    checkDoubleTap(id, now);
}

// ═══════════════════════════════════════════════════════════════════════════
// Release Handling (decomposed)
// ═══════════════════════════════════════════════════════════════════════════

void InputBinding::handleScopedRelease(oc::type::ButtonID id, oc::type::ScopeID pressOwner, uint32_t pressDuration) {
    if (shouldActivateLatch(id, pressOwner, pressDuration)) {
        latch_.activate(id, pressOwner);
    } else {
        if (!dispatchReleaseToScope(id, pressOwner, false) &&
            config_.releaseRoutingPolicy == ReleaseRoutingPolicy::OwnerThenFallback) {
            dispatchButtonEvent(id, ButtonBindingType::RELEASE);
        }
    }
}

void InputBinding::handleLatchedRelease(oc::type::ButtonID id, oc::type::ScopeID latchOwner) {
    if (!dispatchReleaseToScope(id, latchOwner, false) &&
        config_.releaseRoutingPolicy == ReleaseRoutingPolicy::OwnerThenFallback) {
        dispatchButtonEvent(id, ButtonBindingType::RELEASE);
    }
    latch_.release(id);
}

bool InputBinding::shouldActivateLatch(oc::type::ButtonID id, oc::type::ScopeID pressOwner, uint32_t pressDuration) const {
    if (pressDuration >= config_.latchThresholdMs) return false;

    for (const auto& binding : button_registry_.bindings()) {
        if (binding.buttonId == id &&
            binding.type == ButtonBindingType::PRESS &&
            binding.scopeId == pressOwner &&
            binding.latch &&
            binding.enabled) {
            return true;
        }
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// oc::type::Event Dispatch
// ═══════════════════════════════════════════════════════════════════════════

void InputBinding::dispatchButtonEvent(oc::type::ButtonID id, ButtonBindingType type) {
    if (!bindings_enabled_) return;

    // Try scoped bindings first (stop after first match)
    for (auto& binding : button_registry_.bindings()) {
        if (!binding.enabled || binding.buttonId != id || binding.type != type) continue;
        if (binding.scopeId == 0) continue;
        if (!isBindingActive(binding) || !hasAuthority(binding.scopeId)) continue;

        if (binding.action) {
            binding.action();
            return;
        }
    }

    // Fall back to global bindings (trigger all matches)
    for (auto& binding : button_registry_.bindings()) {
        if (!binding.enabled || binding.buttonId != id || binding.type != type) continue;
        if (binding.scopeId != 0) continue;
        if (!isBindingActive(binding)) continue;

        if (binding.action) {
            binding.action();
        }
    }
}

void InputBinding::dispatchEncoderEvent(oc::type::EncoderID id, float value) {
    if (!bindings_enabled_) return;

    const bool traceNav = INPUT_BINDING_TRACE_NAV && (id == 400);
    if (traceNav) {
        const oc::type::ScopeID authority =
            authority_resolver_ ? authority_resolver_->getAuthority() : 0;
        OC_LOG_DEBUG("[InputBinding] dispatch encoder id={} value={} authority={} scopedBindings={}",
                     static_cast<unsigned>(id),
                     value,
                     static_cast<unsigned>(authority),
                     static_cast<unsigned>(encoder_registry_.size()));
    }

    // Try scoped bindings first
    for (auto& binding : encoder_registry_.bindings()) {
        if (!binding.enabled || binding.encoderId != id) continue;
        if (binding.scopeId == 0) continue;
        const bool active = isBindingActive(binding);
        const bool authority = hasAuthority(binding.scopeId);
        const bool requiredButton = checkRequiredButton(binding);
        if (traceNav) {
            OC_LOG_DEBUG("[InputBinding] scoped candidate binding={} scope={} active={} authority={} requiredButton={}",
                         static_cast<unsigned>(binding.id),
                         static_cast<unsigned>(binding.scopeId),
                         active ? 1U : 0U,
                         authority ? 1U : 0U,
                         requiredButton ? 1U : 0U);
        }
        if (!active || !authority) continue;
        if (!requiredButton) continue;

        if (binding.action) {
            if (traceNav) {
                OC_LOG_DEBUG("[InputBinding] dispatch scoped binding={} scope={}",
                             static_cast<unsigned>(binding.id),
                             static_cast<unsigned>(binding.scopeId));
            }
            binding.action(value);
            return;
        }
    }

    // Fall back to global bindings
    for (auto& binding : encoder_registry_.bindings()) {
        if (!binding.enabled || binding.encoderId != id) continue;
        if (binding.scopeId != 0) continue;
        const bool active = isBindingActive(binding);
        const bool requiredButton = checkRequiredButton(binding);
        if (traceNav) {
            OC_LOG_DEBUG("[InputBinding] global candidate binding={} active={} requiredButton={}",
                         static_cast<unsigned>(binding.id),
                         active ? 1U : 0U,
                         requiredButton ? 1U : 0U);
        }
        if (!active) continue;
        if (!requiredButton) continue;

        if (binding.action) {
            if (traceNav) {
                OC_LOG_DEBUG("[InputBinding] dispatch global binding={}",
                             static_cast<unsigned>(binding.id));
            }
            binding.action(value);
        }
    }

    if (traceNav) {
        OC_LOG_DEBUG("[InputBinding] no encoder binding dispatched for id={}",
                     static_cast<unsigned>(id));
    }
}

oc::type::ScopeID InputBinding::dispatchPress(oc::type::ButtonID id, oc::type::ScopeID excludeScope) {
    if (!bindings_enabled_) return 0;

    // Try scoped bindings first
    for (auto& binding : button_registry_.bindings()) {
        if (!binding.enabled || binding.buttonId != id) continue;
        if (binding.type != ButtonBindingType::PRESS) continue;
        if (binding.scopeId == 0) continue;
        if (!isBindingActive(binding) || !hasAuthority(binding.scopeId)) continue;
        if (excludeScope != 0 && binding.scopeId == excludeScope) continue;

        if (binding.action) {
            binding.action();
            return binding.scopeId;
        }
    }

    // Fall back to global bindings (no ownership returned)
    for (auto& binding : button_registry_.bindings()) {
        if (!binding.enabled || binding.buttonId != id) continue;
        if (binding.type != ButtonBindingType::PRESS) continue;
        if (binding.scopeId != 0) continue;
        if (!isBindingActive(binding)) continue;

        if (binding.action) {
            binding.action();
        }
    }

    return 0;
}

bool InputBinding::dispatchReleaseToScope(oc::type::ButtonID id,
                                          oc::type::ScopeID scope,
                                          bool enforceAuthority) {
    for (auto& binding : button_registry_.bindings()) {
        if (!binding.enabled || binding.buttonId != id) continue;
        if (binding.type != ButtonBindingType::RELEASE) continue;
        if (binding.scopeId != scope) continue;
        if (!isBindingActive(binding)) continue;
        if (enforceAuthority && !hasAuthority(binding.scopeId)) continue;

        if (binding.action) {
            binding.action();
            return true;
        }
    }
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════
// Gesture Checks
// ═══════════════════════════════════════════════════════════════════════════

void InputBinding::checkLongPress(oc::type::ButtonID id, uint32_t now) {
    if (id >= MAX_BUTTONS) return;
    if (!gesture_.isPressed(id) || gesture_.longPressTriggered(id)) return;

    // Try scoped bindings first
    for (auto& binding : button_registry_.bindings()) {
        if (!binding.enabled || binding.type != ButtonBindingType::LONG_PRESS) continue;
        if (binding.buttonId != id || binding.scopeId == 0) continue;
        if (!isBindingActive(binding)) continue;
        if (!hasAuthority(binding.scopeId)) continue;

        if (gesture_.checkLongPress(id, now, binding.longPressMs)) {
            gesture_.markLongPressTriggered(id);
            if (binding.action) {
                binding.action();
                return;
            }
        }
    }

    // Fall back to global bindings
    for (auto& binding : button_registry_.bindings()) {
        if (!binding.enabled || binding.type != ButtonBindingType::LONG_PRESS) continue;
        if (binding.buttonId != id || binding.scopeId != 0) continue;

        if (gesture_.checkLongPress(id, now, binding.longPressMs)) {
            gesture_.markLongPressTriggered(id);
            if (binding.action) {
                binding.action();
            }
        }
    }
}

void InputBinding::checkDoubleTap(oc::type::ButtonID id, uint32_t now) {
    if (id >= MAX_BUTTONS) return;
    if (gesture_.tapCount(id) < 2) return;

    bool triggered = false;

    // Try scoped bindings first
    for (auto& binding : button_registry_.bindings()) {
        if (!binding.enabled || binding.type != ButtonBindingType::DOUBLE_TAP) continue;
        if (binding.buttonId != id || binding.scopeId == 0) continue;
        if (!isBindingActive(binding)) continue;
        if (!hasAuthority(binding.scopeId)) continue;

        if (gesture_.checkDoubleTap(id, now, binding.doubleTapWindowMs)) {
            if (binding.action) {
                binding.action();
                triggered = true;
                break;  // Stop after first scoped match
            }
        }
    }

    // Fall back to global bindings if no scoped triggered
    if (!triggered) {
        for (auto& binding : button_registry_.bindings()) {
            if (!binding.enabled || binding.type != ButtonBindingType::DOUBLE_TAP) continue;
            if (binding.buttonId != id || binding.scopeId != 0) continue;

            if (gesture_.checkDoubleTap(id, now, binding.doubleTapWindowMs)) {
                if (binding.action) {
                    binding.action();
                    triggered = true;
                }
            }
        }
    }

    if (triggered) {
        gesture_.resetTapCount(id);
    }
}

void InputBinding::checkCombo(oc::type::ButtonID releasedId) {
    // Try scoped bindings first
    for (auto& binding : button_registry_.bindings()) {
        if (!binding.enabled || binding.type != ButtonBindingType::COMBO) continue;
        if (binding.scopeId == 0 || !isBindingActive(binding)) continue;
        if (!hasAuthority(binding.scopeId)) continue;
        if (!binding.secondaryButton.has_value()) continue;

        bool isPartOfCombo = (binding.buttonId == releasedId) ||
                             (*binding.secondaryButton == releasedId);
        if (!isPartOfCombo) continue;

        if (gesture_.isComboActive(binding.buttonId, *binding.secondaryButton)) {
            if (binding.action) {
                binding.action();
                return;
            }
        }
    }

    // Fall back to global bindings
    for (auto& binding : button_registry_.bindings()) {
        if (!binding.enabled || binding.type != ButtonBindingType::COMBO) continue;
        if (binding.scopeId != 0) continue;
        if (!binding.secondaryButton.has_value()) continue;

        bool isPartOfCombo = (binding.buttonId == releasedId) ||
                             (*binding.secondaryButton == releasedId);
        if (!isPartOfCombo) continue;

        if (gesture_.isComboActive(binding.buttonId, *binding.secondaryButton)) {
            if (binding.action) {
                binding.action();
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Binding Helpers
// ═══════════════════════════════════════════════════════════════════════════

template <typename BindingType>
bool InputBinding::isBindingActive(const BindingType& binding) const {
    if (!binding.isActive) return true;
    return binding.isActive();
}

template bool InputBinding::isBindingActive(const ButtonBinding&) const;
template bool InputBinding::isBindingActive(const EncoderBinding&) const;

bool InputBinding::hasAuthority(oc::type::ScopeID scope) const {
    if (!authority_resolver_) return true;

    oc::type::ScopeID authority = authority_resolver_->getAuthority();
    if (authority == 0) return true;

    return scope == authority;
}

bool InputBinding::checkRequiredButton(const EncoderBinding& binding) const {
    if (binding.type != EncoderBindingType::TURN_WHILE_PRESSED) return true;
    if (!binding.requiredButton.has_value()) return true;

    oc::type::ButtonID btn = *binding.requiredButton;
    return gesture_.isPressed(btn) || latch_.isLatched(btn);
}

}  // namespace oc::core::input
