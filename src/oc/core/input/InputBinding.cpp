#include "InputBinding.hpp"

#include <oc/core/event/Events.hpp>

namespace oc::core::input {

using namespace event;

InputBinding::InputBinding(IEventBus& eventBus, const InputConfig& config)
    : event_bus_(eventBus), config_(config) {
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
}

bool InputBinding::isLatched(hal::ButtonID btn) const {
    auto it = latch_states_.find(btn);
    return it != latch_states_.end() && it->second;
}

void InputBinding::setLatch(hal::ButtonID btn, bool latched) { latch_states_[btn] = latched; }

void InputBinding::processTick(uint32_t currentTimeMs) {
    current_time_ = currentTimeMs;
    for (const auto& [buttonId, isPressed] : button_states_) {
        if (isPressed) checkAndTriggerLongPress(buttonId, current_time_);
    }
}

void InputBinding::clearBindings() {
    button_bindings_.clear();
    encoder_bindings_.clear();
}

void InputBinding::setBindingsEnabled(bool enabled) { bindings_enabled_ = enabled; }

// ═══════════════════════════════════════════════════
// Separate button/encoder operations
// ═══════════════════════════════════════════════════

bool InputBinding::isButtonPressed(hal::ButtonID id) const {
    auto it = button_states_.find(id);
    return it != button_states_.end() && it->second;
}

void InputBinding::clearButtonBindings() {
    button_bindings_.clear();
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
    BindingID id = next_binding_id_++;
    binding.id = id;
    button_bindings_.push_back(std::move(binding));
    return id;
}

BindingID InputBinding::registerEncoderBinding(EncoderBinding binding) {
    BindingID id = next_binding_id_++;
    binding.id = id;
    encoder_bindings_.push_back(std::move(binding));
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
    hal::ButtonID buttonId = evt.buttonId;
    const uint32_t now = current_time_;

    button_states_[buttonId] = true;
    button_press_time_[buttonId] = now;

    if (now - button_release_time_[buttonId] < config_.doubleTapWindowMs) {
        button_tap_count_[buttonId]++;
    } else {
        button_tap_count_[buttonId] = 1;
    }

    if (!latch_states_[buttonId]) {
        triggerMatchingButtonBindings(buttonId, ButtonBindingType::PRESS);
    }
}

void InputBinding::onButtonRelease(const Event& event) {
    auto& evt = static_cast<const ButtonReleaseEvent&>(event);
    hal::ButtonID buttonId = evt.buttonId;
    const uint32_t now = current_time_;

    const uint32_t pressDuration = now - button_press_time_[buttonId];
    const bool wasLatched = latch_states_[buttonId];

    if (wasLatched) {
        latch_states_[buttonId] = false;
    }

    if (!wasLatched) {
        checkAndTriggerCombosOnRelease(buttonId);
    }

    button_states_[buttonId] = false;
    button_release_time_[buttonId] = now;
    long_press_triggered_[buttonId] = false;

    bool hasLatchBinding = false;
    for (const auto& binding : button_bindings_) {
        if (binding.buttonId == buttonId && binding.type == ButtonBindingType::PRESS && binding.latch &&
            binding.enabled && isBindingActive(binding)) {
            hasLatchBinding = true;
            break;
        }
    }

    if (hasLatchBinding && !wasLatched && pressDuration < config_.latchThresholdMs) {
        latch_states_[buttonId] = true;
    } else {
        triggerMatchingButtonBindings(buttonId, ButtonBindingType::RELEASE);
    }
    checkAndTriggerDoubleTap(buttonId, now);
}

// ═══════════════════════════════════════════════════
// Trigger Logic
// ═══════════════════════════════════════════════════

bool InputBinding::triggerScopedButtonBindings(hal::ButtonID buttonId, ButtonBindingType type) {
    bool anyTriggered = false;
    for (auto& binding : button_bindings_) {
        if (!binding.enabled || binding.buttonId != buttonId || binding.type != type) continue;
        if (binding.scopeId == 0) continue;
        if (!isBindingActive(binding)) continue;

        if (binding.action) {
            binding.action();
            anyTriggered = true;
        }
    }
    return anyTriggered;
}

bool InputBinding::triggerGlobalButtonBindings(hal::ButtonID buttonId, ButtonBindingType type) {
    bool anyTriggered = false;
    for (auto& binding : button_bindings_) {
        if (!binding.enabled || binding.buttonId != buttonId || binding.type != type) continue;
        if (binding.scopeId != 0) continue;

        if (binding.action) {
            binding.action();
            anyTriggered = true;
        }
    }
    return anyTriggered;
}

void InputBinding::triggerMatchingButtonBindings(hal::ButtonID buttonId, ButtonBindingType type) {
    if (!bindings_enabled_) return;
    if (triggerScopedButtonBindings(buttonId, type)) return;
    triggerGlobalButtonBindings(buttonId, type);
}

bool InputBinding::triggerScopedEncoderBindings(hal::EncoderID encoderId, float encoderValue) {
    bool anyTriggered = false;
    for (auto& binding : encoder_bindings_) {
        if (!binding.enabled || binding.encoderId != encoderId) continue;
        if (binding.scopeId == 0) continue;
        if (!isBindingActive(binding)) continue;

        if (binding.type == EncoderBindingType::TURN_WHILE_PRESSED && binding.requiredButton.has_value()) {
            hal::ButtonID btn = *binding.requiredButton;
            bool isPressed = button_states_.count(btn) && button_states_.at(btn);
            bool isLatched = latch_states_.count(btn) && latch_states_.at(btn);
            if (!isPressed && !isLatched) continue;
        }

        if (binding.action) {
            binding.action(encoderValue);
            anyTriggered = true;
        }
    }
    return anyTriggered;
}

bool InputBinding::triggerGlobalEncoderBindings(hal::EncoderID encoderId, float encoderValue) {
    bool anyTriggered = false;
    for (auto& binding : encoder_bindings_) {
        if (!binding.enabled || binding.encoderId != encoderId) continue;
        if (binding.scopeId != 0) continue;

        if (binding.type == EncoderBindingType::TURN_WHILE_PRESSED && binding.requiredButton.has_value()) {
            hal::ButtonID btn = *binding.requiredButton;
            bool isPressed = button_states_.count(btn) && button_states_.at(btn);
            bool isLatched = latch_states_.count(btn) && latch_states_.at(btn);
            if (!isPressed && !isLatched) continue;
        }

        if (binding.action) {
            binding.action(encoderValue);
            anyTriggered = true;
        }
    }
    return anyTriggered;
}

void InputBinding::triggerMatchingEncoderBindings(hal::EncoderID encoderId, float encoderValue) {
    if (!bindings_enabled_) return;
    if (triggerScopedEncoderBindings(encoderId, encoderValue)) return;
    triggerGlobalEncoderBindings(encoderId, encoderValue);
}

// ═══════════════════════════════════════════════════
// Gesture Detection
// ═══════════════════════════════════════════════════

void InputBinding::checkAndTriggerLongPress(hal::ButtonID buttonId, uint32_t now) {
    if (!button_states_[buttonId] || long_press_triggered_[buttonId]) return;

    auto pressTimeIt = button_press_time_.find(buttonId);
    if (pressTimeIt == button_press_time_.end()) return;

    bool scopedTriggered = false;

    for (auto& binding : button_bindings_) {
        if (!binding.enabled || binding.type != ButtonBindingType::LONG_PRESS) continue;
        if (binding.buttonId != buttonId || binding.scopeId == 0) continue;
        if (!isBindingActive(binding)) continue;

        const uint32_t duration = binding.longPressMs > 0 ? binding.longPressMs : config_.longPressMs;
        if ((now - pressTimeIt->second) >= duration) {
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
        if ((now - pressTimeIt->second) >= duration) {
            long_press_triggered_[buttonId] = true;
            if (binding.action) binding.action();
        }
    }
}

void InputBinding::checkAndTriggerDoubleTap(hal::ButtonID buttonId, uint32_t now) {
    auto tapIt = button_tap_count_.find(buttonId);
    if (tapIt == button_tap_count_.end() || tapIt->second < 2) return;

    auto releaseIt = button_release_time_.find(buttonId);
    if (releaseIt == button_release_time_.end()) return;

    bool anyTriggered = false;
    bool scopedTriggered = false;

    // Check scoped bindings first
    for (auto& binding : button_bindings_) {
        if (!binding.enabled || binding.type != ButtonBindingType::DOUBLE_TAP) continue;
        if (binding.buttonId != buttonId || binding.scopeId == 0) continue;
        if (!isBindingActive(binding)) continue;

        const uint32_t window = binding.doubleTapWindowMs > 0 ? binding.doubleTapWindowMs : config_.doubleTapWindowMs;
        if ((now - releaseIt->second) < window) {
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
            if ((now - releaseIt->second) < window) {
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

void InputBinding::checkAndTriggerCombosOnRelease(hal::ButtonID releasedButtonID) {
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

bool InputBinding::isButtonComboActive(hal::ButtonID btn1, hal::ButtonID btn2) const {
    auto isPressed = [this](hal::ButtonID btn) { return button_states_.count(btn) && button_states_.at(btn); };
    return isPressed(btn1) && isPressed(btn2);
}

bool InputBinding::isBindingActive(const ButtonBinding& binding) const {
    if (binding.scopeId == 0) return true;
    if (!binding.isActive) return true;  // nullptr = always active
    return binding.isActive();
}

bool InputBinding::isBindingActive(const EncoderBinding& binding) const {
    if (binding.scopeId == 0) return true;
    if (!binding.isActive) return true;  // nullptr = always active
    return binding.isActive();
}

}  // namespace oc::core::input
