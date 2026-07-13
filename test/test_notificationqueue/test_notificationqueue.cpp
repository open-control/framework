#include <unity.h>

#include <oc/state/NotificationQueue.hpp>
#if OC_ENABLE_STATS
#include <oc/diagnostics/Performance.hpp>
#include <oc/log/Log.hpp>
#include <oc/state/DerivedSignal.hpp>
#include <oc/state/Signal.hpp>
#include <oc/state/SignalString.hpp>
#include <oc/state/SignalWatcher.hpp>
#include <oc/state/StaticSignalWatcher.hpp>

#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#endif

using namespace oc::state;

namespace {

struct CounterContext {
    int count = 0;
    NotificationQueue::Key reenqueueKey{nullptr, 0};
    bool reenqueueTriggered = false;
};

struct CancelContext {
    int count = 0;
    void* owner = nullptr;
};

void incrementCounter(void* context, size_t);

#if OC_ENABLE_STATS

std::string capturedLog;

void captureChar(char value) {
    capturedLog.push_back(value);
}

void captureString(const char* value) {
    if (value != nullptr) capturedLog += value;
}

void captureInt32(int32_t value) {
    capturedLog += std::to_string(value);
}

void captureUint32(uint32_t value) {
    capturedLog += std::to_string(value);
}

void captureFloat(float value) {
    capturedLog += std::to_string(value);
}

void captureBool(bool value) {
    capturedLog += value ? "true" : "false";
}

uint32_t captureTimeMs() {
    return 0;
}

const oc::log::Output captureOutput{
    captureChar,
    captureString,
    captureInt32,
    captureUint32,
    captureFloat,
    captureBool,
    captureTimeMs,
};

struct PerformanceCapture {
    bool sawNotificationFlush = false;
    oc::diagnostics::PerformanceSample notificationFlush{};
};

std::string formatPointer(const void* value) {
    constexpr char HEX_DIGITS[] = "0123456789abcdef";
    const uintptr_t address = reinterpret_cast<uintptr_t>(value);
    std::string result{"0x"};

    for (int shift = static_cast<int>(sizeof(uintptr_t) * 8U) - 4;
         shift >= 0;
         shift -= 4) {
        const auto digit = static_cast<uint8_t>((address >> shift) & 0x0FU);
        result.push_back(HEX_DIGITS[digit]);
    }
    return result;
}

void fillNotificationQueue(
    std::array<int, NotificationQueue::maxPending()>& counters,
    const char* debugLabel
) {
    for (size_t index = 0; index < counters.size(); ++index) {
        NotificationQueue::instance().enqueue(
            {&counters[index], index},
            &counters[index],
            incrementCounter,
            debugLabel
        );
    }
}

void assertAllCallbacksRan(
    const std::array<int, NotificationQueue::maxPending()>& counters
) {
    for (const auto counter : counters) {
        TEST_ASSERT_EQUAL(1, counter);
    }
}

struct StaticWatchOwner {
    int callbackCount = 0;
    std::function<void()> action;

    void onChanged() {
        ++callbackCount;
        if (action) action();
    }
};

void capturePerformance(
    void* context,
    const oc::diagnostics::PerformanceSample& sample
) {
    auto* capture = static_cast<PerformanceCapture*>(context);
    if (sample.label != nullptr && std::strcmp(sample.label, "notifications.flush") == 0) {
        capture->sawNotificationFlush = true;
        capture->notificationFlush = sample;
    }
}

#endif

void incrementCounter(void* context, size_t) {
    auto* counter = static_cast<int*>(context);
    (*counter)++;
}

void incrementContextCounter(void* context, size_t) {
    auto* counter = static_cast<CounterContext*>(context);
    counter->count++;
}

void incrementAndReenqueue(void* context, size_t) {
    auto* counter = static_cast<CounterContext*>(context);
    counter->count++;
    if (!counter->reenqueueTriggered) {
        counter->reenqueueTriggered = true;
        NotificationQueue::instance().enqueue(counter->reenqueueKey, context, incrementContextCounter);
    }
}

void incrementAndCancelOwner(void* context, size_t) {
    auto* cancel = static_cast<CancelContext*>(context);
    ++cancel->count;
    NotificationQueue::instance().cancelOwner(cancel->owner);
}

}  // namespace

