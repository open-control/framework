#pragma once

#include <oc/state/Signal.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>

namespace oc::state {

/**
 * @brief Zero-allocation reactive vector with fixed capacity
 *
 * SignalVector provides a dynamic-size collection with a fixed maximum capacity,
 * combined with reactive notification on any structural change. Ideal for embedded
 * systems where predictable memory usage is required.
 *
 * ## Design Rationale
 *
 * Unlike `Signal<std::vector<T>>` which allocates on every change, SignalVector:
 * - Uses a **fixed buffer** (no heap allocation after construction)
 * - Notifies on **any structural change** (set, clear, resize)
 * - Provides **whole-list notification** (not per-element)
 * - Supports iteration via begin()/end()
 *
 * ## Notification Model
 *
 * SignalVector uses **whole-list notification**: subscribers are notified whenever
 * the collection structure changes, not for individual element modifications.
 * This is appropriate for lists that are typically replaced entirely (e.g., DAW
 * track lists, device lists, page names).
 *
 * For per-element reactivity, use `std::array<Signal<T>, N>` instead.
 *
 * ## Usage
 *
 * @code
 * // In state struct
 * struct DeviceState {
 *     SignalVector<std::string, 32> deviceNames;
 *     Signal<uint8_t> selectedIndex{0};
 * };
 *
 * // Handler updates state
 * void onDeviceListReceived(const std::vector<std::string>& names) {
 *     state_.deviceNames.set(names.data(), names.size());
 * }
 *
 * // View subscribes
 * sub_ = state_.deviceNames.subscribe([this]() {
 *     rebuildList();  // Called when list structure changes
 * });
 * @endcode
 *
 * @tparam T Element type (must be default-constructible and copy-assignable)
 * @tparam MaxN Maximum number of elements
 * @tparam MaxSubs Maximum concurrent subscribers (default: 4)
 *
 * @see Signal for scalar value observation
 * @see SignalString for string observation
 */
template <typename T, size_t MaxN, size_t MaxSubs = 4>
class SignalVector {
public:
    // =========================================================================
    // Construction
    // =========================================================================

    /// Construct empty vector
    SignalVector() = default;

    // Non-copyable, non-movable (subscribers hold pointers)
    SignalVector(const SignalVector&) = delete;
    SignalVector& operator=(const SignalVector&) = delete;
    SignalVector(SignalVector&&) = delete;
    SignalVector& operator=(SignalVector&&) = delete;

    ~SignalVector() = default;

    // =========================================================================
    // Modifiers
    // =========================================================================

    /**
     * @brief Replace all elements from array
     *
     * Copies up to MaxN elements from the source array. If count > MaxN,
     * only the first MaxN elements are copied.
     *
     * Notifies subscribers if the new content differs from current.
     *
     * @param items Source array
     * @param count Number of elements to copy
     */
    void set(const T* items, size_t count) {
        size_t newCount = (count > MaxN) ? MaxN : count;

        // Check if content changed
        bool changed = (newCount != count_);
        if (!changed) {
            for (size_t i = 0; i < newCount && !changed; ++i) {
                if (!(items_[i] == items[i])) {
                    changed = true;
                }
            }
        }

        if (!changed) return;

        // Copy new content
        for (size_t i = 0; i < newCount; ++i) {
            items_[i] = items[i];
        }
        count_ = static_cast<uint8_t>(newCount);

        notify();
    }

    /**
     * @brief Replace all elements from initializer list
     *
     * @param items Initializer list of elements
     */
    void set(std::initializer_list<T> items) {
        set(items.begin(), items.size());
    }

    /**
     * @brief Clear all elements
     *
     * Sets count to 0. Notifies subscribers if not already empty.
     */
    void clear() {
        if (count_ == 0) return;
        count_ = 0;
        notify();
    }

    /**
     * @brief Force notification without changing content
     *
     * Useful for initial synchronization after subscribing.
     */
    void notify() { revision_.set(revision_.get() + 1); }

    // =========================================================================
    // Accessors
    // =========================================================================

    /**
     * @brief Get element at index (unchecked)
     * @param i Index (must be < size())
     * @return Const reference to element
     */
    [[nodiscard]] const T& operator[](size_t i) const { return items_[i]; }

    /**
     * @brief Get pointer to underlying array
     * @return Pointer to first element
     */
    [[nodiscard]] const T* data() const { return items_.data(); }

    /**
     * @brief Get current number of elements
     */
    [[nodiscard]] size_t size() const { return count_; }

    /**
     * @brief Check if vector is empty
     */
    [[nodiscard]] bool empty() const { return count_ == 0; }

    // =========================================================================
    // Iteration
    // =========================================================================

    /// Iterator to first element
    [[nodiscard]] const T* begin() const { return items_.data(); }

    /// Iterator past last element
    [[nodiscard]] const T* end() const { return items_.data() + count_; }

    // =========================================================================
    // Subscription
    // =========================================================================

    /**
     * @brief Subscribe to collection changes
     *
     * Callback is invoked whenever the collection structure changes
     * (set, clear). Callback receives no parameters - query the
     * vector directly for current state.
     *
     * @param callback Function called on changes (no parameters)
     * @return Subscription RAII handle (unsubscribes on destruction)
     *
     * @code
     * auto sub = names.subscribe([this]() {
     *     rebuildListUI();
     * });
     * @endcode
     */
    [[nodiscard]] Subscription subscribe(std::function<void()> callback) {
        return revision_.subscribe([cb = std::move(callback)](uint8_t) {
            if (cb) cb();
        });
    }

    /**
     * @brief Subscribe and immediately invoke callback
     *
     * Useful for initializing UI state on subscription.
     *
     * @param callback Function called immediately and on future changes
     * @return Subscription RAII handle
     */
    [[nodiscard]] Subscription subscribeAndInvoke(std::function<void()> callback) {
        if (callback) {
            callback();
        }
        return subscribe(std::move(callback));
    }

    // =========================================================================
    // Capacity
    // =========================================================================

    /// Maximum number of elements
    [[nodiscard]] static constexpr size_t maxSize() { return MaxN; }

    /// Current number of active subscribers
    [[nodiscard]] size_t subscriberCount() const { return revision_.subscriberCount(); }

    /// Maximum subscribers allowed
    [[nodiscard]] static constexpr size_t maxSubscribers() { return MaxSubs; }

private:
    std::array<T, MaxN> items_{};
    uint8_t count_ = 0;
    Signal<uint8_t, MaxSubs> revision_{0};
};

}  // namespace oc::state
