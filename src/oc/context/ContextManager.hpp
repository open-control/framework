#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <type_traits>

#include "IContext.hpp"

namespace oc::api {
class ControlAPI;
}

namespace oc::context {

/**
 * @brief Maximum number of contexts that can be registered
 *
 * Fixed-size array eliminates dynamic allocation.
 * 16 slots is sufficient for most applications.
 */
inline constexpr size_t MAX_CONTEXTS = 16;

/**
 * @brief Invalid context ID sentinel
 */
inline constexpr uint8_t INVALID_CONTEXT_ID = 0xFF;

/**
 * @brief SFINAE helper to detect static loadResources() method
 */
template <typename T, typename = void>
struct has_load_resources : std::false_type {};

template <typename T>
struct has_load_resources<T, std::void_t<decltype(T::loadResources())>> : std::true_type {};

/**
 * @brief Manages context registration, lifecycle, and switching
 *
 * Uses factory pattern with fixed-size storage. Only ONE context
 * is in memory at a time - switching destroys the old and creates the new.
 *
 * Context IDs are user-defined enums (e.g., `enum class ContextID : uint8_t`).
 * The framework stores them as uint8_t internally.
 *
 * @code
 * // User-defined in Config.hpp:
 * enum class ContextID : uint8_t {
 *     STANDALONE = 0,
 *     BITWIG = 1,
 *     _COUNT  // Optional: for compile-time validation
 * };
 *
 * // Registration:
 * mgr.registerContext<StandaloneContext>(ContextID::STANDALONE);
 * mgr.registerContext<BitwigContext>(ContextID::BITWIG);
 * mgr.setDefault(ContextID::STANDALONE);
 * mgr.begin();  // Activates default context
 *
 * // Runtime switching:
 * mgr.switchTo(ContextID::BITWIG);
 * @endcode
 */
class ContextManager {
public:
    /// Factory function signature: creates a new context instance
    using ContextFactory = std::unique_ptr<IContext> (*)();

    explicit ContextManager(oc::api::ControlAPI& api);
    ~ContextManager();

    ContextManager(const ContextManager&) = delete;
    ContextManager& operator=(const ContextManager&) = delete;

    // ═══════════════════════════════════════════════════
    // Registration
    // ═══════════════════════════════════════════════════

    /**
     * @brief Register a context factory for a given ID
     * @tparam T Context class deriving from IContext
     * @tparam ID Enum type convertible to uint8_t
     * @param id Context identifier (user-defined enum value)
     * @return true if registration succeeded
     *
     * If T has a static loadResources() method, it will be called
     * immediately for resource preloading.
     *
     * @note Context is NOT created yet - only the factory is stored.
     *       Actual instantiation happens on first switchTo().
     */
    template <typename T, typename ID>
    bool registerContext(ID id) {
        static_assert(std::is_base_of_v<IContext, T>, "T must inherit from IContext");
        static_assert(std::is_enum_v<ID> || std::is_integral_v<ID>,
                      "ID must be an enum or integral type");

        const uint8_t idx = static_cast<uint8_t>(id);
        if (idx >= MAX_CONTEXTS) {
            return false;  // Out of range
        }
        if (factories_[idx] != nullptr) {
            return false;  // Already registered
        }

        // Load resources if available (SFINAE)
        if constexpr (has_load_resources<T>::value) {
            T::loadResources();
        }

        // Store factory function (no allocation, just function pointer)
        factories_[idx] = []() -> std::unique_ptr<IContext> {
            return std::make_unique<T>();
        };

        // Track for iteration/debugging
        registered_count_++;

        // First registered context becomes default
        if (default_id_ == INVALID_CONTEXT_ID) {
            default_id_ = idx;
        }

        return true;
    }

    // ═══════════════════════════════════════════════════
    // Switching
    // ═══════════════════════════════════════════════════

    /**
     * @brief Switch to context by ID
     * @tparam ID Enum type convertible to uint8_t
     * @param id Context identifier
     * @return true if switch succeeded
     *
     * Destroys the current context and creates the new one.
     * If initialization fails, falls back to default context.
     */
    template <typename ID>
    bool switchTo(ID id) {
        static_assert(std::is_enum_v<ID> || std::is_integral_v<ID>,
                      "ID must be an enum or integral type");
        return switchToImpl(static_cast<uint8_t>(id));
    }

    /// Switch to the default context
    void switchToDefault();

    /**
     * @brief Set which context ID is the default
     * @tparam ID Enum type convertible to uint8_t
     * @param id Context identifier to use as default
     */
    template <typename ID>
    void setDefault(ID id) {
        static_assert(std::is_enum_v<ID> || std::is_integral_v<ID>,
                      "ID must be an enum or integral type");
        default_id_ = static_cast<uint8_t>(id);
    }

    // ═══════════════════════════════════════════════════
    // State
    // ═══════════════════════════════════════════════════

    /// Get currently active context (may be nullptr before begin())
    IContext* active() const { return active_.get(); }

    /// Get active context ID (INVALID_CONTEXT_ID if none)
    uint8_t activeId() const { return active_id_; }

    /// Get default context ID
    uint8_t defaultId() const { return default_id_; }

    /// Check if a context is registered
    template <typename ID>
    bool hasContext(ID id) const {
        const uint8_t idx = static_cast<uint8_t>(id);
        return idx < MAX_CONTEXTS && factories_[idx] != nullptr;
    }

    /// Number of registered contexts
    size_t registeredCount() const { return registered_count_; }

    // ═══════════════════════════════════════════════════
    // Lifecycle
    // ═══════════════════════════════════════════════════

    /// Initialize and activate the default context
    bool begin();

    /// Call active context's update() - should be called every frame
    /// Also checks isConnected() and triggers fallback if disconnected
    void update();

private:
    bool switchToImpl(uint8_t id);
    void emitActivated(uint8_t id, const IContext& ctx);
    void emitDeactivated(uint8_t id, const IContext& ctx);
    void emitError(uint8_t id);

    oc::api::ControlAPI& api_;

    // Fixed-size storage - no dynamic allocation
    std::array<ContextFactory, MAX_CONTEXTS> factories_{};

    // Only ONE context in memory at a time
    std::unique_ptr<IContext> active_;
    uint8_t active_id_ = INVALID_CONTEXT_ID;
    uint8_t default_id_ = INVALID_CONTEXT_ID;
    size_t registered_count_ = 0;
};

}  // namespace oc::context
