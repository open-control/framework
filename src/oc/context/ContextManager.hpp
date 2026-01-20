#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>

#include "APIs.hpp"
#include <oc/interface/IContext.hpp>
#include <oc/interface/IContextSwitcher.hpp>
#include "Requirements.hpp"

#include <oc/Config.hpp>
#include <oc/types/Result.hpp>
#include <oc/core/Warning.hpp>

namespace oc::context {

// Import from central config
using oc::MAX_CONTEXTS;

/// @brief Sentinel value indicating no valid context ID
inline constexpr uint8_t INVALID_CONTEXT_ID = 0xFF;

/// @cond INTERNAL
template <typename T, typename = void>
struct has_load_resources : std::false_type {};

template <typename T>
struct has_load_resources<T, std::void_t<decltype(T::loadResources())>> : std::true_type {};
/// @endcond

/**
 * @brief Manages context lifecycle, registration, and switching
 *
 * ContextManager is the central hub for managing application contexts.
 * It handles registration, instantiation, lifecycle management, and
 * safe switching between contexts.
 *
 * ## Registration
 *
 * Contexts are registered with a unique ID and name at startup:
 *
 * @code
 * enum class ContextID : uint8_t { MAIN = 0, SETTINGS = 1, DAW = 2 };
 *
 * manager.registerContext<MainContext>(ContextID::MAIN, "Main");
 * manager.registerContext<SettingsContext>(ContextID::SETTINGS, "Settings");
 * manager.registerContext<DawContext>(ContextID::DAW, "DAW Control");
 * @endcode
 *
 * ## Lifecycle
 *
 * 1. Call registerContext() for each context type
 * 2. Optionally call setDefault() to specify fallback context
 * 3. Call begin() to activate the default context
 * 4. Call update() every frame in the main loop
 *
 * ## Context Switching
 *
 * All context switching is **deferred** via switchTo() - the actual switch
 * occurs after the current update() cycle completes. This ensures safe
 * lifecycle management (no use-after-free when a context switches away
 * from itself).
 *
 * The only exception is begin() which performs an immediate switch to
 * bootstrap the application.
 *
 * ## Requirements Validation
 *
 * If a context defines static REQUIRES, registration will fail if the
 * required APIs are not configured.
 *
 * ## Resource Loading
 *
 * If a context defines static loadResources(), it will be called during
 * registration (before any instance is created).
 *
 * @see IContext
 * @see IContextSwitcher
 * @see Requirements
 */
class ContextManager : public interface::IContextSwitcher {
public:
    /// @brief Factory function type for creating context instances
    /// Uses std::function to support lambdas with captures
    using ContextFactory = std::function<std::unique_ptr<interface::IContext>()>;

    /**
     * @brief Construct a ContextManager with API references
     * @param apis Reference to APIs container (must outlive ContextManager)
     */
    explicit ContextManager(const APIs& apis);

    /**
     * @brief Destructor - cleans up active context
     */
    ~ContextManager() override;

    ContextManager(const ContextManager&) = delete;
    ContextManager& operator=(const ContextManager&) = delete;

    // ─────────────────────────────────────────────────────────────────────
    // Registration
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Register a context type with an ID and name
     *
     * The context is not instantiated until it becomes active.
     * The first registered context becomes the default.
     *
     * @tparam T Context class (must inherit from IContext)
     * @tparam ID Enum class or integral type for context IDs
     * @param id Unique identifier for this context (0-15)
     * @param name Human-readable name for UI/debugging
     * @return true if registration succeeded, false if:
     *         - ID >= MAX_CONTEXTS
     *         - ID already registered
     *         - Required APIs not available
     *
     * @code
     * manager.registerContext<MainContext>(ContextID::MAIN, "Main Menu");
     * @endcode
     */
    template <typename T, typename ID>
    bool registerContext(ID id, const char* name) {
        static_assert(std::is_base_of_v<interface::IContext, T>, "T must inherit from IContext");
        static_assert(std::is_enum_v<ID> || std::is_integral_v<ID>,
                      "ID must be an enum or integral type");

        const uint8_t idx = static_cast<uint8_t>(id);
        if (idx >= MAX_CONTEXTS) return false;
        if (factories_[idx] != nullptr) return false;

        // Validate requirements if context defines them
        if constexpr (has_requirements<T>::value) {
            if (T::REQUIRES.button && !apis_.button) {
                core::warn("[ContextManager] Context requires ButtonAPI but none provided");
                return false;
            }
            if (T::REQUIRES.encoder && !apis_.encoder) {
                core::warn("[ContextManager] Context requires EncoderAPI but none provided");
                return false;
            }
            if (T::REQUIRES.midi && !apis_.midi) {
                core::warn("[ContextManager] Context requires MidiAPI but none provided");
                return false;
            }
            if (T::REQUIRES.frames && !apis_.frames) {
                core::warn("[ContextManager] Context requires ITransport but none provided");
                return false;
            }
        }

        // Load static resources if context defines loadResources()
        if constexpr (has_load_resources<T>::value) {
            T::loadResources();
        }

        factories_[idx] = []() -> std::unique_ptr<interface::IContext> {
            return std::make_unique<T>();
        };
        names_[idx] = name;

        registered_count_++;
        if (default_id_ == INVALID_CONTEXT_ID) {
            default_id_ = idx;
        }
        return true;
    }

