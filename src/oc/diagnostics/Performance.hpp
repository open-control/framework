#pragma once

#include <cstdint>

#include <oc/Config.hpp>

#if OC_ENABLE_STATS
#include <oc/time/Time.hpp>
#endif

namespace oc::diagnostics {

struct PerformanceSample {
    // Labels are queued by pointer; they must have static storage duration.
    const char* label = nullptr;
    uint32_t elapsedUs = 0;
    uint32_t unitA = 0;
    uint32_t unitB = 0;
};

#if OC_ENABLE_STATS

using PerformanceSink = void (*)(void* context, const PerformanceSample& sample);

/**
 * Install the process-wide diagnostics sink before emitters start. Reconfigure
 * or clear it only after emitters have stopped. The sink must be allocation-free
 * and safe for every context that records samples.
 */
void setPerformanceSink(void* context, PerformanceSink sink);
void clearPerformanceSink();
void recordPerformance(const PerformanceSample& sample);

class PerformanceScope {
public:
    /** label must remain valid for the lifetime of the diagnostics sink. */
    explicit PerformanceScope(const char* label);

    PerformanceScope(const PerformanceScope&) = delete;
    PerformanceScope& operator=(const PerformanceScope&) = delete;

    ~PerformanceScope();

    void setUnits(uint32_t unitA, uint32_t unitB = 0);

private:
    const char* label_ = nullptr;
    uint32_t startedAtUs_ = 0;
    uint32_t unitA_ = 0;
    uint32_t unitB_ = 0;
};

#endif

}  // namespace oc::diagnostics

#if OC_ENABLE_STATS
#define OC_PERF_SCOPE(variable, label) \
    ::oc::diagnostics::PerformanceScope variable{label}
#define OC_PERF_RECORD(label, elapsedUs, unitA, unitB) \
    ::oc::diagnostics::recordPerformance({label, elapsedUs, unitA, unitB})
#define OC_PERF_UNITS(variable, unitA, unitB) variable.setUnits(unitA, unitB)
#else
#define OC_PERF_SCOPE(variable, label) ((void)0)
#define OC_PERF_RECORD(label, elapsedUs, unitA, unitB) ((void)0)
#define OC_PERF_UNITS(variable, unitA, unitB) ((void)0)
#endif
