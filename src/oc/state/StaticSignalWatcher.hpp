#pragma once

#include <array>
#include <cstddef>
#include <type_traits>

#include <config/PlatformCompat.hpp>
#include <oc/diagnostics/Performance.hpp>
#include <oc/log/Log.hpp>

#include "NotificationQueue.hpp"
#include "FixedSubscriptionList.hpp"
#include "Signal.hpp"

namespace oc::state {

class StaticWatchGroupBase {
public:
    StaticWatchGroupBase() = default;
    ~StaticWatchGroupBase() {
        cancelPending();
    }

    StaticWatchGroupBase(const StaticWatchGroupBase&) = delete;
    StaticWatchGroupBase& operator=(const StaticWatchGroupBase&) = delete;

    void bindRaw(void* owner,
                 void (*callback)(void*),
                 size_t keyIndex,
                 const char* debugLabel) {
        cancelPending();
        owner_ = owner;
        callback_ = callback;
        key_ = NotificationQueue::Key(static_cast<void*>(this), keyIndex);
#if OC_ENABLE_STATS
        debug_label_ = debugLabel;
#else
        (void)debugLabel;
#endif
    }

    void enqueue() {
        NotificationQueue::instance().enqueue(
            key_,
            static_cast<void*>(this),
            [](void* context, size_t) {
                auto* self = static_cast<StaticWatchGroupBase*>(context);
                if (self->callback_ != nullptr) {
                    self->callback_(self->owner_);
                }
            }
#if OC_ENABLE_STATS
            , debug_label_ ? debug_label_ : "static-watch-group.callback"
#endif
        );
    }

    [[nodiscard]] const char* debugLabel() const {
#if OC_ENABLE_STATS
        return debug_label_;
#else
        return nullptr;
#endif
    }
    [[nodiscard]] bool bound() const { return callback_ != nullptr; }

    void cancelPending() {
        if (key_.first != nullptr) {
            NotificationQueue::instance().cancel(key_);
        }
    }

private:
    void* owner_ = nullptr;
    void (*callback_)(void*) = nullptr;
    NotificationQueue::Key key_{nullptr, 0};
#if OC_ENABLE_STATS
    const char* debug_label_ = nullptr;
#endif
};

namespace detail {

struct StaticWatchNotificationThunk {
    StaticWatchGroupBase* group = nullptr;

    void operator()() const { group->enqueue(); }

    template <typename T>
    void operator()(const T&) const {
        group->enqueue();
    }
};

template <typename SignalLike>
// These setup-only adapters are expanded into their caller so a cold caller's
// section placement is preserved. Applying FLASHMEM directly to template
// instantiations creates conflicting COMDAT sections on Teensy GCC.
OC_ALWAYS_INLINE Subscription subscribeStaticWatchGroup(SignalLike& signal,
                                                         StaticWatchGroupBase& group) {
    ScopedSubscriptionDebugContext context(group.debugLabel());
    return signal.subscribe(StaticWatchNotificationThunk{&group});
}

}  // namespace detail

template <size_t MaxSignals>
class StaticWatchGroup : public StaticWatchGroupBase {

public:
    StaticWatchGroup() = default;

    StaticWatchGroup(const StaticWatchGroup&) = delete;
    StaticWatchGroup& operator=(const StaticWatchGroup&) = delete;

    template <auto Callback, typename Owner>
    void bind(Owner& owner, size_t keyIndex, const char* debugLabel = nullptr) {
        static_assert(std::is_member_function_pointer_v<decltype(Callback)>,
                      "Callback must be a member function pointer");

        clear();
        bindRaw(
            static_cast<void*>(&owner),
            [](void* context) { (static_cast<Owner*>(context)->*Callback)(); },
            keyIndex,
            debugLabel
        );
    }

    template <typename SignalLike>
    OC_ALWAYS_INLINE bool watch(SignalLike& signal) {
        if (!bound()) {
            OC_LOG_ERROR("[StaticWatchGroup] watch() called before bind label={}",
                         debugLabel() ? debugLabel() : "<unnamed>");
            return false;
        }

        if (subscriptions_.full()) {
            OC_LOG_ERROR("[StaticWatchGroup] capacity exceeded label={} subscriptions={} max={}",
                         debugLabel() ? debugLabel() : "<unnamed>",
                         subscriptions_.size(),
                         MaxSignals);
            return false;
        }

        return subscriptions_.tryAdd(detail::subscribeStaticWatchGroup(signal, *this));
    }

    template <typename... Signals>
    OC_ALWAYS_INLINE bool watchAll(Signals&... signals) {
        bool ok = true;
        ((ok = watch(signals) && ok), ...);
        return ok;
    }

    void clear() {
        cancelPending();
        subscriptions_.clear();
    }

    [[nodiscard]] size_t subscriptionCount() const { return subscriptions_.size(); }
    [[nodiscard]] static constexpr size_t capacity() { return MaxSignals; }

private:
    FixedSubscriptionList<MaxSignals> subscriptions_;
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