    /**
     * @brief Register a context with a custom factory function
     *
     * Use this variant when the context requires constructor arguments
     * (e.g., external state references).
     *
     * @tparam ID Enum class or integral type for context IDs
     * @param id Unique identifier for this context (0-15)
     * @param name Human-readable name for UI/debugging
     * @param factory Factory function that creates the context instance
     * @return true if registration succeeded
     *
     * @code
     * manager.registerContextWithFactory(
     *     ContextID::STANDALONE,
     *     "Standalone",
     *     [&state]() { return std::make_unique<StandaloneContext>(state); }
     * );
     * @endcode
     */
    template <typename ID>
    bool registerContextWithFactory(ID id, const char* name, ContextFactory factory) {
        static_assert(std::is_enum_v<ID> || std::is_integral_v<ID>,
                      "ID must be an enum or integral type");

        const uint8_t idx = static_cast<uint8_t>(id);
        if (idx >= MAX_CONTEXTS) return false;
        if (factories_[idx] != nullptr) return false;

        factories_[idx] = std::move(factory);
        names_[idx] = name;

        registered_count_++;
        if (default_id_ == INVALID_CONTEXT_ID) {
            default_id_ = idx;
        }
        return true;
    }

    // ─────────────────────────────────────────────────────────────────────
    // Context Switching (deferred - processed after update())
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Schedule a deferred switch to context by ID
     *
     * The switch will occur after the current update() cycle completes.
     *
     * @param id Raw context ID (0-255)
     */
    void switchToById(uint8_t id) override { pending_switch_ = id; }

    /**
     * @brief Schedule a deferred switch to the default context
     */
    void switchToDefault() override {
        if (default_id_ != INVALID_CONTEXT_ID) {
            pending_switch_ = default_id_;
        }
    }

    /**
     * @brief Check if a context with the given ID is registered
     * @param id Raw context ID
     * @return true if context exists
     */
    bool hasContextById(uint8_t id) const override {
        return id < MAX_CONTEXTS && factories_[id] != nullptr;
    }

    /**
     * @brief Get the registered name of a context
     * @param id Raw context ID
     * @return Name string, or nullptr if not registered
     */
    const char* contextName(uint8_t id) const override {
        if (id < MAX_CONTEXTS && names_[id]) return names_[id];
        return nullptr;
    }

    /**
     * @brief Get the ID of the currently active context
     * @return Active context ID, or INVALID_CONTEXT_ID if none
     */
    uint8_t activeId() const override { return active_id_; }

    /**
     * @brief Get the ID of the default context
     * @return Default context ID, or INVALID_CONTEXT_ID if not set
     */
    uint8_t defaultId() const override { return default_id_; }

    /**
     * @brief Get the number of registered contexts
     * @return Count of registered contexts
     */
    size_t contextCount() const override { return registered_count_; }

    // ─────────────────────────────────────────────────────────────────────
    // Configuration
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Set which context is the default/fallback
     *
     * The default context is activated by begin() and used as fallback
     * when initialization fails or DAW disconnects.
     *
     * @tparam ID Enum class or integral type
     * @param id Context ID to use as default
     */
    template <typename ID>
    void setDefault(ID id) {
        static_assert(std::is_enum_v<ID> || std::is_integral_v<ID>,
                      "ID must be an enum or integral type");
        default_id_ = static_cast<uint8_t>(id);
    }

    // ─────────────────────────────────────────────────────────────────────
    // State Queries
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Get the currently active context instance
     * @return Pointer to active context, or nullptr if none
     */
    interface::IContext* active() const { return active_.get(); }

    /**
     * @brief Check if a deferred switch is pending
     * @return true if a switch will occur after update()
     */
    bool hasPendingSwitch() const { return pending_switch_.has_value(); }

    // ─────────────────────────────────────────────────────────────────────
    // Lifecycle
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Activate the default context
     *
     * Call this once after all contexts are registered to start the application.
     *
     * @return Result<void> - ok() on success, err() with ErrorCode on failure
     */
    oc::Result<void> begin();

    /**
     * @brief Update the active context and process pending switches
     *
     * Call this every frame in the main loop. It will:
     * 1. Call active context's update()
     * 2. Process any pending deferred switch
     * 3. Handle DAW disconnection if applicable
     */
    void update();

protected:
    /// @copydoc IContextSwitcher::forEachContextImpl
    void forEachContextImpl(interface::IContextSwitcher::ContextCallback fn, void* userData) const override {
        for (uint8_t i = 0; i < MAX_CONTEXTS; ++i) {
            if (factories_[i]) {
                fn(i, names_[i], i == default_id_, userData);
            }
        }
    }

private:
    bool switchToImpl(uint8_t id);
    void processPendingSwitch();
    void emitActivated(uint8_t id, const interface::IContext& ctx);
    void emitDeactivated(uint8_t id, const interface::IContext& ctx);
    void emitError(uint8_t id);

    const APIs& apis_;                                    ///< Reference to shared APIs
    std::array<ContextFactory, MAX_CONTEXTS> factories_{};///< Context factory functions
    std::array<const char*, MAX_CONTEXTS> names_{};       ///< Context names for UI/debug
    std::unique_ptr<interface::IContext> active_;                    ///< Currently active context
    uint8_t active_id_ = INVALID_CONTEXT_ID;              ///< ID of active context
    uint8_t default_id_ = INVALID_CONTEXT_ID;             ///< ID of default/fallback context
    size_t registered_count_ = 0;                         ///< Number of registered contexts
    std::optional<uint8_t> pending_switch_;               ///< Deferred switch target
};

}  // namespace oc::context
