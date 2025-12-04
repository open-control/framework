#pragma once

namespace oc {
class ControlAPI;  // Forward declaration (defined in Phase 4)
}

namespace oc::context {

/**
 * @brief Interface for application contexts
 *
 * A context represents a mode of operation or integration (e.g., standalone,
 * DAW integration). Contexts receive lifecycle callbacks and access hardware
 * through the ControlAPI facade.
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
 *     const char* getId() const override { return "my-context"; }
 * };
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
    virtual bool initialize(ControlAPI& api) = 0;

    /// Called every frame while context is active
    virtual void update() = 0;

    /// Called when context is being destroyed
    virtual void cleanup() = 0;

    // ═══════════════════════════════════════════════════
    // Identity
    // ═══════════════════════════════════════════════════

    /// Human-readable display name
    virtual const char* getName() const = 0;

    /// Unique identifier for switching
    virtual const char* getId() const = 0;

    // ═══════════════════════════════════════════════════
    // Connection state (for DAW integrations)
    // ═══════════════════════════════════════════════════

    /// Check if external connection is active (default: always true)
    virtual bool isConnected() const { return true; }

    /// Called when connection is established
    virtual void onConnected() {}

    /// Called when connection is lost
    virtual void onDisconnected() {}
};

}  // namespace oc::context
