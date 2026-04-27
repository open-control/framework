#pragma once

#include <cstdint>

#ifdef ARDUINO
    #include <IntervalTimer.h>
#endif

namespace oc::realtime {

class PeriodicTimer {
public:
    PeriodicTimer() = default;

    PeriodicTimer(const PeriodicTimer&) = delete;
    PeriodicTimer& operator=(const PeriodicTimer&) = delete;

    template <typename Callback>
    bool begin(Callback&& callback, uint32_t periodUs) {
#ifdef ARDUINO
        return timer_.begin(callback, periodUs);
#else
        (void)callback;
        (void)periodUs;
        return false;
#endif
    }

    void end() {
#ifdef ARDUINO
        timer_.end();
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
