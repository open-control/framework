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

namespace oc::state {

/**
 * @brief Manages exclusive visibility of items with single-level stacking
 *
 * Generic visibility stack supporting one level of stacking.
 * When stacking, both items are visible; the top one has priority.
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

        if (stack && current_ != EnumT::NONE && current_ != type) {
            // Stacking: current stays visible, becomes previous
            previous_ = current_;
        } else if (current_ != EnumT::NONE) {
            // Replacing: hide current
            setVisible(current_, false);
            previous_ = EnumT::NONE;
        }

        setVisible(type, true);
        current_ = type;
        OC_LOG_DEBUG("[ExclusiveVisibilityStack] show: {} (stack: {})", static_cast<int>(type), stack);
    }

    /**
     * @brief Hide current item, restore previous if stacked
     *
     * Calls cleanup callback (if set) before hiding.
     */
    void hide() {
        if (current_ == EnumT::NONE) return;

        // Call cleanup callback before hiding
        if (cleanupCallback_) {
            cleanupCallback_(current_);
        }

        setVisible(current_, false);
        OC_LOG_DEBUG("[ExclusiveVisibilityStack] hide: {}", static_cast<int>(current_));

        // Restore previous (already visible if was stacked)
        current_ = previous_;
        previous_ = EnumT::NONE;

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
        // Call cleanup for all potentially visible items
        if (cleanupCallback_) {
            for (size_t i = 1; i < COUNT; ++i) {
                if (items_[i].valid() && items_[i].get(items_[i].object)) {
                    cleanupCallback_(static_cast<EnumT>(i));
                }
            }
        }

        for (size_t i = 1; i < COUNT; ++i) {
            if (items_[i].valid()) {
                items_[i].set(items_[i].object, false);
            }
        }
        current_ = EnumT::NONE;
        previous_ = EnumT::NONE;
        OC_LOG_DEBUG("[ExclusiveVisibilityStack] hideAll");
    }

    /// Get currently active item (top of stack)
    EnumT current() const { return current_; }

    /// Check if any item is visible
    bool hasVisible() const { return current_ != EnumT::NONE; }

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

    void setVisible(EnumT type, bool visible) {
        auto idx = static_cast<size_t>(type);
        if (idx < COUNT && items_[idx].valid()) {
            items_[idx].set(items_[idx].object, visible);
        }
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
};

}  // namespace oc::state
