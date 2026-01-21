#pragma once

/**
 * @file IContext.hpp
 * @brief Pure interface for application contexts (Level 1)
 *
 * This file defines the pure lifecycle interface.
 *
 * For the convenience fluent API (onButton(), onEncoder(), switchTo(), ...),
 * use `oc::context::ContextBase`.
 */

#include <oc/type/Result.hpp>

namespace oc::interface {

/**
 * @brief Pure interface for context lifecycle
 *
 * A context represents a distinct mode of operation in the application.
 * This interface defines only the lifecycle methods that ContextManager needs.
 *
 * ## For context implementations
 *
 * - If you need the fluent API (onButton, onEncoder, switchTo, etc.),
 *   inherit from `oc::context::ContextBase` instead.
 * - If you only need basic lifecycle, inherit directly from IContext.
 *
 * ## Lifecycle
 *
 * 1. **Construction**: Factory creates instance
 * 2. **init()**: Context sets up bindings and initial state
 * 3. **onConnected()**: Called after successful initialization
 * 4. **update()**: Called every frame while active
 * 5. **onDisconnected()**: Called before cleanup
 * 6. **cleanup()**: Context releases resources
 * 7. **Destruction**: Instance is destroyed
 *
 * @see oc::context::ContextBase for the implementation with fluent API
 */
class IContext {
public:
    virtual ~IContext() = default;

    // ─────────────────────────────────────────────────────────────────────
    // Lifecycle (must implement)
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Initialize the context
     * @return Result<void> - ok() if initialization succeeded, err() to trigger fallback
     */
    virtual oc::type::Result<void> init() = 0;

    /**
     * @brief Update the context (called every frame while active)
     */
    virtual void update() = 0;

    /**
     * @brief Clean up resources before destruction
     */
    virtual void cleanup() = 0;

    /**
     * @brief Get the human-readable name of this context
     * @return Static string identifying this context
     */
    virtual const char* getName() const = 0;

    // ─────────────────────────────────────────────────────────────────────
    // Connection State (optional override)
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Check if the context is still connected
     * @return true if connected (default), false if disconnected
     */
    virtual bool isConnected() const { return true; }

    /**
     * @brief Called when the context becomes active
     */
    virtual void onConnected() {}

    /**
     * @brief Called when the context is about to be deactivated
     */
    virtual void onDisconnected() {}
};

}  // namespace oc::interface
