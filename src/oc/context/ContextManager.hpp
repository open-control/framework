#pragma once

#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>

#include "IContext.hpp"

namespace oc::api {
class ControlAPI;
}

namespace oc::context {

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
 * Contexts are registered by type and identified by string ID.
 * Only one context is active at a time. Switching emits events
 * for other systems to react.
 *
 * @code
 * ContextManager mgr(api);
 * mgr.registerContext<BitwigContext>("bitwig");
 * mgr.registerContext<StandaloneContext>("standalone");
 * mgr.setDefault("standalone");
 * mgr.switchToDefault();
 * @endcode
 */
class ContextManager {
public:
    explicit ContextManager(oc::api::ControlAPI& api);
    ~ContextManager();

    ContextManager(const ContextManager&) = delete;
    ContextManager& operator=(const ContextManager&) = delete;

    // ═══════════════════════════════════════════════════
    // Registration
    // ═══════════════════════════════════════════════════

    /**
     * @brief Register a context type with given ID (lazy initialization)
     * @tparam T Context class deriving from IContext
     * @param id Unique identifier for this context
     * @return true if registration succeeded
     *
     * If T has a static loadResources() method, it will be called
     * immediately (SFINAE detection).
     *
     * @note Context initialization is deferred until first switchTo().
     *       This allows registering multiple contexts without allocating
     *       resources for inactive ones.
     */
    template <typename T>
    bool registerContext(const std::string& id) {
        static_assert(std::is_base_of_v<IContext, T>, "T must inherit from IContext");

        if (contexts_.find(id) != contexts_.end()) {
            return false;  // Already registered
        }

        // Load resources if available (SFINAE) - still done eagerly
        if constexpr (has_load_resources<T>::value) {
            T::loadResources();
        }

        // Create context but don't initialize yet (lazy init)
        auto ctx = std::make_unique<T>();
        contexts_[id] = std::move(ctx);
        emitRegistered(*contexts_[id]);

        // First context becomes default
        if (default_id_.empty()) {
            default_id_ = id;
        }

        return true;
    }

    // ═══════════════════════════════════════════════════
    // Switching
    // ═══════════════════════════════════════════════════

    /// Switch to context by ID, returns false if not found
    bool switchTo(const std::string& id);

    /// Switch to the default context
    void switchToDefault();

    /// Set which context ID is the default
    void setDefault(const std::string& id);

    // ═══════════════════════════════════════════════════
    // State
    // ═══════════════════════════════════════════════════

    /// Get currently active context (may be nullptr)
    IContext* active() const { return active_; }

    /// Get default context ID
    const std::string& defaultId() const { return default_id_; }

    /// Check if a context is registered
    bool hasContext(const std::string& id) const;

    // ═══════════════════════════════════════════════════
    // Update
    // ═══════════════════════════════════════════════════

    /// Call active context's update() - should be called every frame
    void update();

private:
    void emitRegistered(const IContext& ctx);
    void emitActivated(const IContext& ctx);
    void emitDeactivated(const IContext& ctx);
    void emitError(const IContext& ctx);

    oc::api::ControlAPI& api_;
    std::unordered_map<std::string, std::unique_ptr<IContext>> contexts_;
    std::unordered_set<std::string> initialized_ids_;  ///< Tracks lazy-initialized contexts
    IContext* active_ = nullptr;
    std::string default_id_;
};

}  // namespace oc::context
