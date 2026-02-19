#pragma once

#include <cstdint>

#include <oc/core/input/AuthorityResolver.hpp>
#include <oc/core/input/ButtonBuilder.hpp>
#include <oc/core/input/Binding.hpp>
#include <oc/interface/IButton.hpp>
#include <oc/type/Ids.hpp>
#include <oc/type/Callbacks.hpp>

namespace oc::core::input {
class InputBinding;
}

namespace oc::api {

/**
 * @brief API for button bindings and state management
 *
 * Provides:
 * - Fluent binding API via button(id)
 * - Scope/binding cleanup
 * - Button state queries (isPressed, isLatched)
 * - Latch state management
 *
 * @code
 * // Via IContext accessors:
 * onButton(BTN_1).press().then([this]{ doAction(); });
 * button(BTN_1).clearLatch();  // Clear latch for a button
 * buttons().clearBindings();
 * @endcode
 */
class ButtonAPI {
public:
    ButtonAPI(core::input::InputBinding& binding, interface::IButton& hw);

    // ═══════════════════════════════════════════════════
    // RAII handles
    // ═══════════════════════════════════════════════════

    /**
     * @brief RAII handle for a scoped authority resolver assignment
     */
    class AuthorityResolverHandle {
    public:
        AuthorityResolverHandle() = default;

        AuthorityResolverHandle(const AuthorityResolverHandle&) = delete;
        AuthorityResolverHandle& operator=(const AuthorityResolverHandle&) = delete;

        AuthorityResolverHandle(AuthorityResolverHandle&& other) noexcept;
        AuthorityResolverHandle& operator=(AuthorityResolverHandle&& other) noexcept;

        ~AuthorityResolverHandle();

        void reset();

    private:
        friend class ButtonAPI;
        AuthorityResolverHandle(core::input::InputBinding* binding, uint32_t token)
            : binding_(binding), token_(token) {}

        core::input::InputBinding* binding_ = nullptr;
        uint32_t token_ = 0;
    };

    // ═══════════════════════════════════════════════════
    // Binding fluent API
    // ═══════════════════════════════════════════════════

    /**
     * @brief Start building a button binding
     * @param id The button to bind (uint16_t or enum class : uint16_t)
     * @return ButtonBuilder for chained configuration
     */
    [[nodiscard]] core::input::ButtonBuilder button(oc::type::ButtonID id);

    /// @brief Template overload for enum class button IDs
    template <typename EnumT, typename = std::enable_if_t<oc::type::is_id_v<EnumT>>>
    [[nodiscard]] core::input::ButtonBuilder button(EnumT id) {
        return button(static_cast<oc::type::ButtonID>(id));
    }

    // ═══════════════════════════════════════════════════
    // Scope/cleanup
    // ═══════════════════════════════════════════════════

    /// Clear all button bindings
    void clearBindings();

    /// Clear button bindings in a specific scope
    void clearScope(oc::type::ScopeID scope);

    /**
     * @brief Set the authority resolver for scope-based input filtering
     *
     * When set, scoped bindings will only trigger if their scope has authority.
     * This prevents multiple overlays from receiving the same input events.
     *
     * @param resolver Pointer to the authority resolver (nullptr to disable)
     */
    void setAuthorityResolver(const core::input::AuthorityResolver* resolver);

    /**
     * @brief Set authority resolver and return a handle that clears it on destruction
     */
    [[nodiscard]] AuthorityResolverHandle setAuthorityResolverScoped(const core::input::AuthorityResolver* resolver);

    // ═══════════════════════════════════════════════════
    // Button state
    // ═══════════════════════════════════════════════════

    /// Check if button is currently pressed (instantaneous state)
    bool isPressed(oc::type::ButtonID id) const;
    template <typename EnumT, typename = std::enable_if_t<oc::type::is_id_v<EnumT>>>
    bool isPressed(EnumT id) const { return isPressed(static_cast<oc::type::ButtonID>(id)); }

    /**
     * @brief Override current press ownership for a button
     *
     * Advanced API for flows that switch overlay scope on press and require
     * release routing to follow the new scope.
     */
    void setPressOwner(oc::type::ButtonID id, oc::type::ScopeID scope);
    template <typename EnumT, typename = std::enable_if_t<oc::type::is_id_v<EnumT>>>
    void setPressOwner(EnumT id, oc::type::ScopeID scope) {
        setPressOwner(static_cast<oc::type::ButtonID>(id), scope);
    }

    /**
     * @brief Get a predicate for use with when()
     * @param id The button to check
     * @return oc::type::IsActiveFn that returns true when button is pressed
     *
     * @code
     * onEncoder(ENC_1).turn()
     *     .when(button(BTN_SHIFT).pressed())
     *     .then([](float v){ fineAdjust(v); });
     * @endcode
     */
    oc::type::IsActiveFn pressed(oc::type::ButtonID id) const;
    template <typename EnumT, typename = std::enable_if_t<oc::type::is_id_v<EnumT>>>
    oc::type::IsActiveFn pressed(EnumT id) const { return pressed(static_cast<oc::type::ButtonID>(id)); }

    // ═══════════════════════════════════════════════════
    // Latch state
    // ═══════════════════════════════════════════════════

    /// Check if button is in latched state
    bool isLatched(oc::type::ButtonID id) const;
    template <typename EnumT, typename = std::enable_if_t<oc::type::is_id_v<EnumT>>>
    bool isLatched(EnumT id) const { return isLatched(static_cast<oc::type::ButtonID>(id)); }

    /// Clear button latch state
    void clearLatch(oc::type::ButtonID id);
    template <typename EnumT, typename = std::enable_if_t<oc::type::is_id_v<EnumT>>>
    void clearLatch(EnumT id) { clearLatch(static_cast<oc::type::ButtonID>(id)); }

private:
    core::input::InputBinding& binding_;
    interface::IButton& hw_;
};

}  // namespace oc::api
