#pragma once

/**
 * @file ExclusiveVisibilityStack.hpp
 * @brief Template for managing exclusive visibility with single-level stacking
 *
 * Manages visibility of mutually exclusive items with support for one stacked item.
 * When stacking, the previous item remains visible underneath.
 *
 * @tparam EnumT Enum type for item identifiers. Must have:
 *         - NONE as first value (0)
 *         - COUNT as last value
 */

#include <array>
#include <cstdint>
#include <functional>

#include <config/PlatformCompat.hpp>
#include <oc/log/Log.hpp>

#include "Signal.hpp"

namespace oc::state {

/**
 * @brief Manages exclusive visibility of items with single-level stacking
 *
 * Generic visibility stack supporting one level of stacking.
 * When stacking, both items are visible; the top one has priority.
 * Registered signals may be prepared by their owning state before a stack
 * transition. show() only replaces items already tracked by this stack;
 * hideAll() is the explicit reconciliation boundary that clears every
 * registered visible signal.
 *
 * @tparam EnumT Enum type with NONE and COUNT values
 *
 * Usage:
 * @code
 * enum class OverlayType : uint8_t { NONE = 0, PAGE_SELECTOR, DEVICE_SELECTOR, COUNT };
 * ExclusiveVisibilityStack<OverlayType> overlays;
 *
 * overlays.registerItem(OverlayType::DEVICE_SELECTOR, deviceVisible);
 * overlays.show(OverlayType::DEVICE_SELECTOR);              // Shows device
 * overlays.show(OverlayType::PAGE_SELECTOR, true);          // Page on top, device stays visible
 * overlays.hide();                                          // Hides page, device is current
 * overlays.hide();                                          // Hides device
 * @endcode
 */
template <typename EnumT>
class ExclusiveVisibilityStack {
    static_assert(static_cast<int>(EnumT::NONE) == 0, "EnumT::NONE must be 0");
    static constexpr size_t COUNT = static_cast<size_t>(EnumT::COUNT);

public:
    // Keep template methods out of FLASHMEM: GCC emits inline template
    // instantiations in COMDAT sections, which conflicts with Teensy's shared
    // `.flashmem` section on direct PlatformIO builds.
    /// Cleanup callback type - called before hiding an item
    using CleanupCallback = std::function<void(EnumT)>;
    using VisibilityTransitionCallback = void (*)(void* context, EnumT type, bool visible);

    /**
     * @brief RAII handle for a registered cleanup callback
     *
     * Prevents use-after-free when a callback captures an object that
     * may be destroyed before this stack.
     */
    class CleanupHandle {
    public:
        CleanupHandle() = default;

        CleanupHandle(const CleanupHandle&) = delete;
        CleanupHandle& operator=(const CleanupHandle&) = delete;

        CleanupHandle(CleanupHandle&& other) noexcept
            : owner_(other.owner_), token_(other.token_) {
            other.owner_ = nullptr;
            other.token_ = 0;
        }

        CleanupHandle& operator=(CleanupHandle&& other) noexcept {
            if (this == &other) return *this;
            reset();
            owner_ = other.owner_;
            token_ = other.token_;
            other.owner_ = nullptr;
            other.token_ = 0;
            return *this;
        }

        ~CleanupHandle() { reset(); }

        void reset() {
            if (owner_ && token_ != 0) {
                owner_->clearCleanupCallback(token_);
            }
            owner_ = nullptr;
            token_ = 0;
        }

    private:
        friend class ExclusiveVisibilityStack;
        CleanupHandle(ExclusiveVisibilityStack* owner, uint32_t token)
            : owner_(owner), token_(token) {}

        ExclusiveVisibilityStack* owner_ = nullptr;
        uint32_t token_ = 0;
    };

    class VisibilityTransitionHandle {
    public:
        VisibilityTransitionHandle() = default;

        VisibilityTransitionHandle(const VisibilityTransitionHandle&) = delete;
        VisibilityTransitionHandle& operator=(const VisibilityTransitionHandle&) = delete;

        VisibilityTransitionHandle(VisibilityTransitionHandle&& other) noexcept
            : owner_(other.owner_), token_(other.token_) {
            other.owner_ = nullptr;
            other.token_ = 0;
        }

