#pragma once

namespace oc::api {
class ControlAPI;
}

namespace oc::context {

/**
 * @brief Interface for application contexts
 *
 * A context represents a mode of operation or integration (e.g., standalone,
 * DAW integration). Contexts receive lifecycle callbacks and access hardware
 * through the ControlAPI facade.
 *
 * Context identification is handled by ContextID (user-defined enum),
 * not by the context itself. This eliminates string allocations.
 *
 * @code
 * class MyContext : public IContext {
 * public:
 *     bool initialize(ControlAPI& api) override {
 *         api.onPressed(BTN_1, []() { doSomething(); });
 *         return true;
 *     }
 *     void update() override {}
 *     void cleanup() override {}
 *     const char* getName() const override { return "My Context"; }
 * };
 *
 * // Registration with ContextID:
 * app->registerContext<MyContext>(ContextID::MY_CONTEXT);
 * @endcode
 */
class IContext {
public:
    virtual ~IContext() = default;

    // ═══════════════════════════════════════════════════
    // Lifecycle
    // ═══════════════════════════════════════════════════

    /**
     * @brief Initialize the context with API access
     * @param api Reference to ControlAPI for hardware/input binding
     * @return true if initialization succeeded
     */
    virtual bool initialize(oc::api::ControlAPI& api) = 0;

    /// Called every frame while context is active
    virtual void update() = 0;

    /// Called when context is being destroyed
    virtual void cleanup() = 0;

    // ═══════════════════════════════════════════════════
    // Identity
    // ═══════════════════════════════════════════════════

    /// Human-readable display name (for logging/debug only)
    virtual const char* getName() const = 0;

    // ═══════════════════════════════════════════════════
    // Connection state (for DAW integrations)
    // ═══════════════════════════════════════════════════

    /// Check if external connection is active (default: always true)
    virtual bool isConnected() const { return true; }

    /// Called when connection is established
    virtual void onConnected() {}

    /// Called when connection is lost (before switchToDefault)
    virtual void onDisconnected() {}
};

}  // namespace oc::context
