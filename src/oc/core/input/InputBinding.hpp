#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <oc/Config.hpp>
#include <oc/interface/IEventBus.hpp>
#include <oc/core/input/AuthorityResolver.hpp>
#include <oc/core/input/BindingRegistry.hpp>
#include <oc/core/input/GestureDetector.hpp>
#include <oc/core/input/InputConfig.hpp>
#include <oc/core/input/LatchManager.hpp>
#include <oc/core/input/OwnershipTracker.hpp>
#include <oc/core/input/Binding.hpp>
#include <oc/types/Ids.hpp>
#include <oc/types/Callbacks.hpp>
#include <oc/types/Event.hpp>

// For tests: if no TimeProvider given, use a default that returns 0
#ifndef OC_DEFAULT_TIME_PROVIDER
#define OC_DEFAULT_TIME_PROVIDER nullptr
#endif

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
    /**
     * @brief Construct InputBinding with optional TimeProvider
     * @param eventBus Event bus for input events
     * @param timeProvider Function returning current time in ms (for tests: can be nullptr)
     * @param config Gesture timing configuration
     */
    explicit InputBinding(interface::IEventBus& eventBus,
                          TimeProvider timeProvider = OC_DEFAULT_TIME_PROVIDER,
                          const InputConfig& config = {});
    ~InputBinding();

    InputBinding(const InputBinding&) = delete;
    InputBinding& operator=(const InputBinding&) = delete;

    // ═══════════════════════════════════════════════════
    // Scope and State Management
    // ═══════════════════════════════════════════════════

    /// Remove all bindings associated with a scope
    void clearScope(ScopeID scope);

    /// Check if a button is in latched state (by any scope)
    bool isLatched(ButtonID btn) const;

    /// Clear latch for a button (regardless of which scope owns it)
    void clearLatch(ButtonID btn);

    /// Clear all latches owned by a specific scope
    void clearLatchesForScope(ScopeID scope);

    /// Must be called periodically for long press detection
    void processTick();
    void clearBindings();
    void setBindingsEnabled(bool enabled);

    /**
     * @brief Set the authority resolver for scope-based input filtering
     *
     * When set, scoped bindings will only trigger if their scope has authority
     * according to the resolver. This prevents multiple overlays from receiving
     * the same input events.
     *
     * @param resolver Pointer to the authority resolver (nullptr to disable)
     */
    void setAuthorityResolver(const AuthorityResolver* resolver);

    // ═══════════════════════════════════════════════════
    // Separate button/encoder operations (for ButtonAPI/EncoderAPI)
    // ═══════════════════════════════════════════════════

    /// Check if a button is currently pressed (instantaneous state)
    bool isButtonPressed(ButtonID id) const;

    /// Clear only button bindings
    void clearButtonBindings();

    /// Clear only encoder bindings
    void clearEncoderBindings();

    /// Clear button bindings in a specific scope
    void clearButtonScope(ScopeID scope);

    /// Clear encoder bindings in a specific scope
    void clearEncoderScope(ScopeID scope);

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
    BindingRegistry<ButtonBinding> button_registry_;
    BindingRegistry<EncoderBinding> encoder_registry_;

    void onEncoderChanged(const oc::Event& event);
    void onButtonPress(const oc::Event& event);
    void onButtonRelease(const oc::Event& event);

    // Event dispatch
    void dispatchButtonEvent(ButtonID id, ButtonBindingType type);
    void dispatchEncoderEvent(EncoderID id, float value);
    ScopeID dispatchPress(ButtonID id, ScopeID excludeScope = 0);
    bool dispatchReleaseToScope(ButtonID id, ScopeID scope);

    // Binding filters
    template <typename BindingType>
    bool isBindingActive(const BindingType& binding) const;
    bool hasAuthority(ScopeID scope) const;
    bool checkRequiredButton(const EncoderBinding& binding) const;

    // Gesture triggers
    void checkLongPress(ButtonID id, uint32_t now);
    void checkDoubleTap(ButtonID id, uint32_t now);
    void checkCombo(ButtonID releasedId);

    // Release handling (decomposed from onButtonRelease)
    void handleScopedRelease(ButtonID id, ScopeID pressOwner, uint32_t pressDuration);
    void handleLatchedRelease(ButtonID id, ScopeID latchOwner);
    bool shouldActivateLatch(ButtonID id, ScopeID pressOwner, uint32_t pressDuration) const;

    // Subsystems
    GestureDetector gesture_;
    LatchManager latch_;
    OwnershipTracker ownership_;

    interface::IEventBus& event_bus_;
    TimeProvider time_provider_;
    interface::SubscriptionID encoder_sub_;
    interface::SubscriptionID button_press_sub_;
    interface::SubscriptionID button_release_sub_;

    InputConfig config_;
    bool bindings_enabled_ = true;
    uint32_t current_time_ = 0;
    BindingID next_binding_id_ = 1;  ///< Next ID to assign (0 = invalid)
    const AuthorityResolver* authority_resolver_ = nullptr;  ///< Optional authority check
};

}  // namespace oc::core::input
