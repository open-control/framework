#pragma once

/**
 * @file OverlayManager.hpp
 * @brief Template overlay manager with single-level stacking
 *
 * Manages overlay visibility with support for one stacked overlay.
 * When stacking, the previous overlay remains visible underneath.
 *
 * @tparam EnumT Enum type for overlay identifiers. Must have:
 *         - NONE as first value (0)
 *         - COUNT as last value
 */

#include <oc/log/Log.hpp>
#include <oc/state/Signal.hpp>

#include <array>
#include <cstdint>

namespace oc::state {

/**
 * @brief Centralized manager for overlay visibility
 *
 * Supports one level of stacking (e.g., TrackSelector on top of DeviceSelector).
 * When stacking, both overlays are visible; the top one has input priority.
 *
 * @tparam EnumT Overlay enum type with NONE and COUNT values
 *
 * Usage:
 * @code
 * enum class OverlayType : uint8_t { NONE = 0, PAGE_SELECTOR, DEVICE_SELECTOR, COUNT };
 * OverlayManager<OverlayType> overlays;
 *
 * overlays.registerOverlay(OverlayType::DEVICE_SELECTOR, deviceVisible);
 * overlays.show(OverlayType::DEVICE_SELECTOR);              // Shows device
 * overlays.show(OverlayType::PAGE_SELECTOR, true);          // Page on top, device stays visible
 * overlays.hide();                                          // Hides page, device is current
 * overlays.hide();                                          // Hides device
 * @endcode
 */
template <typename EnumT>
class OverlayManager {
    static_assert(static_cast<int>(EnumT::NONE) == 0, "EnumT::NONE must be 0");
    static constexpr size_t COUNT = static_cast<size_t>(EnumT::COUNT);

public:
    OverlayManager() = default;

    // Non-copyable, non-movable (holds pointers to signals)
    OverlayManager(const OverlayManager&) = delete;
    OverlayManager& operator=(const OverlayManager&) = delete;
    OverlayManager(OverlayManager&&) = delete;
    OverlayManager& operator=(OverlayManager&&) = delete;

    /**
     * @brief Register an overlay's visible signal
     * @param type Overlay identifier
     * @param visible Signal controlling this overlay's visibility
     */
    void registerOverlay(EnumT type, Signal<bool>& visible) {
        auto idx = static_cast<size_t>(type);
        if (idx < COUNT) {
            overlays_[idx] = &visible;
        }
    }

    /**
     * @brief Show an overlay
     * @param type The overlay to show
     * @param stack If true, current overlay stays visible underneath
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
        OC_LOG_DEBUG("[OverlayManager] show: {} (stack: {})", static_cast<int>(type), stack);
    }

    /**
     * @brief Hide current overlay, restore previous if stacked
     */
    void hide() {
        if (current_ == EnumT::NONE) return;

        setVisible(current_, false);
        OC_LOG_DEBUG("[OverlayManager] hide: {}", static_cast<int>(current_));

        // Restore previous (already visible if was stacked)
        current_ = previous_;
        previous_ = EnumT::NONE;

        if (current_ != EnumT::NONE) {
            OC_LOG_DEBUG("[OverlayManager] current now: {}", static_cast<int>(current_));
        }
    }

    /**
     * @brief Hide all overlays
     */
    void hideAll() {
        for (size_t i = 1; i < COUNT; ++i) {
            if (overlays_[i] != nullptr) {
                overlays_[i]->set(false);
            }
        }
        current_ = EnumT::NONE;
        previous_ = EnumT::NONE;
        OC_LOG_DEBUG("[OverlayManager] hideAll");
    }

    /// Get currently active overlay (top of stack)
    EnumT current() const { return current_; }

    /// Check if any overlay is visible
    bool hasVisibleOverlay() const { return current_ != EnumT::NONE; }

private:
    void setVisible(EnumT type, bool visible) {
        auto idx = static_cast<size_t>(type);
        if (idx < COUNT && overlays_[idx] != nullptr) {
            overlays_[idx]->set(visible);
        }
    }

    std::array<Signal<bool>*, COUNT> overlays_{};
    EnumT current_ = EnumT::NONE;
    EnumT previous_ = EnumT::NONE;  // For single-level stacking
};

}  // namespace oc::state
