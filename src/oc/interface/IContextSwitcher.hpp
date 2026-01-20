#pragma once

#include <cstdint>
#include <type_traits>

namespace oc::interface {

/**
 * @brief Information about a registered context for iteration
 *
 * Used by forEachContext() to provide context metadata without allocation.
 */
struct ContextInfo {
    uint8_t id;         ///< Unique identifier of the context
    const char* name;   ///< Human-readable name (set at registration)
    bool isDefault;     ///< True if this is the default/fallback context
};

/**
 * @brief Interface for context switching operations
 *
 * This interface decouples IContext from ContextManager to avoid circular
 * dependency. It is implemented by ContextManager and exposed to contexts
 * via the APIs struct.
 *
 * ## Deferred vs Immediate Switching
 *
 * All switching methods in this interface are **deferred**: they schedule
 * the switch to occur after the current update() cycle completes. This is
 * critical for safe lifecycle management - a context can request a switch
 * without causing use-after-free when it gets destroyed.
 *
 * For immediate switching (e.g., from main.cpp or boot sequence), use
 * ContextManager::switchToImmediate() directly.
 *
 * ## Usage from IContext
 *
 * Contexts access this interface through protected helper methods:
 * @code
 * class MyContext : public IContext {
 *     void onButtonPress() {
 *         // Request switch to another context (deferred)
 *         switchTo(ContextID::OTHER);
 *     }
 *
 *     void buildMenu() {
 *         // List all available contexts
 *         forEachContext([](const ContextInfo& info) {
 *             menu.addItem(info.name, info.id);
 *         });
 *     }
 * };
 * @endcode
 */
class IContextSwitcher {
public:
    virtual ~IContextSwitcher() = default;

    // ─────────────────────────────────────────────────────────────────────
    // Deferred Switching
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Request a deferred switch to the context with given ID
     *
     * The switch will occur after the current update() cycle completes.
     * Safe to call from within a context's update() or event handlers.
     *
     * @param id Raw context ID (0-255)
     * @note Prefer using switchTo<ID>() with enum class for type safety
     */
    virtual void switchToById(uint8_t id) = 0;

    /**
     * @brief Request a deferred switch to the default context
     *
     * The switch will occur after the current update() cycle completes.
     * Useful for "home" or "back to main menu" actions.
     */
    virtual void switchToDefault() = 0;

    // ─────────────────────────────────────────────────────────────────────
    // State Queries
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Check if a context with the given ID is registered
     * @param id Raw context ID to check
     * @return true if context exists, false otherwise
     */
    virtual bool hasContextById(uint8_t id) const = 0;

    /**
     * @brief Get the name of a registered context
     * @param id Raw context ID
     * @return Context name, or nullptr if ID is not registered
     */
    virtual const char* contextName(uint8_t id) const = 0;

    /**
     * @brief Get the ID of the currently active context
     * @return Active context ID, or INVALID_CONTEXT_ID (0xFF) if none active
     */
    virtual uint8_t activeId() const = 0;

    /**
     * @brief Get the ID of the default/fallback context
     * @return Default context ID, or INVALID_CONTEXT_ID (0xFF) if not set
     */
    virtual uint8_t defaultId() const = 0;

    /**
     * @brief Get the number of registered contexts
     * @return Count of registered contexts (0 to MAX_CONTEXTS)
     */
    virtual size_t contextCount() const = 0;

    // ─────────────────────────────────────────────────────────────────────
    // Iteration
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Iterate over all registered contexts
     *
     * Calls the provided callback for each registered context. This is useful
     * for building menus, debugging, or context selection UI.
     *
     * @tparam Fn Callable type accepting const ContextInfo&
     * @param fn Callback invoked for each context
     *
     * @code
     * switcher->forEachContext([](const ContextInfo& info) {
     *     printf("%d: %s%s\n", info.id, info.name,
     *            info.isDefault ? " (default)" : "");
     * });
     * @endcode
     *
     * @note Zero allocation - uses type-erased callback internally
     */
    template <typename Fn>
    void forEachContext(Fn&& fn) const {
        auto wrapper = [](uint8_t id, const char* name, bool isDefault, void* userData) {
            auto& callback = *static_cast<std::decay_t<Fn>*>(userData);
            callback(ContextInfo{id, name, isDefault});
        };
        forEachContextImpl(wrapper, const_cast<void*>(static_cast<const void*>(&fn)));
    }

    // ─────────────────────────────────────────────────────────────────────
    // Type-Safe Template Helpers
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Request a deferred switch using an enum class ID
     *
     * Type-safe wrapper around switchToById() that accepts enum class types.
     *
     * @tparam ID Enum class or integral type representing context IDs
     * @param id Context ID to switch to
     *
     * @code
     * enum class ContextID : uint8_t { MAIN = 0, SETTINGS = 1 };
     * switcher->switchTo(ContextID::SETTINGS);
     * @endcode
     */
    template <typename ID>
    void switchTo(ID id) {
        static_assert(std::is_enum_v<ID> || std::is_integral_v<ID>,
                      "ID must be an enum or integral type");
        switchToById(static_cast<uint8_t>(id));
    }

    /**
     * @brief Check if a context exists using an enum class ID
     *
     * Type-safe wrapper around hasContextById().
     *
     * @tparam ID Enum class or integral type representing context IDs
     * @param id Context ID to check
     * @return true if context is registered, false otherwise
     */
    template <typename ID>
    bool hasContext(ID id) const {
        static_assert(std::is_enum_v<ID> || std::is_integral_v<ID>,
                      "ID must be an enum or integral type");
        return hasContextById(static_cast<uint8_t>(id));
    }

protected:
    /// @brief Function pointer type for context iteration callback
    using ContextCallback = void (*)(uint8_t id, const char* name, bool isDefault, void* userData);

    /**
     * @brief Implementation hook for forEachContext iteration
     *
     * Subclasses implement this to iterate over their registered contexts.
     * The callback receives raw parameters plus userData for type erasure.
     *
     * @param fn Callback function pointer (non-capturing lambda convertible)
     * @param userData Opaque pointer passed through to callback
     */
    virtual void forEachContextImpl(ContextCallback fn, void* userData) const = 0;
};

}  // namespace oc::interface
