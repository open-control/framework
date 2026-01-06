#pragma once

/**
 * @file AutoPersist.hpp
 * @brief Automatic persistence with debounced saves
 *
 * AutoPersist watches signals and automatically saves to Settings<T> when
 * they change. Saves are debounced to avoid excessive writes to storage.
 *
 * @code
 * Settings<MyData> settings{backend, 0, 1};
 * AutoPersist<MyData> persist{settings, 1000};  // 1s debounce
 *
 * persist.watch(signal1, [](MyData& data, int val) {
 *     data.field1 = val;
 * });
 *
 * // In main loop:
 * persist.update();  // Saves after 1s of no changes
 * @endcode
 */

#include <cstdint>
#include <functional>
#include <vector>

#include "Settings.hpp"
#include "Signal.hpp"

#include <oc/time/Time.hpp>

namespace oc::state {

/**
 * @brief Automatic persistence with debounced saves
 *
 * Watches signals and saves to Settings<T> when they change.
 * Saves are debounced to avoid excessive writes.
 *
 * @tparam T Settings data type (must be trivially copyable)
 */
template <typename T>
class AutoPersist {
public:
    /**
     * @brief Construct with settings and debounce delay
     * @param settings Settings container to persist to
     * @param debounce_ms Delay in milliseconds before saving (default 1000ms)
     */
    AutoPersist(Settings<T>& settings, uint32_t debounce_ms = 1000)
        : settings_(settings), debounce_ms_(debounce_ms) {}

    // Non-copyable, non-movable (holds subscriptions)
    AutoPersist(const AutoPersist&) = delete;
    AutoPersist& operator=(const AutoPersist&) = delete;
    AutoPersist(AutoPersist&&) = delete;
    AutoPersist& operator=(AutoPersist&&) = delete;

    ~AutoPersist() = default;

    /**
     * @brief Watch a signal and update settings when it changes
     *
     * When the signal changes, the updater function is called to modify
     * the settings data. The save is debounced.
     *
     * @tparam TSignal Signal type (e.g., Signal<int>)
     * @tparam Updater Function type: void(T& data, const TValue& value)
     * @param signal Signal to watch
     * @param updater Function to update settings data from signal value
     *
     * @code
     * persist.watch(volumeSignal, [](Settings& s, float vol) {
     *     s.volume = vol;
     * });
     * @endcode
     */
    template <typename TSignal, typename Updater>
    void watch(TSignal& signal, Updater&& updater) {
        subscriptions_.push_back(signal.subscribe(
            [this, updater = std::forward<Updater>(updater)](const auto& value) {
                settings_.modify([&](T& data) { updater(data, value); });
                markDirty();
            }));
    }

    /**
     * @brief Call from main loop to process debounced saves
     *
     * Checks if enough time has passed since the last change and saves
     * if the debounce period has elapsed.
     */
    void update() {
        if (dirty_timestamp_ == 0) return;

        uint32_t now = oc::time::millis();
        if ((now - dirty_timestamp_) < debounce_ms_) return;

        settings_.save();
        dirty_timestamp_ = 0;
    }

    /**
     * @brief Force immediate save
     *
     * Bypasses debounce and saves immediately if there are pending changes.
     * Useful for cleanup or context switches.
     */
    void flush() {
        if (dirty_timestamp_ != 0) {
            settings_.save();
            dirty_timestamp_ = 0;
        }
    }

    /// Check if there are unsaved changes pending
    [[nodiscard]] bool hasPendingChanges() const { return dirty_timestamp_ != 0; }

    /// Get the debounce delay in milliseconds
    [[nodiscard]] uint32_t debounceMs() const { return debounce_ms_; }

    /// Set the debounce delay in milliseconds
    void setDebounceMs(uint32_t ms) { debounce_ms_ = ms; }

private:
    void markDirty() {
        if (dirty_timestamp_ == 0) {
            dirty_timestamp_ = oc::time::millis();
            // Handle edge case where millis() returns 0
            if (dirty_timestamp_ == 0) {
                dirty_timestamp_ = 1;
            }
        }
    }

    Settings<T>& settings_;
    uint32_t debounce_ms_;
    uint32_t dirty_timestamp_ = 0;
    std::vector<Subscription> subscriptions_;
};

}  // namespace oc::state
