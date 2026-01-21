#pragma once

/**
 * @file AuthorityResolver.hpp
 * @brief Determines which scope has input authority at any given time
 *
 * Input authority follows a strict hierarchy:
 * 1. Top overlay (if any)
 * 2. Active view
 * 3. Global (scope = 0)
 *
 * This ensures that only one scope receives input at any time,
 * preventing ghost inputs and scope conflicts.
 */

#include <functional>
#include <vector>

#include <oc/core/input/Binding.hpp>

namespace oc::core::input {

/**
 * @brief Resolves which scope currently has input authority
 *
 * Usage:
 * @code
 * AuthorityResolver resolver;
 *
 * // Set up overlay stack provider
 * resolver.setOverlayProvider([&overlayManager]() {
 *     return overlayManager.currentScope();
 * });
 *
 * // Set up active view provider
 * resolver.setActiveViewProvider([&viewManager]() {
 *     return viewManager.activeViewScope();
 * });
 *
 * // Query authority
 * oc::type::ScopeID auth = resolver.getAuthority();
 * if (resolver.hasAuthority(myScope)) {
 *     // This scope can receive input
 * }
 * @endcode
 */
class AuthorityResolver {
public:
    /// Provider function that returns a oc::type::ScopeID (0 = none/inactive)
    using ScopeProvider = std::function<oc::type::ScopeID()>;

    AuthorityResolver() = default;
    ~AuthorityResolver() = default;

    // Non-copyable, movable
    AuthorityResolver(const AuthorityResolver&) = delete;
    AuthorityResolver& operator=(const AuthorityResolver&) = delete;
    AuthorityResolver(AuthorityResolver&&) = default;
    AuthorityResolver& operator=(AuthorityResolver&&) = default;

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * @brief Set the overlay stack provider
     *
     * Should return the oc::type::ScopeID of the top overlay, or 0 if no overlay is active.
     * This has the highest priority.
     */
    void setOverlayProvider(ScopeProvider provider) {
        overlayProvider_ = std::move(provider);
    }

    /**
     * @brief Set the active view provider
     *
     * Should return the oc::type::ScopeID of the currently active view, or 0 if none.
     * This has second priority after overlays.
     */
    void setActiveViewProvider(ScopeProvider provider) {
        activeViewProvider_ = std::move(provider);
    }

    /**
     * @brief Add a custom authority layer
     *
     * Custom layers are checked after overlay but before active view.
     * Higher index = higher priority within custom layers.
     *
     * @param provider Function returning the scope for this layer (0 = inactive)
     */
    void addCustomLayer(ScopeProvider provider) {
        customLayers_.push_back(std::move(provider));
    }

    /**
     * @brief Clear all custom layers
     */
    void clearCustomLayers() {
        customLayers_.clear();
    }

    // =========================================================================
    // Authority Query
    // =========================================================================

    /**
     * @brief Get the scope that currently has input authority
     *
     * Priority order:
     * 1. Overlay (if overlayProvider returns non-zero)
     * 2. Custom layers (in reverse order - last added = highest priority)
     * 3. Active view (if activeViewProvider returns non-zero)
     * 4. Global (returns 0)
     *
     * @return oc::type::ScopeID of the authoritative scope (0 = global)
     */
    oc::type::ScopeID getAuthority() const {
        // 1. Check overlay (highest priority)
        if (overlayProvider_) {
            oc::type::ScopeID overlay = overlayProvider_();
            if (overlay != 0) return overlay;
        }

        // 2. Check custom layers (reverse order)
        for (auto it = customLayers_.rbegin(); it != customLayers_.rend(); ++it) {
            if (*it) {
                oc::type::ScopeID custom = (*it)();
                if (custom != 0) return custom;
            }
        }

        // 3. Check active view
        if (activeViewProvider_) {
            oc::type::ScopeID view = activeViewProvider_();
            if (view != 0) return view;
        }

        // 4. Fall back to global
        return 0;
    }

    /**
     * @brief Check if a specific scope currently has authority
     *
     * A scope has authority if:
     * - It is the current authority (getAuthority() returns this scope), OR
     * - scope == 0 (global bindings always participate, but at lowest priority)
     *
     * Note: This method answers "can this scope receive input?" not
     * "is this scope the exclusive authority?". Global bindings (scope=0)
     * can still be blocked by scoped bindings in InputBinding dispatch logic.
     *
     * @param scope The scope to check
     * @return true if this scope can participate in input dispatch
     */
    bool hasAuthority(oc::type::ScopeID scope) const {
        if (scope == 0) {
            // Global scope always participates (but lowest priority)
            return true;
        }
        return scope == getAuthority();
    }

    /**
     * @brief Check if a specific scope is the exclusive authority
     *
     * Unlike hasAuthority(), this returns true ONLY if this scope
     * is the top-level authority (not global fallback).
     *
     * @param scope The scope to check
     * @return true if this scope is the exclusive authority
     */
    bool isExclusiveAuthority(oc::type::ScopeID scope) const {
        return scope != 0 && scope == getAuthority();
    }

    /**
     * @brief Check if any overlay or view is currently blocking global input
     *
     * @return true if a non-global scope has authority
     */
    bool isGlobalBlocked() const {
        return getAuthority() != 0;
    }

private:
    ScopeProvider overlayProvider_;
    ScopeProvider activeViewProvider_;
    std::vector<ScopeProvider> customLayers_;
};

}  // namespace oc::core::input