void setUp() {
#if OC_ENABLE_STATS
    oc::diagnostics::clearPerformanceSink();
    oc::log::setOutput(captureOutput);
#endif
    NotificationQueue::instance().flush();
    NotificationQueue::instance().setDeferredMode(true);
    NotificationQueue::instance().resetOverflowCount();
#if OC_ENABLE_STATS
    capturedLog.clear();
#endif
}

void tearDown() {
    NotificationQueue::instance().flush();
    NotificationQueue::instance().setDeferredMode(true);
#if OC_ENABLE_STATS
    oc::diagnostics::clearPerformanceSink();
    capturedLog.clear();
#endif
}

void test_enqueue_coalesces_same_key_without_dynamic_storage() {
    int counter = 0;
    auto key = NotificationQueue::Key(&counter, 1);

    NotificationQueue::instance().enqueue(key, &counter, incrementCounter);
    NotificationQueue::instance().enqueue(key, &counter, incrementCounter);

    TEST_ASSERT_EQUAL(1, NotificationQueue::instance().pendingCount());
    NotificationQueue::instance().flush();
    TEST_ASSERT_EQUAL(1, counter);
}

void test_flush_processes_notifications_enqueued_during_flush() {
    CounterContext context{};
    context.reenqueueKey = NotificationQueue::Key(&context, 2);

    NotificationQueue::instance().enqueue({&context, 1}, &context, incrementAndReenqueue);

    NotificationQueue::instance().flush();

    TEST_ASSERT_EQUAL(2, context.count);
    TEST_ASSERT_FALSE(NotificationQueue::instance().hasPending());
}

void test_overflow_is_bounded_and_counted() {
    int counters[NotificationQueue::maxPending() + 1] = {};

    for (size_t i = 0; i < NotificationQueue::maxPending(); ++i) {
        NotificationQueue::instance().enqueue({&counters[i], i}, &counters[i], incrementCounter);
    }

    TEST_ASSERT_EQUAL(NotificationQueue::maxPending(), NotificationQueue::instance().pendingCount());
    TEST_ASSERT_FALSE(NotificationQueue::instance().hasOverflowed());

    NotificationQueue::instance().enqueue({&counters[NotificationQueue::maxPending()], 99},
                                          &counters[NotificationQueue::maxPending()],
                                          incrementCounter);

    TEST_ASSERT_TRUE(NotificationQueue::instance().hasOverflowed());
    TEST_ASSERT_EQUAL(1, NotificationQueue::instance().overflowCount());
    TEST_ASSERT_EQUAL(NotificationQueue::maxPending(), NotificationQueue::instance().pendingCount());

    NotificationQueue::instance().flush();
    TEST_ASSERT_FALSE(NotificationQueue::instance().hasPending());
}

void test_cancel_removes_only_matching_key() {
    int counters[2] = {};
    void* owner = &counters;
    NotificationQueue::instance().enqueue({owner, 1}, &counters[0], incrementCounter);
    NotificationQueue::instance().enqueue({owner, 2}, &counters[1], incrementCounter);

    NotificationQueue::instance().cancel({owner, 1});
    TEST_ASSERT_EQUAL(1, NotificationQueue::instance().pendingCount());
    NotificationQueue::instance().flush();

    TEST_ASSERT_EQUAL(0, counters[0]);
    TEST_ASSERT_EQUAL(1, counters[1]);
}

