#pragma once

/**
 * @file ChangeCoalescer.hpp
 * @brief Coalesce a bounded group of changes into one deferred action
 */

#include <cstddef>
#include <cstdint>
#include <functional>
#include <utility>

#include <oc/time/Time.hpp>

#include "FixedSubscriptionList.hpp"
#include "Signal.hpp"

namespace oc::state {

/**
 * Groups signal notifications and explicit change marks into one action.
 *
 * The coalescing window starts with the first pending change. Later changes
 * join that window without extending it, so a continuous edit cannot postpone
 * the action indefinitely. Signal subscriptions use fixed storage.
 *
 * @tparam MaxSubscriptions Maximum number of signals watched by this instance
 */
template <size_t MaxSubscriptions = 0>
class ChangeCoalescer {
public:
    using Action = std::function<void()>;

    explicit ChangeCoalescer(Action action, uint32_t coalesceMs = 300)
        : action_(std::move(action))
        , coalesceMs_(coalesceMs) {}

    ChangeCoalescer(const ChangeCoalescer&) = delete;
    ChangeCoalescer& operator=(const ChangeCoalescer&) = delete;
    ChangeCoalescer(ChangeCoalescer&&) = delete;
    ChangeCoalescer& operator=(ChangeCoalescer&&) = delete;

    template <typename TSignal>
    bool watch(TSignal& signal) {
        if (!valid_ || subscriptions_.full()) {
            valid_ = false;
            return false;
        }

        valid_ = subscriptions_.tryAdd(signal.subscribe(
            [this](const auto&) { markChanged(); }
        ));
        return valid_;
    }

    void markChanged() {
        if (pending_) return;
        pending_ = true;
        firstChangeMs_ = oc::time::millis();
    }

    void update() {
        if (!pending_) return;

        const uint32_t now = oc::time::millis();
        if ((now - firstChangeMs_) < coalesceMs_) return;
        runPendingAction_();
    }

    void flush() {
        if (pending_) {
            runPendingAction_();
        }
    }

    /**
     * Consume this coalescer's deferred signal callbacks and pending mark
     * without invoking its action.
     *
     * A caller that has already performed the action atomically can use this
     * to prevent the same watched changes from publishing again. Other
     * NotificationQueue subscribers are untouched.
     */
    void consumePendingChangesWithoutAction() {
        subscriptions_.cancelPendingNotifications();
        pending_ = false;
        firstChangeMs_ = 0;
    }

    [[nodiscard]] bool hasPendingChanges() const { return pending_; }
    [[nodiscard]] uint32_t coalesceMs() const { return coalesceMs_; }
    [[nodiscard]] size_t subscriptionCount() const { return subscriptions_.size(); }
    [[nodiscard]] bool valid() const { return valid_; }

    void setCoalesceMs(uint32_t value) { coalesceMs_ = value; }

private:
    void runPendingAction_() {
        // Clear first so changes raised by the action start a new window.
        pending_ = false;
        firstChangeMs_ = 0;
        if (action_) {
            action_();
        }
    }

    Action action_;
    uint32_t coalesceMs_ = 300;
    uint32_t firstChangeMs_ = 0;
    bool pending_ = false;
    bool valid_ = true;
    FixedSubscriptionList<MaxSubscriptions> subscriptions_;
};

}  // namespace oc::state
