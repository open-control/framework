#pragma once

#include <array>
#include <cstddef>
#include <type_traits>

#include <oc/log/Log.hpp>

#include "NotificationQueue.hpp"
#include "Signal.hpp"

namespace oc::state {

template <size_t MaxSignals>
class StaticWatchGroup {
    struct NotificationThunk {
        StaticWatchGroup* group = nullptr;

        void operator()() const {
            group->enqueue();
        }

        template <typename T>
        void operator()(const T&) const {
            group->enqueue();
        }
    };

public:
    StaticWatchGroup() = default;

    StaticWatchGroup(const StaticWatchGroup&) = delete;
    StaticWatchGroup& operator=(const StaticWatchGroup&) = delete;

    template <auto Callback, typename Owner>
    void bind(Owner& owner, size_t keyIndex, const char* debugLabel = nullptr) {
        static_assert(std::is_member_function_pointer_v<decltype(Callback)>,
                      "Callback must be a member function pointer");

        clear();
        owner_ = static_cast<void*>(&owner);
        key_ = NotificationQueue::Key(static_cast<void*>(this), keyIndex);
        debug_label_ = debugLabel;
        callback_ = [](void* context) {
            (static_cast<Owner*>(context)->*Callback)();
        };
    }

    template <typename SignalLike>
    bool watch(SignalLike& signal) {
        if (callback_ == nullptr) {
            OC_LOG_ERROR("[StaticWatchGroup] watch() called before bind label={}",
                         debug_label_ ? debug_label_ : "<unnamed>");
            return false;
        }

        if (subscription_count_ >= MaxSignals) {
            OC_LOG_ERROR("[StaticWatchGroup] capacity exceeded label={} subscriptions={} max={}",
                         debug_label_ ? debug_label_ : "<unnamed>",
                         subscription_count_,
                         MaxSignals);
            return false;
        }

        detail::ScopedSubscriptionDebugContext context(debug_label_);
        auto subscription = signal.subscribe(NotificationThunk{this});
        if (!subscription) {
            return false;
        }

        subscriptions_[subscription_count_++] = std::move(subscription);
        return true;
    }

    template <typename... Signals>
    bool watchAll(Signals&... signals) {
        bool ok = true;
        ((ok = watch(signals) && ok), ...);
        return ok;
    }

    void clear() {
        for (size_t i = 0; i < subscription_count_; ++i) {
            subscriptions_[i].reset();
        }
        subscription_count_ = 0;
    }

    [[nodiscard]] size_t subscriptionCount() const { return subscription_count_; }
    [[nodiscard]] static constexpr size_t capacity() { return MaxSignals; }
    [[nodiscard]] const char* debugLabel() const { return debug_label_; }

private:
    void enqueue() {
        NotificationQueue::instance().enqueue(
            key_,
            static_cast<void*>(this),
            [](void* context, size_t) {
                auto* self = static_cast<StaticWatchGroup*>(context);
                if (self->callback_ != nullptr) {
                    self->callback_(self->owner_);
                }
            });
    }

    void* owner_ = nullptr;
    void (*callback_)(void*) = nullptr;
    NotificationQueue::Key key_{nullptr, 0};
    std::array<Subscription, MaxSignals> subscriptions_{};
    size_t subscription_count_ = 0;
    const char* debug_label_ = nullptr;
};

template <size_t MaxGroups, size_t MaxSignalsPerGroup>
class StaticSignalWatcher {
public:
    StaticSignalWatcher() = default;

    StaticSignalWatcher(const StaticSignalWatcher&) = delete;
    StaticSignalWatcher& operator=(const StaticSignalWatcher&) = delete;

    template <auto Callback, typename Owner>
    StaticWatchGroup<MaxSignalsPerGroup>* tryGroup(Owner& owner, const char* debugLabel = nullptr) {
        if (group_count_ >= MaxGroups) {
            OC_LOG_ERROR("[StaticSignalWatcher] group capacity exceeded label={} groups={} max={}",
                         debugLabel ? debugLabel : "<unnamed>",
                         group_count_,
                         MaxGroups);
            return nullptr;
        }

        auto& group = groups_[group_count_];
        group.template bind<Callback>(owner, group_count_, debugLabel);
        ++group_count_;
        return &group;
    }

    template <auto Callback, typename Owner, typename... Signals>
    bool watchAll(Owner& owner, const char* debugLabel, Signals&... signals) {
        auto* group = tryGroup<Callback>(owner, debugLabel);
        return group != nullptr && group->watchAll(signals...);
    }

    void clear() {
        for (size_t i = 0; i < group_count_; ++i) {
            groups_[i].clear();
        }
        group_count_ = 0;
    }

    [[nodiscard]] size_t groupCount() const { return group_count_; }

    [[nodiscard]] size_t subscriptionCount() const {
        size_t count = 0;
        for (size_t i = 0; i < group_count_; ++i) {
            count += groups_[i].subscriptionCount();
        }
        return count;
    }

private:
    std::array<StaticWatchGroup<MaxSignalsPerGroup>, MaxGroups> groups_{};
    size_t group_count_ = 0;
};

}  // namespace oc::state
