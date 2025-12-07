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
 * Subscribes to EventBus input events and manages button/encoder bindings.
 * Supports complex patterns: combos, long press, double tap, and scoped
 * bindings with priority.
 *
 * This class is used internally by ControlAPI. Use the fluent API instead:
 * @code
 * api.button(BTN_1).onPress().then([]{ doAction(); });
 * api.encoder(ENC_1).onTurn().then([](float v){ setParam(v); });
 * api.button(BTN_A).combo(BTN_B).then([]{ reset(); });
 * @endcode
 */
class InputBinding {
public:
    explicit InputBinding(event::IEventBus& eventBus, const InputConfig& config = {});
    ~InputBinding();

    InputBinding(const InputBinding&) = delete;
    InputBinding& operator=(const InputBinding&) = delete;

    // ═══════════════════════════════════════════════════
    // Scope and State Management
    // ═══════════════════════════════════════════════════

    /// Remove all bindings associated with a scope
    void clearScope(ScopeID scope);

    /// Check if a button is in latched state
    bool isLatched(hal::ButtonID btn) const;
    void setLatch(hal::ButtonID btn, bool latched);

    /// Must be called periodically for long press detection
    void processTick(uint32_t currentTimeMs);
    void clearBindings();
    void setBindingsEnabled(bool enabled);

    // ═══════════════════════════════════════════════════
    // Internal API for fluent builders
    // ═══════════════════════════════════════════════════

    /**
     * @brief Register a button binding and return its ID
     * @param binding The binding to register (id field will be set)
     * @return The assigned BindingID
     */
    BindingID registerButtonBinding(ButtonBinding binding);

    /**
     * @brief Register an encoder binding and return its ID
     * @param binding The binding to register (id field will be set)
     * @return The assigned BindingID
     */
    BindingID registerEncoderBinding(EncoderBinding binding);

    /**
     * @brief Remove a binding by its ID
     * @param id The binding ID to remove
     * @return true if binding was found and removed
     */
    bool removeById(BindingID id);

    /**
     * @brief Get access to config for builders
     */
    const InputConfig& config() const { return config_; }

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
    event::SubscriptionID encoder_sub_;
    event::SubscriptionID button_press_sub_;
    event::SubscriptionID button_release_sub_;

    InputConfig config_;
    bool bindings_enabled_ = true;
    uint32_t current_time_ = 0;
    BindingID next_binding_id_ = 1;  ///< Next ID to assign (0 = invalid)
};

}  // namespace oc::core::input
