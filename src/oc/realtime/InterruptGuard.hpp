#pragma once

#include <cstdint>

#ifdef ARDUINO
    #include <Arduino.h>
#endif

namespace oc::realtime {

class InterruptGuard {
public:
    InterruptGuard() {
#ifdef ARDUINO
        primask_ = readPrimask_();
        __disable_irq();
#endif
    }

    ~InterruptGuard() {
#ifdef ARDUINO
        asm volatile("MSR primask, %0" : : "r"(primask_) : "memory");
#endif
    }

    InterruptGuard(const InterruptGuard&) = delete;
    InterruptGuard& operator=(const InterruptGuard&) = delete;

private:
#ifdef ARDUINO
    static uint32_t readPrimask_() {
        uint32_t primask = 0;
        asm volatile("MRS %0, primask" : "=r"(primask));
        return primask;
    }
#endif

    uint32_t primask_ = 0;
};

}  // namespace oc::realtime
