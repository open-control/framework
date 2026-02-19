#pragma once

#include <array>
#include <cstdint>

#include <oc/Config.hpp>
#include <oc/interface/IEventBus.hpp>
#include <oc/core/input/AuthorityResolver.hpp>
#include <oc/core/input/BindingRegistry.hpp>
#include <oc/core/input/GestureDetector.hpp>
#include <oc/core/input/InputConfig.hpp>
#include <oc/core/input/LatchManager.hpp>
#include <oc/core/input/OwnershipTracker.hpp>
#include <oc/core/input/Binding.hpp>
#include <oc/type/Ids.hpp>
#include <oc/type/Callbacks.hpp>
#include <oc/type/Event.hpp>

// For tests: if no oc::type::TimeProvider given, use a default that returns 0
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
 * This class is used internally by ButtonAPI / EncoderAPI. Prefer the fluent API instead:
 * @code
 * onButton(BTN_1).press().then([]{ doAction(); });
 * onEncoder(ENC_1).turn().then([](float v){ setParam(v); });
 * onButton(BTN_A).combo(BTN_B).then([]{ reset(); });
 * @endcode
 */
class InputBinding {
public:
    using AuthorityToken = uint32_t;
    /**
     * @brief Construct InputBinding with optional oc::type::TimeProvider
     * @param eventBus oc::type::Event bus for input events
     * @param timeProvider Function returning current time in ms (for tests: can be nullptr)
     * @param config Gesture timing configuration
     */
    explicit InputBinding(interface::IEventBus& eventBus,
                          oc::type::TimeProvider timeProvider = OC_DEFAULT_TIME_PROVIDER,
                          const InputConfig& config = {});
    ~InputBinding();

    InputBinding(const InputBinding&) = delete;
    InputBinding& operator=(const InputBinding&) = delete;

    // ═══════════════════════════════════════════════════
    // Scope and State Management
    // ═══════════════════════════════════════════════════

    /// Remove all bindings associated with a scope
    void clearScope(oc::type::ScopeID scope);

    /// Check if a button is in latched state (by any scope)
    bool isLatched(oc::type::ButtonID btn) const;

    /// Clear latch for a button (regardless of which scope owns it)
    void clearLatch(oc::type::ButtonID btn);

    /// Clear all latches owned by a specific scope
    void clearLatchesForScope(oc::type::ScopeID scope);

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

    /**
     * @brief Set authority resolver and return a token for safe clearing
     *
     * The returned token can be used to clear the resolver only if it has
     * not been replaced since.
     */
    [[nodiscard]] AuthorityToken setAuthorityResolverScoped(const AuthorityResolver* resolver);

    /// Clear resolver only if token matches the current assignment
    void clearAuthorityResolver(AuthorityToken token);

    // ═══════════════════════════════════════════════════
    // Separate button/encoder operations (for ButtonAPI/EncoderAPI)
    // ═══════════════════════════════════════════════════

    /// Check if a button is currently pressed (instantaneous state)
    bool isButtonPressed(oc::type::ButtonID id) const;

    /**
     * @brief Override current press ownership for a button
     *
     * Advanced API for flows that change authority/scope on press and need
     * the paired release to be routed to a different scope.
     */
    void setPressOwner(oc::type::ButtonID id, oc::type::ScopeID scope);

    /// Clear only button bindings
    void clearButtonBindings();

    /// Clear only encoder bindings
    void clearEncoderBindings();

    /// Clear button bindings in a specific scope
    void clearButtonScope(oc::type::ScopeID scope);

    /// Clear encoder bindings in a specific scope
    void clearEncoderScope(oc::type::ScopeID scope);

    // ═══════════════════════════════════════════════════
    // Internal API for fluent builders
    // ═══════════════════════════════════════════════════

    /**
     * @brief Register a button binding and return its ID
     * @param binding The binding to register (id field will be set)
     * @return The assigned oc::type::BindingID
     */
    oc::type::BindingID registerButtonBinding(ButtonBinding binding);

    /**
     * @brief Register an encoder binding and return its ID
     * @param binding The binding to register (id field will be set)
     * @return The assigned oc::type::BindingID
     */
    oc::type::BindingID registerEncoderBinding(EncoderBinding binding);

    /**
     * @brief Remove a binding by its ID
     * @param id The binding ID to remove
     * @return true if binding was found and removed
     */
    bool removeById(oc::type::BindingID id);

    /**
     * @brief Get access to config for builders
     */
    const InputConfig& config() const { return config_; }

private:
    BindingRegistry<ButtonBinding> button_registry_;
    BindingRegistry<EncoderBinding> encoder_registry_;

    void onEncoderChanged(const oc::type::Event& event);
    void onButtonPress(const oc::type::Event& event);
    void onButtonRelease(const oc::type::Event& event);

    // oc::type::Event dispatch
    void dispatchButtonEvent(oc::type::ButtonID id, ButtonBindingType type);
    void dispatchEncoderEvent(oc::type::EncoderID id, float value);
    oc::type::ScopeID dispatchPress(oc::type::ButtonID id, oc::type::ScopeID excludeScope = 0);
    bool dispatchReleaseToScope(oc::type::ButtonID id,
                                oc::type::ScopeID scope,
                                bool enforceAuthority);

    // Binding filters
    template <typename BindingType>
    bool isBindingActive(const BindingType& binding) const;
    bool hasAuthority(oc::type::ScopeID scope) const;
    bool checkRequiredButton(const EncoderBinding& binding) const;

    // Gesture triggers
    void checkLongPress(oc::type::ButtonID id, uint32_t now);
    void checkDoubleTap(oc::type::ButtonID id, uint32_t now);
    void checkCombo(oc::type::ButtonID releasedId);

    // Release handling (decomposed from onButtonRelease)
    void handleScopedRelease(oc::type::ButtonID id, oc::type::ScopeID pressOwner, uint32_t pressDuration);
    void handleLatchedRelease(oc::type::ButtonID id, oc::type::ScopeID latchOwner);
    bool shouldActivateLatch(oc::type::ButtonID id, oc::type::ScopeID pressOwner, uint32_t pressDuration) const;

    // Subsystems
    GestureDetector gesture_;
    LatchManager latch_;
    OwnershipTracker ownership_;

    interface::IEventBus& event_bus_;
    oc::type::TimeProvider time_provider_;
    interface::SubscriptionID encoder_sub_;
    interface::SubscriptionID button_press_sub_;
    interface::SubscriptionID button_release_sub_;

    InputConfig config_;
    bool bindings_enabled_ = true;
    uint32_t current_time_ = 0;
    oc::type::BindingID next_binding_id_ = 1;  ///< Next ID to assign (0 = invalid)
    const AuthorityResolver* authority_resolver_ = nullptr;  ///< Optional authority check
    AuthorityToken authority_token_ = 0;
};

}  // namespace oc::core::input
