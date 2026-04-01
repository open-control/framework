#include <unity.h>

#include <oc/state/NotificationQueue.hpp>
#include <oc/state/Signal.hpp>
#include <oc/state/SignalString.hpp>
#include <oc/state/SignalVector.hpp>
#include <oc/state/SignalWatcher.hpp>

#include <array>

using namespace oc::state;

static int callCountA = 0;
static int callCountB = 0;

void setUp() {
    callCountA = 0;
    callCountB = 0;
    NotificationQueue::instance().flush();
    NotificationQueue::instance().setDeferredMode(false);
}

void tearDown() {
    NotificationQueue::instance().flush();
    NotificationQueue::instance().setDeferredMode(true);
}

void test_watch_all_coalesces_multiple_signals_into_single_callback() {
    Signal<int> a{0};
    Signal<int> b{0};
    SignalWatcher watcher;

    watcher.watchAll([&]() { callCountA++; }, a, b);

    NotificationQueue::instance().setDeferredMode(true);

    a.set(1);
    b.set(2);

    TEST_ASSERT_EQUAL(0, callCountA);
    TEST_ASSERT_TRUE(NotificationQueue::instance().hasPending());

    NotificationQueue::instance().flush();

    TEST_ASSERT_EQUAL(1, callCountA);
}

void test_group_supports_incremental_binding_from_arrays() {
    std::array<Signal<int>, 4> signals = {
        Signal<int>{0},
        Signal<int>{0},
        Signal<int>{0},
        Signal<int>{0},
    };
    SignalWatcher watcher;

    auto& group = watcher.group([&]() { callCountA++; });
    for (auto& signal : signals) {
        group.watch(signal);
    }

    TEST_ASSERT_EQUAL(1, watcher.groupCount());
    TEST_ASSERT_EQUAL(4, watcher.subscriptionCount());

    NotificationQueue::instance().setDeferredMode(true);

    for (size_t i = 0; i < signals.size(); ++i) {
        signals[i].set(static_cast<int>(i) + 1);
    }

    TEST_ASSERT_EQUAL(0, callCountA);
    TEST_ASSERT_TRUE(NotificationQueue::instance().hasPending());

    NotificationQueue::instance().flush();

    TEST_ASSERT_EQUAL(1, callCountA);
}

void test_multiple_groups_queue_independently() {
    Signal<int> a{0};
    Signal<int> b{0};
    SignalWatcher watcher;

    watcher.watchAll([&]() { callCountA++; }, a);
    watcher.watchAll([&]() { callCountB++; }, b);

    NotificationQueue::instance().setDeferredMode(true);

    a.set(1);
    b.set(2);

    TEST_ASSERT_EQUAL(0, callCountA);
    TEST_ASSERT_EQUAL(0, callCountB);
    TEST_ASSERT_TRUE(NotificationQueue::instance().hasPending());

    NotificationQueue::instance().flush();

    TEST_ASSERT_EQUAL(1, callCountA);
    TEST_ASSERT_EQUAL(1, callCountB);
}

void test_watcher_destruction_unsubscribes_all_groups() {
    Signal<int> sig{0};

    {
        SignalWatcher watcher;
        watcher.watchAll([&]() { callCountA++; }, sig);
        sig.set(1);
        TEST_ASSERT_EQUAL(1, callCountA);
    }

    sig.set(2);
    TEST_ASSERT_EQUAL(1, callCountA);
}

void test_watcher_accepts_signal_string_and_signal_vector() {
    SignalString name{"Init"};
    SignalVector<int, 4> values;
    SignalWatcher watcher;

    watcher.watchAll([&]() { callCountA++; }, name, values);

    NotificationQueue::instance().setDeferredMode(true);

    name.set("Updated");
    values.set({1, 2, 3});

    TEST_ASSERT_EQUAL(0, callCountA);
    TEST_ASSERT_TRUE(NotificationQueue::instance().hasPending());

    NotificationQueue::instance().flush();

    TEST_ASSERT_EQUAL(1, callCountA);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_watch_all_coalesces_multiple_signals_into_single_callback);
    RUN_TEST(test_group_supports_incremental_binding_from_arrays);
    RUN_TEST(test_multiple_groups_queue_independently);
    RUN_TEST(test_watcher_destruction_unsubscribes_all_groups);
    RUN_TEST(test_watcher_accepts_signal_string_and_signal_vector);

    return UNITY_END();
}