void test_cancel_owner_suppresses_later_callbacks_in_active_wave() {
    CancelContext context{};
    context.owner = &context;
    int later = 0;
    NotificationQueue::instance().enqueue(
        {context.owner, 1},
        &context,
        incrementAndCancelOwner
    );
    NotificationQueue::instance().enqueue({context.owner, 2}, &later, incrementCounter);

    NotificationQueue::instance().flush();

    TEST_ASSERT_EQUAL(1, context.count);
    TEST_ASSERT_EQUAL(0, later);
    TEST_ASSERT_FALSE(NotificationQueue::instance().hasPending());
}

#if OC_ENABLE_STATS

void test_overflow_diagnostics_correlate_current_and_rejected_signals() {
    std::array<int, NotificationQueue::maxPending()> nextWaveCounters{};
    int rejectedCallbacks = 0;
    Signal<int> currentSignal{0};
    Signal<int> rejectedSignal{0};
    currentSignal.setDebugLabel("current.signal");
    rejectedSignal.setDebugLabel("rejected.signal");

    auto rejectedSubscription = rejectedSignal.subscribe(
        [&rejectedCallbacks](const int&) { ++rejectedCallbacks; }
    );
    auto currentSubscription = currentSignal.subscribe(
        [&nextWaveCounters, &rejectedSignal](const int&) {
            fillNotificationQueue(nextWaveCounters, "next-wave.raw");
            rejectedSignal.set(1);
        }
    );
    TEST_ASSERT_TRUE(rejectedSubscription.isValid());
    TEST_ASSERT_TRUE(currentSubscription.isValid());

    PerformanceCapture performance{};
    oc::diagnostics::setPerformanceSink(&performance, capturePerformance);

    currentSignal.set(1);
    NotificationQueue::instance().flush();
    oc::diagnostics::clearPerformanceSink();

    TEST_ASSERT_EQUAL(1, NotificationQueue::instance().overflowCount());
    TEST_ASSERT_EQUAL(0, rejectedCallbacks);
    TEST_ASSERT_FALSE(NotificationQueue::instance().hasPending());
    assertAllCallbacksRan(nextWaveCounters);

    TEST_ASSERT_TRUE(performance.sawNotificationFlush);
    TEST_ASSERT_EQUAL_UINT32(
        static_cast<uint32_t>(NotificationQueue::maxPending()),
        performance.notificationFlush.unitA
    );
    TEST_ASSERT_EQUAL_UINT32(2, performance.notificationFlush.unitB);

    const auto currentOwner = formatPointer(&currentSignal);
    const auto rejectedOwner = formatPointer(&rejectedSignal);
    TEST_ASSERT_TRUE(capturedLog.find("[NotificationQueue] overflow") != std::string::npos);
    TEST_ASSERT_TRUE(capturedLog.find("dropped=1") != std::string::npos);
    TEST_ASSERT_TRUE(capturedLog.find("pending=64") != std::string::npos);
    TEST_ASSERT_TRUE(capturedLog.find("capacity=64") != std::string::npos);
    TEST_ASSERT_TRUE(capturedLog.find("highWater=64") != std::string::npos);
    TEST_ASSERT_TRUE(capturedLog.find("wave=1") != std::string::npos);
    TEST_ASSERT_TRUE(capturedLog.find("currentLabel=current.signal") != std::string::npos);
    TEST_ASSERT_TRUE(capturedLog.find("currentOwner=" + currentOwner) != std::string::npos);
    TEST_ASSERT_TRUE(capturedLog.find("currentSlot=0") != std::string::npos);
    TEST_ASSERT_TRUE(capturedLog.find("rejectedLabel=rejected.signal") != std::string::npos);
    TEST_ASSERT_TRUE(capturedLog.find("rejectedOwner=" + rejectedOwner) != std::string::npos);
    TEST_ASSERT_TRUE(capturedLog.find("rejectedSlot=0") != std::string::npos);
}

