#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <oc/core/event/IEventBus.hpp>
#include <oc/core/input/InputConfig.hpp>
#include <oc/core/struct/Binding.hpp>
#include <oc/hal/Types.hpp>

namespace oc::core::input {

/**
 * @brief Centralized input binding and gesture recognition system
 *
 * Subscribes to EventBus input events and provides a simple API for binding
 * actions to hardware controls. Supports complex patterns: combos, long press,
 * double tap, and scoped bindings with priority.
 *
 * @code
 * binding.onPressed(ButtonID::LEFT, []() { doAction(); });
 * binding.onTurned(EncoderID::MAIN, [](float v) { setParam(v); });
 * binding.onCombo(ButtonID::A, ButtonID::B, []() { reset(); });
 * @endcode
 */
class InputBinding {
public:
    explicit InputBinding(event::IEventBus& eventBus, const InputConfig& config = {});
    ~InputBinding();

    InputBinding(const InputBinding&) = delete;
    InputBinding& operator=(const InputBinding&) = delete;

    // ═══════════════════════════════════════════════════
    // Global bindings (always active)
    // ═══════════════════════════════════════════════════

    void onPressed(hal::ButtonID id, ActionCallback cb);
    void onReleased(hal::ButtonID id, ActionCallback cb);
    void onLongPress(hal::ButtonID id, ActionCallback cb, uint32_t ms = 0);
    void onDoubleTap(hal::ButtonID id, ActionCallback cb);
    void onCombo(hal::ButtonID btn1, hal::ButtonID btn2, ActionCallback cb);
    void onTurned(hal::EncoderID id, EncoderActionCallback cb);
    void onTurnedWhilePressed(hal::EncoderID enc, hal::ButtonID btn, EncoderActionCallback cb);

    // ═══════════════════════════════════════════════════
    // Scoped bindings (active when scope visible)
    // ═══════════════════════════════════════════════════

    void onPressed(hal::ButtonID id, ActionCallback cb, VisibilityPredicate isVisible, ScopeId scope,
                   bool latch = false);
    void onReleased(hal::ButtonID id, ActionCallback cb, VisibilityPredicate isVisible, ScopeId scope);
    void onLongPress(hal::ButtonID id, ActionCallback cb, uint32_t ms, VisibilityPredicate isVisible,
                     ScopeId scope);
    void onDoubleTap(hal::ButtonID id, ActionCallback cb, VisibilityPredicate isVisible, ScopeId scope);
    void onCombo(hal::ButtonID btn1, hal::ButtonID btn2, ActionCallback cb,
                 VisibilityPredicate isVisible, ScopeId scope);
    void onTurned(hal::EncoderID id, EncoderActionCallback cb, VisibilityPredicate isVisible,
                  ScopeId scope);
    void onTurnedWhilePressed(hal::EncoderID enc, hal::ButtonID btn, EncoderActionCallback cb,
                              VisibilityPredicate isVisible, ScopeId scope);

    /// Remove all bindings associated with a scope
    void clearScope(ScopeId scope);

    /// Check if a button is in latched state
    bool isLatched(hal::ButtonID btn) const;
    void setLatch(hal::ButtonID btn, bool latched);

    /// Must be called periodically for long press detection
    void processTick(uint32_t currentTimeMs);
    void clearBindings();
    void setBindingsEnabled(bool enabled);

private:
    std::vector<ButtonBinding> button_bindings_;
    std::vector<EncoderBinding> encoder_bindings_;

    void onEncoderChanged(const event::Event& event);
    void onButtonPress(const event::Event& event);
    void onButtonRelease(const event::Event& event);

    void triggerMatchingButtonBindings(hal::ButtonID buttonId, ButtonBindingType type);
    void triggerMatchingEncoderBindings(hal::EncoderID encoderId, float encoderValue);

    bool triggerScopedButtonBindings(hal::ButtonID buttonId, ButtonBindingType type);
    bool triggerGlobalButtonBindings(hal::ButtonID buttonId, ButtonBindingType type);
    bool triggerScopedEncoderBindings(hal::EncoderID encoderId, float encoderValue);
    bool triggerGlobalEncoderBindings(hal::EncoderID encoderId, float encoderValue);

    bool isBindingActive(const ButtonBinding& binding) const;
    bool isBindingActive(const EncoderBinding& binding) const;

    void checkAndTriggerLongPress(hal::ButtonID buttonId, uint32_t now);
    void checkAndTriggerDoubleTap(hal::ButtonID buttonId, uint32_t now);
    void checkAndTriggerCombosOnRelease(hal::ButtonID releasedButtonID);
    bool isButtonComboActive(hal::ButtonID btn1, hal::ButtonID btn2) const;

    std::unordered_map<hal::ButtonID, bool> button_states_;
    std::unordered_map<hal::ButtonID, uint32_t> button_press_time_;
    std::unordered_map<hal::ButtonID, uint32_t> button_release_time_;
    std::unordered_map<hal::ButtonID, uint8_t> button_tap_count_;
    std::unordered_map<hal::ButtonID, bool> long_press_triggered_;
    std::unordered_map<hal::ButtonID, bool> latch_states_;

    event::IEventBus& event_bus_;
    event::SubscriptionId encoder_sub_;
    event::SubscriptionId button_press_sub_;
    event::SubscriptionId button_release_sub_;

    InputConfig config_;
    bool bindings_enabled_ = true;
    uint32_t current_time_ = 0;
};

}  // namespace oc::core::input