        VisibilityTransitionHandle& operator=(VisibilityTransitionHandle&& other) noexcept {
            if (this == &other) return *this;
            reset();
            owner_ = other.owner_;
            token_ = other.token_;
            other.owner_ = nullptr;
            other.token_ = 0;
            return *this;
        }

        ~VisibilityTransitionHandle() { reset(); }

        void reset() {
            if (owner_ && token_ != 0) {
                owner_->clearVisibilityTransitionCallback(token_);
            }
            owner_ = nullptr;
            token_ = 0;
        }

    private:
        friend class ExclusiveVisibilityStack;
        VisibilityTransitionHandle(ExclusiveVisibilityStack* owner, uint32_t token)
            : owner_(owner), token_(token) {}

        ExclusiveVisibilityStack* owner_ = nullptr;
        uint32_t token_ = 0;
    };

    ExclusiveVisibilityStack() = default;

    // Non-copyable, non-movable (holds pointers to signals)
    ExclusiveVisibilityStack(const ExclusiveVisibilityStack&) = delete;
    ExclusiveVisibilityStack& operator=(const ExclusiveVisibilityStack&) = delete;
    ExclusiveVisibilityStack(ExclusiveVisibilityStack&&) = delete;
    ExclusiveVisibilityStack& operator=(ExclusiveVisibilityStack&&) = delete;

    /**
     * @brief Set cleanup callback called before hiding any item
     *
     * Use this to perform cleanup actions (e.g., clear button latches)
     * without creating dependencies in the framework.
     *
     * @param callback Function called with item type before hide
     */
    void setCleanupCallback(CleanupCallback callback) {
        cleanupCallback_ = std::move(callback);
        // Invalidate any outstanding handles
        cleanup_token_++;
        if (cleanup_token_ == 0) cleanup_token_ = 1;
    }

    /**
     * @brief Set cleanup callback with automatic lifetime management
     *
     * The returned handle clears the callback when destroyed.
     */
    [[nodiscard]] CleanupHandle setCleanupCallbackScoped(CleanupCallback callback) {
        cleanupCallback_ = std::move(callback);
        cleanup_token_++;
        if (cleanup_token_ == 0) cleanup_token_ = 1;  // avoid 0 as sentinel
        return CleanupHandle(this, cleanup_token_);
    }

    [[nodiscard]] VisibilityTransitionHandle setVisibilityTransitionCallbackScoped(
        void* context,
        VisibilityTransitionCallback callback
    ) {
        visibility_transition_context_ = context;
        visibility_transition_callback_ = callback;
        visibility_transition_token_++;
        if (visibility_transition_token_ == 0) visibility_transition_token_ = 1;
        return VisibilityTransitionHandle(this, visibility_transition_token_);
    }

    /**
     * @brief Show an item
     * @param type The item to show
     * @param stack If true, current item stays visible underneath
     */
    void show(EnumT type, bool stack = false) {
        if (type == EnumT::NONE || static_cast<size_t>(type) >= COUNT) {
            hideAll();
            return;
        }

        if (current_ == type) {
            bool changed = false;
            if (!stack && previous_ != EnumT::NONE) {
                hideItem(previous_);
                previous_ = EnumT::NONE;
                changed = true;
            }
            if (!itemVisible(type)) {
                setVisible(type, true, true);
                changed = true;
            }
            if (changed) bumpRevision();
            return;
        }

        if (stack && current_ != EnumT::NONE && current_ != type) {
            // This stack intentionally retains one underlying item. Collapse
            // an older level before promoting the current item.
            if (previous_ != EnumT::NONE && previous_ != current_) {
                hideItem(previous_);
            }
            previous_ = current_;
        } else if (current_ != EnumT::NONE) {
            hideItem(current_);
            if (previous_ != EnumT::NONE && previous_ != current_) {
                hideItem(previous_);
            }
            previous_ = EnumT::NONE;
        }

        setVisible(type, true, true);
        current_ = type;
        bumpRevision();
        OC_LOG_DEBUG("[ExclusiveVisibilityStack] show: {} (stack: {})", static_cast<int>(type), stack);
    }