void test_string_debug_labels_forward_to_queue_diagnostics() {
    std::array<int, NotificationQueue::maxPending()> queuedCounters{};
    int rejectedCallbacks = 0;
    SignalLabel statusText;
    statusText.setDebugLabel("status.text");
    TEST_ASSERT_EQUAL_STRING("status.text", statusText.debugLabel());

    Signal<int> source{7};
    DerivedStringSignal<int, 8> derived{
        source,
        [](int value, char* buffer, size_t size) {
            std::snprintf(buffer, size, "%d", value);
        }
    };
    derived.setDebugLabel("macro.display");
    TEST_ASSERT_EQUAL_STRING("macro.display", derived.debugLabel());

    auto subscription = statusText.subscribe(
        [&rejectedCallbacks](const char*) { ++rejectedCallbacks; }
    );
    TEST_ASSERT_TRUE(subscription.isValid());

    fillNotificationQueue(queuedCounters, "queue.fill");
    statusText.set("ready");

    TEST_ASSERT_EQUAL(1, NotificationQueue::instance().overflowCount());
    TEST_ASSERT_EQUAL(0, rejectedCallbacks);
    TEST_ASSERT_TRUE(capturedLog.find("currentLabel=<none>") != std::string::npos);
    TEST_ASSERT_TRUE(capturedLog.find("rejectedLabel=status.text") != std::string::npos);

    NotificationQueue::instance().flush();
    assertAllCallbacksRan(queuedCounters);
}

void test_watch_group_label_is_current_overflow_context() {
    std::array<int, NotificationQueue::maxPending()> queuedCounters{};
    int watchCallbacks = 0;
    int rejectedCallbacks = 0;
    Signal<int> trigger{0};
    Signal<int> rejectedSignal{0};
    trigger.setDebugLabel("watch.trigger");
    rejectedSignal.setDebugLabel("watch.rejected-signal");

    auto rejectedSubscription = rejectedSignal.subscribe(
        [&rejectedCallbacks](const int&) { ++rejectedCallbacks; }
    );
    SignalWatcher watcher;
    auto& group = watcher.group("watch.current-group", [&]() {
        ++watchCallbacks;
        fillNotificationQueue(queuedCounters, "watch.fill");
        rejectedSignal.set(1);
    });
    group.watch(trigger);
    TEST_ASSERT_TRUE(rejectedSubscription.isValid());

    trigger.set(1);
    NotificationQueue::instance().flush();

    TEST_ASSERT_EQUAL(1, NotificationQueue::instance().overflowCount());
    TEST_ASSERT_EQUAL(1, watchCallbacks);
    TEST_ASSERT_EQUAL(0, rejectedCallbacks);
    assertAllCallbacksRan(queuedCounters);
    TEST_ASSERT_TRUE(capturedLog.find("currentLabel=watch.current-group") != std::string::npos);
    TEST_ASSERT_TRUE(
        capturedLog.find("currentOwner=" + formatPointer(&watcher)) != std::string::npos
    );
    TEST_ASSERT_TRUE(capturedLog.find("currentSlot=0") != std::string::npos);
    TEST_ASSERT_TRUE(
        capturedLog.find("rejectedLabel=watch.rejected-signal") != std::string::npos
    );
    TEST_ASSERT_TRUE(
        capturedLog.find("rejectedOwner=" + formatPointer(&rejectedSignal)) != std::string::npos
    );
    TEST_ASSERT_TRUE(capturedLog.find("rejectedSlot=0") != std::string::npos);
}

void test_watch_group_label_is_rejected_overflow_context() {
    std::array<int, NotificationQueue::maxPending()> queuedCounters{};
    int watchCallbacks = 0;
    Signal<int> source{0};
    source.setDebugLabel("watch.source");

    auto fillerSubscription = source.subscribe([&](const int&) {
        fillNotificationQueue(queuedCounters, "watch.reject-fill");
    });
    SignalWatcher watcher;
    auto& group = watcher.group("watch.rejected-group", [&]() { ++watchCallbacks; });
    group.watch(source);
    TEST_ASSERT_TRUE(fillerSubscription.isValid());

    source.set(1);
    NotificationQueue::instance().flush();

    TEST_ASSERT_EQUAL(1, NotificationQueue::instance().overflowCount());
    TEST_ASSERT_EQUAL(0, watchCallbacks);
    assertAllCallbacksRan(queuedCounters);
    TEST_ASSERT_TRUE(capturedLog.find("currentLabel=watch.source") != std::string::npos);
    TEST_ASSERT_TRUE(
        capturedLog.find("currentOwner=" + formatPointer(&source)) != std::string::npos
    );
    TEST_ASSERT_TRUE(capturedLog.find("currentSlot=1") != std::string::npos);
    TEST_ASSERT_TRUE(
        capturedLog.find("rejectedLabel=watch.rejected-group") != std::string::npos
    );
    TEST_ASSERT_TRUE(
        capturedLog.find("rejectedOwner=" + formatPointer(&watcher)) != std::string::npos
    );
    TEST_ASSERT_TRUE(capturedLog.find("rejectedSlot=0") != std::string::npos);
}

