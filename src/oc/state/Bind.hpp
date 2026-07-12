#pragma once

#include <oc/state/FixedSubscriptionList.hpp>
#include <oc/state/Signal.hpp>

#include <utility>
#include <vector>

namespace oc::state {

/**
 * @brief Fluent builder for batch subscription management
 *
 * Binder provides a clean, chainable API for subscribing to multiple signals
 * and storing the resulting Subscriptions in either fixed or dynamic storage.
 *
 * ## Usage
 *
 * @code
 * class DeviceView {
 *     FixedSubscriptionList<3> subs_;
 *
 *     void setupBindings(DeviceState& state) {
 *         bind(subs_)
 *             .on(state.name,    [this](auto n) { nameLabel_.setText(n); })
 *             .on(state.enabled, [this](auto e) { led_.setOn(e); })
 *             .on(state.volume,  [this](auto v) { knob_.setValue(v); });
 *     }
 * };
 * @endcode
 *
 * ## Lifecycle
 *
 * When the storage is cleared or destroyed, all subscriptions are
 * automatically unsubscribed via RAII.
 *
 * @code
 * // Unbind all at once
 * subs_.clear();
 *
 * // Or let destructor handle it
 * // ~DeviceView() { } // subs_ destroyed -> all unsubscribed
 * @endcode
 *
 * ## With Parameters Array
 *
 * @code
 * void bindParameters(std::array<ParameterState, 8>& params) {
 *     for (uint8_t i = 0; i < 8; ++i) {
 *         bind(subs_)
 *             .on(params[i].name,    [this, i](auto n) { widgets_[i]->setName(n); })
 *             .on(params[i].value,   [this, i](auto v) { widgets_[i]->setValue(v); })
 *             .on(params[i].display, [this, i](auto d) { widgets_[i]->setDisplay(d); });
 *     }
 * }
 * @endcode
 *
 * @see Signal, SignalString for the observable primitives
 */
template <typename Storage>
class Binder {
public:
    /**
     * @brief Construct a Binder attached to a subscription vector
     * @param subs Vector where subscriptions will be stored
     */
    explicit Binder(Storage& subs) : subs_(subs) {}

    /**
     * @brief Subscribe to a signal and store the subscription
     *
     * @tparam S Signal type (Signal<T> or SignalStringBase<N>)
     * @tparam F Callback type (typically a lambda)
     * @param signal The signal to subscribe to
     * @param callback Function called when signal changes
     * @return Reference to this Binder for chaining
     *
     * @code
     * bind(subs_)
     *     .on(state.name, [](const char* n) { ... })
     *     .on(state.value, [](float v) { ... });
     * @endcode
     */
    template <typename S, typename F>
    Binder& on(S& signal, F&& callback) {
        subs_.push_back(signal.subscribe(std::forward<F>(callback)));
        return *this;
    }

    /**
     * @brief Subscribe and immediately invoke callback with current value
     *
     * Only works with SignalStringBase types that have subscribeAndInvoke().
     * For Signal<T>, use on() and manually invoke if needed.
     *
     * @tparam S SignalStringBase type
     * @tparam F Callback type
     * @param signal The signal to subscribe to
     * @param callback Function called immediately and on future changes
     * @return Reference to this Binder for chaining
     */
    template <typename S, typename F>
    Binder& onImmediate(S& signal, F&& callback) {
        subs_.push_back(signal.subscribeAndInvoke(std::forward<F>(callback)));
        return *this;
    }

private:
    Storage& subs_;
};

/**
 * @brief Create a Binder for fluent subscription building
 *
 * @param subs Vector to store subscriptions in
 * @return Binder instance for chaining .on() calls
 *
 * @code
 * std::vector<Subscription> subs;
 * subs.reserve(10);
 *
 * bind(subs)
 *     .on(signal1, callback1)
 *     .on(signal2, callback2)
 *     .on(signal3, callback3);
 * @endcode
 */
template <typename Storage>
Binder<Storage> bind(Storage& subs) {
    return Binder<Storage>(subs);
}

}  // namespace oc::state
