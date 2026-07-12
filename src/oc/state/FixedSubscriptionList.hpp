#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <utility>

#include "Signal.hpp"

namespace oc::state {

/**
 * Owns a compile-time bounded set of signal subscriptions.
 *
 * Keeping handles inline avoids heap traffic in retained UI objects whose
 * signal topology is known when they are constructed.
 */
template <size_t Capacity>
class FixedSubscriptionList {
public:
    FixedSubscriptionList() = default;

    FixedSubscriptionList(const FixedSubscriptionList&) = delete;
    FixedSubscriptionList& operator=(const FixedSubscriptionList&) = delete;
    FixedSubscriptionList(FixedSubscriptionList&&) = delete;
    FixedSubscriptionList& operator=(FixedSubscriptionList&&) = delete;

    bool tryAdd(Subscription subscription) {
        if (!subscription || size_ >= Capacity) return false;
        subscriptions_[size_++] = std::move(subscription);
        return true;
    }

    void add(Subscription subscription) {
        const bool added = tryAdd(std::move(subscription));
        assert(added && "FixedSubscriptionList capacity exceeded or invalid subscription");
        (void)added;
    }

    void push_back(Subscription subscription) {
        add(std::move(subscription));
    }

    void clear() {
        for (size_t i = 0; i < size_; ++i) {
            subscriptions_[i].reset();
        }
        size_ = 0;
    }

    [[nodiscard]] size_t size() const { return size_; }
    [[nodiscard]] bool empty() const { return size_ == 0; }
    [[nodiscard]] bool full() const { return size_ == Capacity; }
    [[nodiscard]] static constexpr size_t capacity() { return Capacity; }

private:
    std::array<Subscription, Capacity> subscriptions_{};
    size_t size_ = 0;
};

/**
 * Fixed subscription storage for fallible construction batches.
 *
 * push_back() records an invalid handle or capacity overflow instead of
 * asserting. Call valid() after wiring the batch and propagate failure to the
 * owning component's initialization result.
 */
template <size_t Capacity>
class CheckedSubscriptionList {
public:
    CheckedSubscriptionList() = default;

    CheckedSubscriptionList(const CheckedSubscriptionList&) = delete;
    CheckedSubscriptionList& operator=(const CheckedSubscriptionList&) = delete;
    CheckedSubscriptionList(CheckedSubscriptionList&&) = delete;
    CheckedSubscriptionList& operator=(CheckedSubscriptionList&&) = delete;

    void push_back(Subscription subscription) {
        valid_ = subscriptions_.tryAdd(std::move(subscription)) && valid_;
    }

    void clear() {
        subscriptions_.clear();
        valid_ = true;
    }

    [[nodiscard]] bool valid() const { return valid_; }
    [[nodiscard]] size_t size() const { return subscriptions_.size(); }
    [[nodiscard]] bool empty() const { return subscriptions_.empty(); }
    [[nodiscard]] static constexpr size_t capacity() { return Capacity; }

private:
    FixedSubscriptionList<Capacity> subscriptions_{};
    bool valid_ = true;
};

}  // namespace oc::state