void test_static_watch_group_labels_are_current_and_rejected_context() {
    std::array<int, NotificationQueue::maxPending()> queuedCounters{};
    StaticWatchOwner currentOwner{};
    StaticWatchOwner rejectedOwner{};
    StaticWatchGroup<1> currentGroup;
    StaticWatchGroup<1> rejectedGroup;
    rejectedGroup.bind<&StaticWatchOwner::onChanged>(
        rejectedOwner,
        9,
        "static.rejected-group"
    );
    currentOwner.action = [&]() {
        fillNotificationQueue(queuedCounters, "static.fill");
        rejectedGroup.enqueue();
    };
    currentGroup.bind<&StaticWatchOwner::onChanged>(
        currentOwner,
        7,
        "static.current-group"
    );

    currentGroup.enqueue();
    NotificationQueue::instance().flush();

    TEST_ASSERT_EQUAL(1, NotificationQueue::instance().overflowCount());
    TEST_ASSERT_EQUAL(1, currentOwner.callbackCount);
    TEST_ASSERT_EQUAL(0, rejectedOwner.callbackCount);
    assertAllCallbacksRan(queuedCounters);
    TEST_ASSERT_TRUE(
        capturedLog.find("currentLabel=static.current-group") != std::string::npos
    );
    TEST_ASSERT_TRUE(
        capturedLog.find("currentOwner=" + formatPointer(&currentGroup)) != std::string::npos
    );
    TEST_ASSERT_TRUE(capturedLog.find("currentSlot=7") != std::string::npos);
    TEST_ASSERT_TRUE(
        capturedLog.find("rejectedLabel=static.rejected-group") != std::string::npos
    );
    TEST_ASSERT_TRUE(
        capturedLog.find("rejectedOwner=" + formatPointer(&rejectedGroup)) != std::string::npos
    );
    TEST_ASSERT_TRUE(capturedLog.find("rejectedSlot=9") != std::string::npos);
}

void test_derived_string_label_is_current_overflow_context() {
    std::array<int, NotificationQueue::maxPending()> queuedCounters{};
    int derivedCallbacks = 0;
    int rejectedCallbacks = 0;
    Signal<int> source{0};
    Signal<int> rejectedSignal{0};
    source.setDebugLabel("derived.source");
    rejectedSignal.setDebugLabel("derived.rejected-signal");
    DerivedStringSignal<int, 8> derived{
        source,
        [](int value, char* buffer, size_t size) {
            std::snprintf(buffer, size, "%d", value);
        }
    };
    derived.setDebugLabel("derived.current-output");

    auto derivedSubscription = derived.subscribe([&](const char*) {
        ++derivedCallbacks;
        fillNotificationQueue(queuedCounters, "derived.fill");
        rejectedSignal.set(1);
    });
    auto rejectedSubscription = rejectedSignal.subscribe(
        [&rejectedCallbacks](const int&) { ++rejectedCallbacks; }
    );
    TEST_ASSERT_TRUE(derivedSubscription.isValid());
    TEST_ASSERT_TRUE(rejectedSubscription.isValid());

    source.set(1);
    NotificationQueue::instance().flush();

    TEST_ASSERT_EQUAL(1, NotificationQueue::instance().overflowCount());
    TEST_ASSERT_EQUAL(1, derivedCallbacks);
    TEST_ASSERT_EQUAL(0, rejectedCallbacks);
    assertAllCallbacksRan(queuedCounters);
    TEST_ASSERT_TRUE(
        capturedLog.find("currentLabel=derived.current-output") != std::string::npos
    );
    TEST_ASSERT_TRUE(capturedLog.find("currentOwner=0x") != std::string::npos);
    TEST_ASSERT_TRUE(capturedLog.find("currentSlot=0") != std::string::npos);
    TEST_ASSERT_TRUE(
        capturedLog.find("rejectedLabel=derived.rejected-signal") != std::string::npos
    );
    TEST_ASSERT_TRUE(
        capturedLog.find("rejectedOwner=" + formatPointer(&rejectedSignal)) != std::string::npos
    );
    TEST_ASSERT_TRUE(capturedLog.find("rejectedSlot=0") != std::string::npos);
}

