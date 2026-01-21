#pragma once

#include <cstdint>
#include <oc/type/Ids.hpp>
#include <oc/type/Callbacks.hpp>

namespace oc::test {

/**
 * @brief Fake time provider for testing time-based logic
 *
 * Usage:
 *   FakeTime time;
 *   InputBinding binding(bus, time.provider(), config);
 *
 *   time.advance(500);  // Advance 500ms
 *   binding.processTick();
 */
class FakeTime {
public:
    FakeTime() : current_time_(0) {}

    /// Get the oc::type::TimeProvider function pointer
    oc::type::TimeProvider provider() {
        // Store pointer to this instance for the lambda
        instance_ = this;
        return []() -> uint32_t {
            return instance_->current_time_;
        };
    }

    /// Get current time
    uint32_t now() const { return current_time_; }

    /// Set absolute time
    void set(uint32_t ms) { current_time_ = ms; }

    /// Advance time by delta
    void advance(uint32_t deltaMs) { current_time_ += deltaMs; }

    /// Reset to 0
    void reset() { current_time_ = 0; }

private:
    uint32_t current_time_;
    static inline FakeTime* instance_ = nullptr;
};

}  // namespace oc::test
