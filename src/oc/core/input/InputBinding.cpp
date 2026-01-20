#include "InputBinding.hpp"

#include <oc/core/event/Events.hpp>
#include <oc/log/Log.hpp>

namespace oc::core::input {

using namespace event;

// ═══════════════════════════════════════════════════════════════════════════
// Construction / Destruction
// ═══════════════════════════════════════════════════════════════════════════

InputBinding::InputBinding(interface::IEventBus& eventBus, TimeProvider timeProvider, const InputConfig& config)
    : button_registry_(MAX_BUTTON_BINDINGS, next_binding_id_),
      encoder_registry_(MAX_ENCODER_BINDINGS, next_binding_id_),
      gesture_(config),
      event_bus_(eventBus),
      time_provider_(timeProvider),
      config_(config) {

    if (!time_provider_) {
        OC_LOG_WARN("{}", "[InputBinding] No TimeProvider - long press and double tap detection disabled");
    }

    encoder_sub_ = event_bus_.on(EventCategory::USER_INPUT, InputEvent::ENCODER_CHANGED,
                                 [this](const Event& e) { onEncoderChanged(e); });

    button_press_sub_ = event_bus_.on(EventCategory::USER_INPUT, InputEvent::BUTTON_PRESS,
                                      [this](const Event& e) { onButtonPress(e); });

    button_release_sub_ = event_bus_.on(EventCategory::USER_INPUT, InputEvent::BUTTON_RELEASE,
                                        [this](const Event& e) { onButtonRelease(e); });
}

InputBinding::~InputBinding() {
    event_bus_.off(encoder_sub_);
    event_bus_.off(button_press_sub_);
    event_bus_.off(button_release_sub_);
}

// ═══════════════════════════════════════════════════════════════════════════
// Public API - Scope Management
// ═══════════════════════════════════════════════════════════════════════════

void InputBinding::clearScope(ScopeID scope) {
    button_registry_.clearScope(scope);
    encoder_registry_.clearScope(scope);
    ownership_.clearForScope(scope);
    latch_.releaseForScope(scope);
}

void InputBinding::clearBindings() {
    button_registry_.clear();
    encoder_registry_.clear();
    ownership_.reset();
    latch_.reset();
    gesture_.reset();
}

void InputBinding::clearButtonBindings() {
    button_registry_.clear();
    ownership_.reset();
    latch_.reset();
    gesture_.reset();
}

void InputBinding::clearEncoderBindings() {
    encoder_registry_.clear();
}

void InputBinding::clearButtonScope(ScopeID scope) {
    button_registry_.clearScope(scope);
    ownership_.clearForScope(scope);
    latch_.releaseForScope(scope);
}

void InputBinding::clearEncoderScope(ScopeID scope) {
    encoder_registry_.clearScope(scope);
}

// ═══════════════════════════════════════════════════════════════════════════
// Public API - Latch
// ═══════════════════════════════════════════════════════════════════════════

bool InputBinding::isLatched(ButtonID btn) const {
    return latch_.isLatched(btn);
}

void InputBinding::clearLatch(ButtonID btn) {
    latch_.release(btn);
}

void InputBinding::clearLatchesForScope(ScopeID scope) {
    latch_.releaseForScope(scope);
}

// ═══════════════════════════════════════════════════════════════════════════
// Public API - State
// ═══════════════════════════════════════════════════════════════════════════

bool InputBinding::isButtonPressed(ButtonID id) const {
    return gesture_.isPressed(id);
}

void InputBinding::setBindingsEnabled(bool enabled) {
    bindings_enabled_ = enabled;
}

void InputBinding::setAuthorityResolver(const AuthorityResolver* resolver) {
    authority_resolver_ = resolver;
}

void InputBinding::processTick() {
    if (time_provider_) {
        current_time_ = time_provider_();
    }
    for (size_t i = 0; i < MAX_BUTTONS; ++i) {
        if (gesture_.isPressed(static_cast<ButtonID>(i))) {
            checkLongPress(static_cast<ButtonID>(i), current_time_);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Public API - Registration
// ═══════════════════════════════════════════════════════════════════════════

BindingID InputBinding::registerButtonBinding(ButtonBinding binding) {
    return button_registry_.add(std::move(binding));
}

BindingID InputBinding::registerEncoderBinding(EncoderBinding binding) {
    return encoder_registry_.add(std::move(binding));
}

bool InputBinding::removeById(BindingID id) {
    if (id == 0) return false;
    if (button_registry_.removeById(id)) return true;
    return encoder_registry_.removeById(id);
}

// ═══════════════════════════════════════════════════════════════════════════
// Event Handlers
// ═══════════════════════════════════════════════════════════════════════════

void InputBinding::onEncoderChanged(const Event& event) {
    auto& evt = static_cast<const EncoderChangedEvent&>(event);
    dispatchEncoderEvent(evt.encoderId, evt.normalizedValue);
}

void InputBinding::onButtonPress(const Event& event) {
    auto& evt = static_cast<const ButtonPressEvent&>(event);
    ButtonID id = evt.buttonId;
    if (id >= MAX_BUTTONS) return;

    gesture_.onButtonPress(id, current_time_);

    // Dispatch to scopes, excluding the one that owns the latch (if any)
    ScopeID newOwner = dispatchPress(id, latch_.owner(id));
    ownership_.setOwner(id, newOwner);
}

void InputBinding::onButtonRelease(const Event& event) {
    auto& evt = static_cast<const ButtonReleaseEvent&>(event);
    ButtonID id = evt.buttonId;
    if (id >= MAX_BUTTONS) return;

    const uint32_t now = current_time_;
    const uint32_t pressDuration = now - gesture_.pressTime(id);
    const ScopeID latchOwner = latch_.owner(id);
    const ScopeID pressOwner = ownership_.owner(id);

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

void InputBinding::handleScopedRelease(ButtonID id, ScopeID pressOwner, uint32_t pressDuration) {
    if (shouldActivateLatch(id, pressOwner, pressDuration)) {
        latch_.activate(id, pressOwner);
    } else {
        if (!dispatchReleaseToScope(id, pressOwner)) {
            dispatchButtonEvent(id, ButtonBindingType::RELEASE);
        }
    }
}

void InputBinding::handleLatchedRelease(ButtonID id, ScopeID latchOwner) {
    if (!dispatchReleaseToScope(id, latchOwner)) {
        dispatchButtonEvent(id, ButtonBindingType::RELEASE);
    }
    latch_.release(id);
}

bool InputBinding::shouldActivateLatch(ButtonID id, ScopeID pressOwner, uint32_t pressDuration) const {
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
// Event Dispatch
// ═══════════════════════════════════════════════════════════════════════════

void InputBinding::dispatchButtonEvent(ButtonID id, ButtonBindingType type) {
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

void InputBinding::dispatchEncoderEvent(EncoderID id, float value) {
    if (!bindings_enabled_) return;

    // Try scoped bindings first
    for (auto& binding : encoder_registry_.bindings()) {
        if (!binding.enabled || binding.encoderId != id) continue;
        if (binding.scopeId == 0) continue;
        if (!isBindingActive(binding) || !hasAuthority(binding.scopeId)) continue;
        if (!checkRequiredButton(binding)) continue;

        if (binding.action) {
            binding.action(value);
            return;
        }
    }

    // Fall back to global bindings
    for (auto& binding : encoder_registry_.bindings()) {
        if (!binding.enabled || binding.encoderId != id) continue;
        if (binding.scopeId != 0) continue;
        if (!isBindingActive(binding)) continue;
        if (!checkRequiredButton(binding)) continue;

        if (binding.action) {
            binding.action(value);
        }
    }
}

ScopeID InputBinding::dispatchPress(ButtonID id, ScopeID excludeScope) {
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

bool InputBinding::dispatchReleaseToScope(ButtonID id, ScopeID scope) {
    for (auto& binding : button_registry_.bindings()) {
        if (!binding.enabled || binding.buttonId != id) continue;
        if (binding.type != ButtonBindingType::RELEASE) continue;
        if (binding.scopeId != scope) continue;
        if (!isBindingActive(binding) || !hasAuthority(binding.scopeId)) continue;

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

void InputBinding::checkLongPress(ButtonID id, uint32_t now) {
    if (id >= MAX_BUTTONS) return;
    if (!gesture_.isPressed(id) || gesture_.longPressTriggered(id)) return;

    // Try scoped bindings first
    for (auto& binding : button_registry_.bindings()) {
        if (!binding.enabled || binding.type != ButtonBindingType::LONG_PRESS) continue;
        if (binding.buttonId != id || binding.scopeId == 0) continue;
        if (!isBindingActive(binding)) continue;

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

void InputBinding::checkDoubleTap(ButtonID id, uint32_t now) {
    if (id >= MAX_BUTTONS) return;
    if (gesture_.tapCount(id) < 2) return;

    bool triggered = false;

    // Try scoped bindings first
    for (auto& binding : button_registry_.bindings()) {
        if (!binding.enabled || binding.type != ButtonBindingType::DOUBLE_TAP) continue;
        if (binding.buttonId != id || binding.scopeId == 0) continue;
        if (!isBindingActive(binding)) continue;

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

void InputBinding::checkCombo(ButtonID releasedId) {
    // Try scoped bindings first
    for (auto& binding : button_registry_.bindings()) {
        if (!binding.enabled || binding.type != ButtonBindingType::COMBO) continue;
        if (binding.scopeId == 0 || !isBindingActive(binding)) continue;
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

bool InputBinding::hasAuthority(ScopeID scope) const {
    if (!authority_resolver_) return true;

    ScopeID authority = authority_resolver_->getAuthority();
    if (authority == 0) return true;

    return scope == authority;
}

bool InputBinding::checkRequiredButton(const EncoderBinding& binding) const {
    if (binding.type != EncoderBindingType::TURN_WHILE_PRESSED) return true;
    if (!binding.requiredButton.has_value()) return true;

    ButtonID btn = *binding.requiredButton;
    return gesture_.isPressed(btn) || latch_.isLatched(btn);
}

}  // namespace oc::core::input
