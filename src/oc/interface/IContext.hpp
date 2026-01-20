#pragma once

/**
 * @file IContext.hpp
 * @brief Pure interface for application contexts (Level 1)
 *
 * This file defines the pure interface. For the convenience API with
 * onButton(), onEncoder(), etc., use ContextBase from context/ContextBase.hpp.
 */

#include <oc/types/Result.hpp>

// Forward declaration to avoid circular dependency
namespace oc::context { struct APIs; }

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
 * 2. **setAPIs()**: Framework injects API references (on ContextBase)
 * 3. **init()**: Context sets up bindings and initial state
 * 4. **onConnected()**: Called after successful initialization
 * 5. **update()**: Called every frame while active
 * 6. **onDisconnected()**: Called before cleanup
 * 7. **cleanup()**: Context releases resources
 * 8. **Destruction**: Instance is destroyed
 *
 * @see oc::context::ContextBase for the implementation with fluent API
 */
class IContext {
public:
    virtual ~IContext() = default;

    // ─────────────────────────────────────────────────────────────────────
    // API Injection (called by ContextManager)
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Inject API references (called by ContextManager before initialize)
     *
     * Override in ContextBase to store the APIs reference.
     * Simple contexts that don't need APIs can ignore this.
     *
     * @param apis Reference to the APIs container
     */
    virtual void setAPIs(const context::APIs& /*apis*/) {}

    // ─────────────────────────────────────────────────────────────────────
    // Lifecycle (must implement)
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Initialize the context
     * @return Result<void> - ok() if initialization succeeded, err() to trigger fallback
     */
    virtual oc::Result<void> init() = 0;

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
