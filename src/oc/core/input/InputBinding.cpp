#include "InputBinding.hpp"

#include <algorithm>

#include <oc/core/event/Events.hpp>
#include <oc/core/Warning.hpp>
#include <oc/log/Log.hpp>

namespace oc::core::input {

using namespace event;
using oc::MAX_BUTTON_BINDINGS;
using oc::MAX_ENCODER_BINDINGS;

InputBinding::InputBinding(interface::IEventBus& eventBus, TimeProvider timeProvider, const InputConfig& config)
    : event_bus_(eventBus), time_provider_(timeProvider), config_(config) {
    // Pre-allocate to avoid runtime heap growth
    button_bindings_.reserve(MAX_BUTTON_BINDINGS);
    encoder_bindings_.reserve(MAX_ENCODER_BINDINGS);

    if (!time_provider_) {
        warn("[InputBinding] No TimeProvider - long press and double tap detection disabled");
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

// ═══════════════════════════════════════════════════
// Scope and State Management
// ═══════════════════════════════════════════════════

void InputBinding::clearScope(ScopeID scope) {
    auto buttonIt = button_bindings_.begin();
    while (buttonIt != button_bindings_.end()) {
        if (buttonIt->scopeId == scope) {
            buttonIt = button_bindings_.erase(buttonIt);
        } else {
            ++buttonIt;
        }
    }

    auto encoderIt = encoder_bindings_.begin();
    while (encoderIt != encoder_bindings_.end()) {
        if (encoderIt->scopeId == scope) {
            encoderIt = encoder_bindings_.erase(encoderIt);
        } else {
            ++encoderIt;
        }
    }

    // Clear ownership and latches for buttons owned by this scope
    for (size_t i = 0; i < MAX_BUTTONS; ++i) {
        if (button_press_owner_[i] == scope) {
            button_press_owner_[i] = 0;
        }
        if (latch_owner_[i] == scope) {
            latch_owner_[i] = 0;
        }
    }
}

bool InputBinding::isLatched(ButtonID btn) const {
    if (btn >= MAX_BUTTONS) return false;
    return latch_owner_[btn] != 0;
}

void InputBinding::clearLatch(ButtonID btn) {
    if (btn < MAX_BUTTONS) {
        latch_owner_[btn] = 0;
    }
}

void InputBinding::clearLatchesForScope(ScopeID scope) {
    for (size_t i = 0; i < MAX_BUTTONS; ++i) {
        if (latch_owner_[i] == scope) {
            latch_owner_[i] = 0;
        }
    }
}

void InputBinding::processTick() {
    if (time_provider_) {
        current_time_ = time_provider_();
    }
    for (size_t i = 0; i < MAX_BUTTONS; ++i) {
        if (button_states_[i]) {
            checkAndTriggerLongPress(static_cast<ButtonID>(i), current_time_);
        }
    }
}

void InputBinding::clearBindings() {
    button_bindings_.clear();
    encoder_bindings_.clear();
    button_press_owner_.fill(0);
    latch_owner_.fill(0);
}

void InputBinding::setBindingsEnabled(bool enabled) { bindings_enabled_ = enabled; }

void InputBinding::setAuthorityResolver(const AuthorityResolver* resolver) {
    authority_resolver_ = resolver;
}

// ═══════════════════════════════════════════════════
// Separate button/encoder operations
// ═══════════════════════════════════════════════════

bool InputBinding::isButtonPressed(ButtonID id) const {
    if (id >= MAX_BUTTONS) return false;
    return button_states_[id];
}

void InputBinding::clearButtonBindings() {
    button_bindings_.clear();
    button_press_owner_.fill(0);
    latch_owner_.fill(0);
}

void InputBinding::clearEncoderBindings() {
    encoder_bindings_.clear();
}

void InputBinding::clearButtonScope(ScopeID scope) {
    auto it = button_bindings_.begin();
    while (it != button_bindings_.end()) {
        if (it->scopeId == scope) {
            it = button_bindings_.erase(it);
        } else {
            ++it;
        }
    }
    // Clear ownership and latches for buttons owned by this scope
    for (size_t i = 0; i < MAX_BUTTONS; ++i) {
        if (button_press_owner_[i] == scope) {
            button_press_owner_[i] = 0;
        }
        if (latch_owner_[i] == scope) {
            latch_owner_[i] = 0;
        }
    }
}

void InputBinding::clearEncoderScope(ScopeID scope) {
    auto it = encoder_bindings_.begin();
    while (it != encoder_bindings_.end()) {
        if (it->scopeId == scope) {
            it = encoder_bindings_.erase(it);
        } else {
            ++it;
        }
    }
}

// ═══════════════════════════════════════════════════
// Internal API for fluent builders
// ═══════════════════════════════════════════════════

BindingID InputBinding::registerButtonBinding(ButtonBinding binding) {
    // Check limit
    if (button_bindings_.size() >= MAX_BUTTON_BINDINGS) {
        OC_LOG_WARN("InputBinding: max button bindings ({}) reached", MAX_BUTTON_BINDINGS);
        return 0;
    }

    BindingID id = next_binding_id_++;
    if (id == 0) id = next_binding_id_++;  // Skip 0 (invalid ID) on overflow
    binding.id = id;

    // Insert sorted by priority (higher priority first)
    auto it = std::find_if(button_bindings_.begin(), button_bindings_.end(),
                           [&](const ButtonBinding& b) { return b.priority < binding.priority; });
    button_bindings_.insert(it, std::move(binding));
    return id;
}

BindingID InputBinding::registerEncoderBinding(EncoderBinding binding) {
    // Check limit
    if (encoder_bindings_.size() >= MAX_ENCODER_BINDINGS) {
        OC_LOG_WARN("InputBinding: max encoder bindings ({}) reached", MAX_ENCODER_BINDINGS);
        return 0;
    }

    BindingID id = next_binding_id_++;
    if (id == 0) id = next_binding_id_++;  // Skip 0 (invalid ID) on overflow
    binding.id = id;

    // Insert sorted by priority (higher priority first)
    auto it = std::find_if(encoder_bindings_.begin(), encoder_bindings_.end(),
                           [&](const EncoderBinding& b) { return b.priority < binding.priority; });
    encoder_bindings_.insert(it, std::move(binding));
    return id;
}

bool InputBinding::removeById(BindingID id) {
    if (id == 0) return false;

    // Check button bindings
    for (auto it = button_bindings_.begin(); it != button_bindings_.end(); ++it) {
        if (it->id == id) {
            button_bindings_.erase(it);
            return true;
        }
    }

    // Check encoder bindings
    for (auto it = encoder_bindings_.begin(); it != encoder_bindings_.end(); ++it) {
        if (it->id == id) {
            encoder_bindings_.erase(it);
            return true;
        }
    }

    return false;
}

// ═══════════════════════════════════════════════════
// Event Handlers
// ═══════════════════════════════════════════════════

void InputBinding::onEncoderChanged(const Event& event) {
    auto& evt = static_cast<const EncoderChangedEvent&>(event);
    triggerMatchingEncoderBindings(evt.encoderId, evt.normalizedValue);
}

void InputBinding::onButtonPress(const Event& event) {
    auto& evt = static_cast<const ButtonPressEvent&>(event);
    ButtonID buttonId = evt.buttonId;
    if (buttonId >= MAX_BUTTONS) return;

    const uint32_t now = current_time_;

    button_states_[buttonId] = true;
    button_press_time_[buttonId] = now;

    if (now - button_release_time_[buttonId] < config_.doubleTapWindowMs) {
        button_tap_count_[buttonId]++;
    } else {
        button_tap_count_[buttonId] = 1;
    }

    // Scope-aware latch: exclude the scope that owns the latch, allow other scopes
    ScopeID latchOwner = latch_owner_[buttonId];
    button_press_owner_[buttonId] = triggerPressExcludingScope(buttonId, latchOwner);
}

void InputBinding::onButtonRelease(const Event& event) {
    auto& evt = static_cast<const ButtonReleaseEvent&>(event);
    ButtonID buttonId = evt.buttonId;
    if (buttonId >= MAX_BUTTONS) return;

    const uint32_t now = current_time_;
    const uint32_t pressDuration = now - button_press_time_[buttonId];

    ScopeID latchOwner = latch_owner_[buttonId];
    ScopeID pressOwner = button_press_owner_[buttonId];

    // Combo detection only if not latched
    if (latchOwner == 0) {
        checkAndTriggerCombosOnRelease(buttonId);
    }

    button_states_[buttonId] = false;
    button_release_time_[buttonId] = now;
    long_press_triggered_[buttonId] = false;

    if (pressOwner != 0) {
        // A scope handled the press - check if it should latch
        bool shouldLatch = false;
        for (const auto& binding : button_bindings_) {
            if (binding.buttonId == buttonId && binding.type == ButtonBindingType::PRESS &&
                binding.latch && binding.enabled && binding.scopeId == pressOwner) {
                shouldLatch = true;
                break;
            }
        }

        if (shouldLatch && pressDuration < config_.latchThresholdMs) {
            // Short press with latch binding - activate latch for this scope
            latch_owner_[buttonId] = pressOwner;
        } else {
            // Long press or no latch - trigger release
            if (!triggerReleaseForOwner(buttonId, pressOwner)) {
                triggerMatchingButtonBindings(buttonId, ButtonBindingType::RELEASE);
            }
        }
    } else if (latchOwner != 0) {
        // No press was triggered (was blocked by latch) - this is a latch release
        if (!triggerReleaseForOwner(buttonId, latchOwner)) {
            // Fallback: try any active release binding (e.g., child overlay with when())
            triggerMatchingButtonBindings(buttonId, ButtonBindingType::RELEASE);
        }
        latch_owner_[buttonId] = 0;
    } else {
        // No press owner and no latch - global binding or nothing
        triggerMatchingButtonBindings(buttonId, ButtonBindingType::RELEASE);
    }

    button_press_owner_[buttonId] = 0;
    checkAndTriggerDoubleTap(buttonId, now);
}

// ═══════════════════════════════════════════════════
// Trigger Logic
// ═══════════════════════════════════════════════════

bool InputBinding::triggerScopedButtonBindings(ButtonID buttonId, ButtonBindingType type) {
    for (auto& binding : button_bindings_) {
        if (!binding.enabled || binding.buttonId != buttonId || binding.type != type) continue;
        if (binding.scopeId == 0) continue;
        if (!isBindingActive(binding)) continue;
        if (!hasAuthority(binding.scopeId)) continue;  // Skip if scope lacks authority

        if (binding.action) {
            binding.action();
            // Stop after first scoped binding - prevents race conditions when
            // the action changes visibility of other scoped elements
            return true;
        }
    }
    return false;
}

bool InputBinding::triggerGlobalButtonBindings(ButtonID buttonId, ButtonBindingType type) {
    bool anyTriggered = false;
    for (auto& binding : button_bindings_) {
        if (!binding.enabled || binding.buttonId != buttonId || binding.type != type) continue;
        if (binding.scopeId != 0) continue;
        if (!isBindingActive(binding)) continue;  // Respect when() predicate for global bindings

        if (binding.action) {
            binding.action();
            anyTriggered = true;
        }
    }
    return anyTriggered;
}

ScopeID InputBinding::triggerPressWithOwnership(ButtonID buttonId) {
    return triggerPressExcludingScope(buttonId, 0);
}

ScopeID InputBinding::triggerPressExcludingScope(ButtonID buttonId, ScopeID excludeScope) {
    if (!bindings_enabled_) return 0;

    // Try scoped bindings first, excluding the latched scope
    for (auto& binding : button_bindings_) {
        if (!binding.enabled || binding.buttonId != buttonId) continue;
        if (binding.type != ButtonBindingType::PRESS) continue;
        if (binding.scopeId == 0) continue;
        if (!isBindingActive(binding)) continue;
        if (!hasAuthority(binding.scopeId)) continue;  // Skip if scope lacks authority
        if (excludeScope != 0 && binding.scopeId == excludeScope) continue;  // Skip latched scope

        if (binding.action) {
            binding.action();
            return binding.scopeId;  // Return owner scope
        }
    }

    // Fall back to global bindings (no ownership)
    triggerGlobalButtonBindings(buttonId, ButtonBindingType::PRESS);
    return 0;
}

bool InputBinding::triggerReleaseForOwner(ButtonID buttonId, ScopeID owner) {
    // Dispatch release ONLY to bindings in the owner scope
    for (auto& binding : button_bindings_) {
        if (!binding.enabled || binding.buttonId != buttonId) continue;
        if (binding.type != ButtonBindingType::RELEASE) continue;
        if (binding.scopeId != owner) continue;
        if (!isBindingActive(binding)) continue;  // Respect when() for fallback support
        if (!hasAuthority(binding.scopeId)) continue;  // Skip if scope lacks authority

        if (binding.action) {
            binding.action();
            return true;
        }
    }
    return false;
}

void InputBinding::triggerMatchingButtonBindings(ButtonID buttonId, ButtonBindingType type) {
    if (!bindings_enabled_) return;
    if (triggerScopedButtonBindings(buttonId, type)) return;
    triggerGlobalButtonBindings(buttonId, type);
}

bool InputBinding::triggerScopedEncoderBindings(EncoderID encoderId, float encoderValue) {
    for (auto& binding : encoder_bindings_) {
        if (!binding.enabled || binding.encoderId != encoderId) continue;
        if (binding.scopeId == 0) continue;
        if (!isBindingActive(binding)) continue;
        if (!hasAuthority(binding.scopeId)) continue;  // Skip if scope lacks authority

        if (binding.type == EncoderBindingType::TURN_WHILE_PRESSED && binding.requiredButton.has_value()) {
            ButtonID btn = *binding.requiredButton;
            bool isPressed = btn < MAX_BUTTONS && button_states_[btn];
            bool isLatchedBtn = btn < MAX_BUTTONS && latch_owner_[btn] != 0;
            if (!isPressed && !isLatchedBtn) continue;
        }

        if (binding.action) {
            binding.action(encoderValue);
            // Stop after first scoped binding - prevents race conditions when
            // the action changes visibility of other scoped elements
            return true;
        }
    }
    return false;
}

bool InputBinding::triggerGlobalEncoderBindings(EncoderID encoderId, float encoderValue) {
    bool anyTriggered = false;
    for (auto& binding : encoder_bindings_) {
        if (!binding.enabled || binding.encoderId != encoderId) continue;
        if (binding.scopeId != 0) continue;
        if (!isBindingActive(binding)) continue;  // Respect when() predicate for global bindings

        if (binding.type == EncoderBindingType::TURN_WHILE_PRESSED && binding.requiredButton.has_value()) {
            ButtonID btn = *binding.requiredButton;
            bool isPressed = btn < MAX_BUTTONS && button_states_[btn];
            bool isLatchedBtn = btn < MAX_BUTTONS && latch_owner_[btn] != 0;
            if (!isPressed && !isLatchedBtn) continue;
        }

        if (binding.action) {
            binding.action(encoderValue);
            anyTriggered = true;
        }
    }
    return anyTriggered;
}

void InputBinding::triggerMatchingEncoderBindings(EncoderID encoderId, float encoderValue) {
    if (!bindings_enabled_) return;
    if (triggerScopedEncoderBindings(encoderId, encoderValue)) return;
    triggerGlobalEncoderBindings(encoderId, encoderValue);
}

// ═══════════════════════════════════════════════════
// Gesture Detection
// ═══════════════════════════════════════════════════

void InputBinding::checkAndTriggerLongPress(ButtonID buttonId, uint32_t now) {
    if (buttonId >= MAX_BUTTONS) return;
    if (!button_states_[buttonId] || long_press_triggered_[buttonId]) return;

    const uint32_t pressTime = button_press_time_[buttonId];
    bool scopedTriggered = false;

    for (auto& binding : button_bindings_) {
        if (!binding.enabled || binding.type != ButtonBindingType::LONG_PRESS) continue;
        if (binding.buttonId != buttonId || binding.scopeId == 0) continue;
        if (!isBindingActive(binding)) continue;

        const uint32_t duration = binding.longPressMs > 0 ? binding.longPressMs : config_.longPressMs;
        if ((now - pressTime) >= duration) {
            long_press_triggered_[buttonId] = true;
            if (binding.action) {
                binding.action();
                scopedTriggered = true;
            }
        }
    }

    if (scopedTriggered) return;

    for (auto& binding : button_bindings_) {
        if (!binding.enabled || binding.type != ButtonBindingType::LONG_PRESS) continue;
        if (binding.buttonId != buttonId || binding.scopeId != 0) continue;

        const uint32_t duration = binding.longPressMs > 0 ? binding.longPressMs : config_.longPressMs;
        if ((now - pressTime) >= duration) {
            long_press_triggered_[buttonId] = true;
            if (binding.action) binding.action();
        }
    }
}

void InputBinding::checkAndTriggerDoubleTap(ButtonID buttonId, uint32_t now) {
    if (buttonId >= MAX_BUTTONS) return;
    if (button_tap_count_[buttonId] < 2) return;

    const uint32_t releaseTime = button_release_time_[buttonId];

    bool anyTriggered = false;
    bool scopedTriggered = false;

    // Check scoped bindings first
    for (auto& binding : button_bindings_) {
        if (!binding.enabled || binding.type != ButtonBindingType::DOUBLE_TAP) continue;
        if (binding.buttonId != buttonId || binding.scopeId == 0) continue;
        if (!isBindingActive(binding)) continue;

        const uint32_t window = binding.doubleTapWindowMs > 0 ? binding.doubleTapWindowMs : config_.doubleTapWindowMs;
        if ((now - releaseTime) < window) {
            if (binding.action) {
                binding.action();
                scopedTriggered = true;
                anyTriggered = true;
            }
        }
    }

    if (!scopedTriggered) {
        // Check global bindings
        for (auto& binding : button_bindings_) {
            if (!binding.enabled || binding.type != ButtonBindingType::DOUBLE_TAP) continue;
            if (binding.buttonId != buttonId || binding.scopeId != 0) continue;

            const uint32_t window = binding.doubleTapWindowMs > 0 ? binding.doubleTapWindowMs : config_.doubleTapWindowMs;
            if ((now - releaseTime) < window) {
                if (binding.action) {
                    binding.action();
                    anyTriggered = true;
                }
            }
        }
    }

    if (anyTriggered) {
        button_tap_count_[buttonId] = 0;
    }
}

void InputBinding::checkAndTriggerCombosOnRelease(ButtonID releasedButtonID) {
    bool scopedTriggered = false;

    for (auto& binding : button_bindings_) {
        if (!binding.enabled || binding.type != ButtonBindingType::COMBO) continue;
        if (binding.scopeId == 0 || !isBindingActive(binding)) continue;

        bool isPartOfCombo = (binding.buttonId == releasedButtonID) ||
                             (binding.secondaryButton.has_value() && *binding.secondaryButton == releasedButtonID);
        if (!isPartOfCombo) continue;

        if (binding.secondaryButton.has_value() && isButtonComboActive(binding.buttonId, *binding.secondaryButton)) {
            if (binding.action) {
                binding.action();
                scopedTriggered = true;
            }
        }
    }

    if (scopedTriggered) return;

    for (auto& binding : button_bindings_) {
        if (!binding.enabled || binding.type != ButtonBindingType::COMBO) continue;
        if (binding.scopeId != 0) continue;

        bool isPartOfCombo = (binding.buttonId == releasedButtonID) ||
                             (binding.secondaryButton.has_value() && *binding.secondaryButton == releasedButtonID);
        if (!isPartOfCombo) continue;

        if (binding.secondaryButton.has_value() && isButtonComboActive(binding.buttonId, *binding.secondaryButton)) {
            if (binding.action) binding.action();
        }
    }
}

bool InputBinding::isButtonComboActive(ButtonID btn1, ButtonID btn2) const {
    auto isPressed = [this](ButtonID btn) {
        return btn < MAX_BUTTONS && button_states_[btn];
    };
    return isPressed(btn1) && isPressed(btn2);
}

bool InputBinding::isBindingActive(const ButtonBinding& binding) const {
    // when() predicate is respected for both scoped and global bindings
    if (!binding.isActive) return true;  // nullptr = always active
    return binding.isActive();
}

bool InputBinding::isBindingActive(const EncoderBinding& binding) const {
    // when() predicate is respected for both scoped and global bindings
    if (!binding.isActive) return true;  // nullptr = always active
    return binding.isActive();
}

bool InputBinding::hasAuthority(ScopeID scope) const {
    // If no resolver configured, all scopes have authority (backwards compatible)
    if (!authority_resolver_) return true;

    // Get current authority (top overlay scope, or 0 if none)
    ScopeID authority = authority_resolver_->getAuthority();

    // If no overlay has authority (authority == 0), all scopes can receive input
    // This allows view-level and global bindings to work when no overlay is active
    if (authority == 0) return true;

    // When an overlay is active, only bindings in that scope can receive input
    return scope == authority;
}

}  // namespace oc::core::input
