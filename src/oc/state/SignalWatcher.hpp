#pragma once

/**
 * @file SignalWatcher.hpp
 * @brief Coalesced signal watching - multiple signals, one callback
 *
 * SignalWatcher allows subscribing to multiple signals with a single callback
 * that is guaranteed to be called only once per tick, regardless of how many
 * watched signals change.
 *
 * ## Problem Solved
 *
 * When multiple related signals change together (e.g., device name, type,
 * and enabled state), the update callback would normally be called N times.
 * SignalWatcher coalesces these into a single call.
 *
 * ## Usage
 *
 * @code
 * class DeviceView {
 *     SignalWatcher watcher_;
 *
 *     void setupBindings(BitwigState& state) {
 *         // Simple case: watch multiple signals
 *         watcher_.watchAll(
 *             [this]() { updateDeviceInfo(); },
 *             state.device.name,
 *             state.device.type,
 *             state.device.enabled
 *         );
 *
 *         // With arrays: use group() + watch()
 *         auto& selectorGroup = watcher_.group([this]() { updateDeviceSelector(); });
 *         selectorGroup.watch(state.deviceSelector.names);
 *         selectorGroup.watch(state.deviceSelector.visible);
 *         for (auto& s : state.deviceSelector.deviceStates) {
 *             selectorGroup.watch(s);
 *         }
 *     }
 * };
 * @endcode
 */

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <config/PlatformCompat.hpp>
#include "NotificationQueue.hpp"
#include "Signal.hpp"
#include "SignalString.hpp"
#include "SignalVector.hpp"

namespace oc::state {

/**
 * @brief A group of signals that share a single coalesced callback
 */
class WatchGroup {
    struct NotificationThunk {
        WatchGroup* group = nullptr;

        void operator()() const {
            group->enqueue();
        }

        template <typename T>
        void operator()(const T&) const {
            group->enqueue();
        }
    };

public:
    WatchGroup(void* owner, size_t index, std::function<void()> callback, const char* debugLabel = nullptr)
        : key_(owner, index), callback_(std::move(callback)), debug_label_(debugLabel) {}

    // Non-copyable, movable
    WatchGroup(const WatchGroup&) = delete;
    WatchGroup& operator=(const WatchGroup&) = delete;
    WatchGroup(WatchGroup&&) = default;
    WatchGroup& operator=(WatchGroup&&) = default;

    /**
     * @brief Add any subscribable signal-like object to this watch group
     *
     * Supported objects must expose:
     * `Subscription subscribe(CallbackLike)`
     *
     * The callback may be either:
     * - `void()`
     * - `void(const T&)`
     * - `void(const char*)`
     *
     * This keeps SignalWatcher compatible with both fixed-capacity and
     * adaptive signal families without duplicating overloads.
     */
    template <typename SignalLike>
        auto watch(SignalLike& signal)
        -> decltype(signal.subscribe(std::declval<NotificationThunk>()), std::declval<WatchGroup&>()) {
        detail::ScopedSubscriptionDebugContext context(debug_label_);
        subscriptions_.push_back(signal.subscribe(NotificationThunk{this}));
        return *this;
    }

    void setDebugLabel(const char* label) { debug_label_ = label; }
    [[nodiscard]] const char* debugLabel() const { return debug_label_; }
    [[nodiscard]] size_t subscriptionCount() const { return subscriptions_.size(); }

private:
    void enqueue() {
        NotificationQueue::instance().enqueue(
            key_,
            static_cast<void*>(this),
            [](void* context, size_t) {
                static_cast<WatchGroup*>(context)->callback_();
            });
    }

    NotificationQueue::Key key_;
    std::function<void()> callback_;
    std::vector<Subscription> subscriptions_;
    const char* debug_label_ = nullptr;
};

/**
 * @brief Watches multiple signals and coalesces notifications
 *
 * Lifetime: SignalWatcher must outlive the signals it watches.
 * When destroyed, all subscriptions are automatically cleaned up.
 */
class SignalWatcher {
public:
    SignalWatcher() = default;
    ~SignalWatcher() = default;

    // Non-copyable, non-movable (subscriptions hold pointers)
    SignalWatcher(const SignalWatcher&) = delete;
    SignalWatcher& operator=(const SignalWatcher&) = delete;
    SignalWatcher(SignalWatcher&&) = delete;
    SignalWatcher& operator=(SignalWatcher&&) = delete;

    /**
     * @brief Create a new watch group with a callback
     *
     * Use this when you need to add signals incrementally (e.g., from arrays).
     *
     * @param callback Function to call when any signal in group changes
     * @return Reference to the created group for adding signals
     *
     * @code
     * auto& g = watcher.group([this]() { update(); });
     * g.watch(signal1).watch(signal2);
     * for (auto& s : signals) { g.watch(s); }
     * @endcode
     */
    WatchGroup& group(std::function<void()> callback) {
        return group(nullptr, std::move(callback));
    }

    WatchGroup& group(const char* debugLabel, std::function<void()> callback) {
        size_t index = groups_.size();
        groups_.push_back(
            std::make_unique<WatchGroup>(
                static_cast<void*>(this), index, std::move(callback), debugLabel));
        return *groups_.back();
    }

    /**
     * @brief Watch multiple signals with a single coalesced callback (convenience)
     *
     * @tparam Callback Callable type
     * @tparam Signals Signal types to watch
     * @param callback Function to call when any signal changes
     * @param signals Signals to watch (variadic)
     * @return Reference to this for chaining
     *
     * @code
     * watcher.watchAll([this]() { refresh(); }, signal1, signal2, signal3);
     * @endcode
     */
    template <typename Callback, typename... Signals>
    SignalWatcher& watchAll(Callback&& callback, Signals&... signals) {
        auto& g = group(std::forward<Callback>(callback));
        (g.watch(signals), ...);  // Fold expression
        return *this;
    }

    template <typename Callback, typename... Signals>
    SignalWatcher& watchAll(const char* debugLabel, Callback&& callback, Signals&... signals) {
        auto& g = group(debugLabel, std::forward<Callback>(callback));
        (g.watch(signals), ...);
        return *this;
    }

    [[nodiscard]] size_t groupCount() const { return groups_.size(); }

    [[nodiscard]] size_t subscriptionCount() const {
        size_t count = 0;
        for (const auto& g : groups_) {
            count += g->subscriptionCount();
        }
        return count;
    }

private:
    std::vector<std::unique_ptr<WatchGroup>> groups_;
};

}  // namespace oc::state
