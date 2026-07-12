#include <oc/diagnostics/Performance.hpp>

#if OC_ENABLE_STATS

#include <atomic>

namespace oc::diagnostics {

namespace {

void* g_sink_context = nullptr;
std::atomic<PerformanceSink> g_sink{nullptr};
static_assert(std::atomic<PerformanceSink>::is_always_lock_free,
              "Performance sink dispatch must remain lock-free");

}  // namespace

void setPerformanceSink(void* context, PerformanceSink sink) {
    g_sink.store(nullptr, std::memory_order_release);
    g_sink_context = context;
    g_sink.store(sink, std::memory_order_release);
}

void clearPerformanceSink() {
    g_sink.store(nullptr, std::memory_order_release);
}

void recordPerformance(const PerformanceSample& sample) {
    if (const PerformanceSink sink = g_sink.load(std::memory_order_acquire)) {
        sink(g_sink_context, sample);
    }
}

}  // namespace oc::diagnostics

#endif
