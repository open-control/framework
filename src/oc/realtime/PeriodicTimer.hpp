#pragma once

#include <cstdint>
#include <utility>

#include <oc/realtime/InterruptGuard.hpp>

#ifdef ARDUINO
    #include <IntervalTimer.h>
#endif

namespace oc::realtime {

namespace detail {

/**
 * Serialize one complete vendor timer transition.
 *
 * The Teensy IntervalTimer allocator scans and then publishes shared PIT state.
 * Keeping the guard alive across the vendor call makes that whole transition
 * indivisible and restores the caller's exact interrupt-mask state afterwards.
 */
template <typename Guard, typename Timer, typename Callback>
bool guardedTimerBegin(Timer& timer, Callback&& callback, uint32_t periodUs) {
    Guard guard;
    return timer.begin(std::forward<Callback>(callback), periodUs);
}

template <typename Guard, typename Timer>
void guardedTimerEnd(Timer& timer) {
    Guard guard;
    timer.end();
}

}  // namespace detail

/**
 * Framework authority for IntervalTimer allocation and release.
 *
 * Framework and application code must use this wrapper rather than access the
 * shared PIT allocator directly. External raw users must serialize their whole
 * begin/end transition against the same interrupt mask.
 */
class PeriodicTimer {
public:
    PeriodicTimer() = default;

    ~PeriodicTimer() {
        end();
    }

    PeriodicTimer(const PeriodicTimer&) = delete;
    PeriodicTimer& operator=(const PeriodicTimer&) = delete;

    template <typename Callback>
    bool begin(Callback&& callback, uint32_t periodUs) {
#ifdef ARDUINO
        return detail::guardedTimerBegin<InterruptGuard>(
            timer_,
            std::forward<Callback>(callback),
            periodUs
        );
#else
        (void)callback;
        (void)periodUs;
        return false;
#endif
    }

    void end() {
#ifdef ARDUINO
        detail::guardedTimerEnd<InterruptGuard>(timer_);
#endif
    }

    void setPriority(uint8_t priority) {
#ifdef ARDUINO
        timer_.priority(priority);
#else
        (void)priority;
#endif
    }

private:
#ifdef ARDUINO
    IntervalTimer timer_{};
#endif
};

}  // namespace oc::realtime
