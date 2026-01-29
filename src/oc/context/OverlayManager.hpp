#pragma once

/**
 * @file OverlayManager.hpp
 * @brief Helper to wire overlays (visibility stack) with input authority
 *
 * Builds on:
 * - oc::state::ExclusiveVisibilityStack (which overlay is visible)
 * - oc::core::input::AuthorityResolver (which scope receives input)
 *
 * Goals:
 * - Provide a single source of truth for "current overlay scope"
 * - Clear latches / cleanup when overlays hide
 * - Avoid UAF by using RAII handles for callbacks and authority resolver
 */

#include <array>
#include <cstddef>

#include <oc/api/ButtonAPI.hpp>
#include <oc/core/input/AuthorityResolver.hpp>
#include <oc/state/ExclusiveVisibilityStack.hpp>
#include <oc/type/Ids.hpp>

namespace oc::context {

/**
 * @brief Cleanup info for an overlay (scope and optional latch button)
 */
struct OverlayCleanupInfo {
    oc::type::ScopeID scopeId = 0;
    oc::type::ButtonID latchButton = 0;
};

/**
 * @brief Overlay manager for ExclusiveVisibilityStack
 *
 * @tparam EnumT Overlay enum type (must have NONE=0 and COUNT members)
 */
template <typename EnumT>
class OverlayManager {
    static_assert(static_cast<int>(EnumT::NONE) == 0, "EnumT::NONE must be 0");
    static constexpr size_t COUNT = static_cast<size_t>(EnumT::COUNT);

public:
    explicit OverlayManager(oc::state::ExclusiveVisibilityStack<EnumT>& manager)
        : manager_(manager) {
        cleanup_handle_ = manager_.setCleanupCallbackScoped([this](EnumT type) { doCleanup(type); });
        authority_.setOverlayProvider([this]() { return currentScope(); });
    }

    OverlayManager(oc::state::ExclusiveVisibilityStack<EnumT>& manager, oc::api::ButtonAPI& buttons)
        : manager_(manager), buttons_(&buttons) {
        cleanup_handle_ = manager_.setCleanupCallbackScoped([this](EnumT type) { doCleanup(type); });
        authority_.setOverlayProvider([this]() { return currentScope(); });

        // Safely bind authority routing to this manager
        authority_handle_ = buttons_->setAuthorityResolverScoped(&authority_);
    }

    ~OverlayManager() = default;

    OverlayManager(const OverlayManager&) = delete;
    OverlayManager& operator=(const OverlayManager&) = delete;
    OverlayManager(OverlayManager&&) = delete;
    OverlayManager& operator=(OverlayManager&&) = delete;

    // =========================================================================
    // Registration
    // =========================================================================

    void registerCleanup(EnumT type, oc::type::ScopeID scopeId, oc::type::ButtonID latchButton = 0) {
        const auto idx = static_cast<size_t>(type);
        if (idx < COUNT) {
            cleanup_[idx] = {scopeId, latchButton};
        }
    }

    // =========================================================================
    // Delegation to ExclusiveVisibilityStack
    // =========================================================================

    void show(EnumT type, bool stack = false) { manager_.show(type, stack); }
    void hide() { manager_.hide(); }
    void hideAll() { manager_.hideAll(); }

    EnumT current() const { return manager_.current(); }
    bool hasVisible() const { return manager_.hasVisible(); }
    bool isCurrent(EnumT type) const { return manager_.current() == type; }

    // =========================================================================
    // Authority
    // =========================================================================

    oc::type::ScopeID currentScope() const {
        const auto type = manager_.current();
        if (type == EnumT::NONE) return 0;
        return cleanup_[static_cast<size_t>(type)].scopeId;
    }

    oc::core::input::AuthorityResolver& authority() { return authority_; }
    const oc::core::input::AuthorityResolver& authority() const { return authority_; }

    bool hasAuthority(oc::type::ScopeID scope) const { return authority_.hasAuthority(scope); }

    oc::type::ScopeID getScopeFor(EnumT type) const {
        const auto idx = static_cast<size_t>(type);
        if (idx < COUNT) return cleanup_[idx].scopeId;
        return 0;
    }

    /**
     * @brief (Re)bind input authority routing to this manager
     */
    void bindAuthority(oc::api::ButtonAPI& buttons) {
        buttons_ = &buttons;
        authority_handle_.reset();
        authority_handle_ = buttons_->setAuthorityResolverScoped(&authority_);
    }

private:
    void doCleanup(EnumT type) {
        if (type == EnumT::NONE || type == EnumT::COUNT) return;

        const auto idx = static_cast<size_t>(type);
        const auto& info = cleanup_[idx];

        if (info.latchButton != 0 && buttons_) {
            buttons_->clearLatch(info.latchButton);
        }
    }

    oc::state::ExclusiveVisibilityStack<EnumT>& manager_;
    oc::api::ButtonAPI* buttons_ = nullptr;

    oc::core::input::AuthorityResolver authority_{};
    std::array<OverlayCleanupInfo, COUNT> cleanup_{};

    typename oc::state::ExclusiveVisibilityStack<EnumT>::CleanupHandle cleanup_handle_{};
    oc::api::ButtonAPI::AuthorityResolverHandle authority_handle_{};
};

}  // namespace oc::context