void test_derived_string_label_is_rejected_overflow_context() {
    std::array<int, NotificationQueue::maxPending()> queuedCounters{};
    int derivedCallbacks = 0;
    Signal<int> source{0};
    source.setDebugLabel("derived.reject-source");

    auto fillerSubscription = source.subscribe([&](const int&) {
        fillNotificationQueue(queuedCounters, "derived.reject-fill");
    });
    DerivedStringSignal<int, 8> derived{
        source,
        [](int value, char* buffer, size_t size) {
            std::snprintf(buffer, size, "%d", value);
        }
    };
    derived.setDebugLabel("derived.rejected-output");
    auto derivedSubscription = derived.subscribe(
        [&derivedCallbacks](const char*) { ++derivedCallbacks; }
    );
    TEST_ASSERT_TRUE(fillerSubscription.isValid());
    TEST_ASSERT_TRUE(derivedSubscription.isValid());

    source.set(1);
    NotificationQueue::instance().flush();

    TEST_ASSERT_EQUAL(1, NotificationQueue::instance().overflowCount());
    TEST_ASSERT_EQUAL(0, derivedCallbacks);
    assertAllCallbacksRan(queuedCounters);
    TEST_ASSERT_TRUE(
        capturedLog.find("currentLabel=derived.reject-source") != std::string::npos
    );
    TEST_ASSERT_TRUE(
        capturedLog.find("currentOwner=" + formatPointer(&source)) != std::string::npos
    );
    TEST_ASSERT_TRUE(capturedLog.find("currentSlot=1") != std::string::npos);
    TEST_ASSERT_TRUE(
        capturedLog.find("rejectedLabel=derived.rejected-output") != std::string::npos
    );
    TEST_ASSERT_TRUE(capturedLog.find("rejectedOwner=0x") != std::string::npos);
    TEST_ASSERT_TRUE(capturedLog.find("rejectedSlot=0") != std::string::npos);
}

#endif

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_enqueue_coalesces_same_key_without_dynamic_storage);
    RUN_TEST(test_flush_processes_notifications_enqueued_during_flush);
    RUN_TEST(test_overflow_is_bounded_and_counted);
    RUN_TEST(test_cancel_removes_only_matching_key);
    RUN_TEST(test_cancel_owner_suppresses_later_callbacks_in_active_wave);
#if OC_ENABLE_STATS
    RUN_TEST(test_overflow_diagnostics_correlate_current_and_rejected_signals);
    RUN_TEST(test_string_debug_labels_forward_to_queue_diagnostics);
    RUN_TEST(test_watch_group_label_is_current_overflow_context);
    RUN_TEST(test_watch_group_label_is_rejected_overflow_context);
    RUN_TEST(test_static_watch_group_labels_are_current_and_rejected_context);
    RUN_TEST(test_derived_string_label_is_current_overflow_context);
    RUN_TEST(test_derived_string_label_is_rejected_overflow_context);
#endif
    return UNITY_END();
}