    /**
     * @brief Hide current item, restore previous if stacked
     *
     * Calls cleanup callback (if set) before hiding.
     */
    void hide() {
        if (current_ == EnumT::NONE) return;

        hideItem(current_);
        OC_LOG_DEBUG("[ExclusiveVisibilityStack] hide: {}", static_cast<int>(current_));

        // Restore previous (already visible if was stacked)
        current_ = previous_;
        previous_ = EnumT::NONE;
        bumpRevision();

        if (current_ != EnumT::NONE) {
            OC_LOG_DEBUG("[ExclusiveVisibilityStack] current now: {}", static_cast<int>(current_));
        }
    }

    /**
     * @brief Hide all items
     *
     * Calls cleanup callback for each visible item before hiding.
     */
    void hideAll() {
        bool changed = current_ != EnumT::NONE || previous_ != EnumT::NONE;
        for (size_t i = 1; i < COUNT; ++i) {
            changed = hideItem(static_cast<EnumT>(i)) || changed;
        }
        current_ = EnumT::NONE;
        previous_ = EnumT::NONE;
        if (changed) bumpRevision();
        OC_LOG_DEBUG("[ExclusiveVisibilityStack] hideAll");
    }

    /// Get currently active item (top of stack)
    EnumT current() const { return current_; }

    /// Check if any item is visible
    bool hasVisible() const { return current_ != EnumT::NONE; }

    Signal<uint32_t>& revisionSignal() { return revision_; }
    const Signal<uint32_t>& revisionSignal() const { return revision_; }

private:
    struct ItemBinding {
        void* object = nullptr;
        bool (*get)(void*) = nullptr;
        void (*set)(void*, bool) = nullptr;

        [[nodiscard]] bool valid() const {
            return object != nullptr && get != nullptr && set != nullptr;
        }
    };

    void clearCleanupCallback(uint32_t token) {
        if (token != 0 && token == cleanup_token_) {
            cleanupCallback_ = nullptr;
        }
    }

    void clearVisibilityTransitionCallback(uint32_t token) {
        if (token == 0 || token != visibility_transition_token_) return;
        visibility_transition_context_ = nullptr;
        visibility_transition_callback_ = nullptr;
    }

    void bumpRevision() {
        uint32_t next = revision_.get() + 1U;
        if (next == 0) next = 1;
        revision_.set(next);
    }

    [[nodiscard]] bool itemVisible(EnumT type) const {
        const auto idx = static_cast<size_t>(type);
        return idx < COUNT && items_[idx].valid() && items_[idx].get(items_[idx].object);
    }

    void setVisible(EnumT type, bool visible, bool forceTransition = false) {
        auto idx = static_cast<size_t>(type);
        if (idx >= COUNT || !items_[idx].valid()) return;

        const bool unchanged = items_[idx].get(items_[idx].object) == visible;
        if (unchanged && !forceTransition) return;

        if (visibility_transition_callback_) {
            visibility_transition_callback_(visibility_transition_context_, type, visible);
        }
        if (!unchanged) items_[idx].set(items_[idx].object, visible);
    }

    bool hideItem(EnumT type) {
        const auto idx = static_cast<size_t>(type);
        if (idx >= COUNT || !items_[idx].valid()) return false;

        const bool visible = items_[idx].get(items_[idx].object);
        const bool tracked = type == current_ || type == previous_;
        if (!visible && !tracked) return false;

        if (cleanupCallback_) cleanupCallback_(type);
        setVisible(type, false, true);
        return true;
    }

public:
    template <typename VisibleSignal>
    void registerItem(EnumT type, VisibleSignal& visible) {
        auto idx = static_cast<size_t>(type);
        if (idx < COUNT) {
            items_[idx] = ItemBinding{
                .object = static_cast<void*>(&visible),
                .get = [](void* object) -> bool {
                    return static_cast<VisibleSignal*>(object)->get();
                },
                .set = [](void* object, bool value) {
                    static_cast<VisibleSignal*>(object)->set(value);
                }
            };
        }
    }

private:
    std::array<ItemBinding, COUNT> items_{};
    EnumT current_ = EnumT::NONE;
    EnumT previous_ = EnumT::NONE;  // For single-level stacking
    CleanupCallback cleanupCallback_;  // Called before hiding
    uint32_t cleanup_token_ = 0;
    void* visibility_transition_context_ = nullptr;
    VisibilityTransitionCallback visibility_transition_callback_ = nullptr;
    uint32_t visibility_transition_token_ = 0;
    Signal<uint32_t> revision_{0};
};

}  // namespace oc::state
