#pragma once

/**
 * @file AutoPersistIncremental.hpp
 * @brief Incremental persistence with per-item dirty tracking
 *
 * Unlike AutoPersist which saves all data at once, AutoPersistIncremental
 * tracks which specific items changed and only saves those.
 *
 * Ideal for:
 * - EEPROM where write cycles are limited
 * - Large data structures where only parts change
 * - Custom storage layouts with per-field save functions
 *
 * @code
 * AutoPersistIncremental<8> persist{
 *     [this](uint8_t i) { settings.saveValue(activePage, i, macros[i].value.get()); },
 *     [this]() { settings.commit(); },
 *     1000  // 1s debounce
 * };
 *
 * for (uint8_t i = 0; i < 8; ++i) {
 *     persist.watchAt(i, macros[i].value);
 * }
 *
 * // In main loop:
 * persist.update();  // Saves only changed items after debounce
 * @endcode
 */

#include <cstdint>
#include <functional>
#include <vector>

#include "Signal.hpp"

#include <oc/time/Time.hpp>

namespace oc::state {

/**
 * @brief Incremental persistence with per-item dirty tracking
 *
 * Watches signals at specific indices and saves only changed items.
 *
 * @tparam MaxItems Maximum number of tracked items (affects bitfield size)
 */
template <size_t MaxItems = 8>
class AutoPersistIncremental {
    static_assert(MaxItems <= 64, "MaxItems must be <= 64 for bitfield");

public:
    /// Callback to save a specific item by index
    using SaveCallback = std::function<void(uint8_t index)>;

    /// Callback to commit all pending writes
    using CommitCallback = std::function<void()>;

    /**
     * @brief Construct with save callbacks and debounce delay
     * @param on_save Called for each dirty index during save
     * @param on_commit Called once after all dirty items are saved
     * @param debounce_ms Delay in milliseconds before saving (default 300ms)
     */
    AutoPersistIncremental(SaveCallback on_save, CommitCallback on_commit,
                           uint32_t debounce_ms = 300)
        : on_save_(std::move(on_save))
        , on_commit_(std::move(on_commit))
        , debounce_ms_(debounce_ms) {}

    // Non-copyable, non-movable (holds subscriptions)
    AutoPersistIncremental(const AutoPersistIncremental&) = delete;
    AutoPersistIncremental& operator=(const AutoPersistIncremental&) = delete;
    AutoPersistIncremental(AutoPersistIncremental&&) = delete;
    AutoPersistIncremental& operator=(AutoPersistIncremental&&) = delete;

    ~AutoPersistIncremental() = default;

    /**
     * @brief Watch a signal at a specific index
     *
     * When the signal changes, the index is marked dirty.
     * Only that index will be saved when the debounce timer expires.
     *
     * @tparam TSignal Signal type (e.g., Signal<float>)
     * @param index Item index (0 to MaxItems-1)
     * @param signal Signal to watch
     */
    template <typename TSignal>
    void watchAt(uint8_t index, TSignal& signal) {
        if (index >= MaxItems) return;

        subscriptions_.push_back(signal.subscribe([this, index](const auto&) {
            markDirty(index);
        }));
    }

    /**
     * @brief Call from main loop to process debounced saves
     *
     * Checks if enough time has passed since the first change and saves
     * all dirty items if the debounce period has elapsed.
     */
    void update() {
        if (dirty_mask_ == 0) return;

        uint32_t now = oc::time::millis();
        if ((now - dirty_timestamp_) < debounce_ms_) return;

        saveDirtyItems();
    }

    /**
     * @brief Force immediate save of all dirty items
     *
     * Bypasses debounce and saves immediately.
     * Useful for cleanup or context switches.
     */
    void flush() {
        if (dirty_mask_ != 0) {
            saveDirtyItems();
        }
    }

    /// Check if there are unsaved changes pending
    [[nodiscard]] bool hasPendingChanges() const { return dirty_mask_ != 0; }

    /// Get the dirty mask (bitfield of changed indices)
    [[nodiscard]] uint64_t dirtyMask() const { return dirty_mask_; }

    /// Check if a specific index is dirty
    [[nodiscard]] bool isDirty(uint8_t index) const {
        if (index >= MaxItems) return false;
        return (dirty_mask_ & (1ULL << index)) != 0;
    }

    /// Get the debounce delay in milliseconds
    [[nodiscard]] uint32_t debounceMs() const { return debounce_ms_; }

    /// Set the debounce delay in milliseconds
    void setDebounceMs(uint32_t ms) { debounce_ms_ = ms; }

private:
    void markDirty(uint8_t index) {
        dirty_mask_ |= (1ULL << index);
        if (dirty_timestamp_ == 0) {
            dirty_timestamp_ = oc::time::millis();
            // Handle edge case where millis() returns 0
            if (dirty_timestamp_ == 0) {
                dirty_timestamp_ = 1;
            }
        }
    }

    void saveDirtyItems() {
        // Save each dirty item
        for (uint8_t i = 0; i < MaxItems; ++i) {
            if (dirty_mask_ & (1ULL << i)) {
                on_save_(i);
            }
        }

        // Commit all writes
        on_commit_();

        // Reset dirty state
        dirty_mask_ = 0;
        dirty_timestamp_ = 0;
    }

    SaveCallback on_save_;
    CommitCallback on_commit_;
    uint32_t debounce_ms_;
    uint64_t dirty_mask_ = 0;
    uint32_t dirty_timestamp_ = 0;
    std::vector<Subscription> subscriptions_;
};

}  // namespace oc::state
