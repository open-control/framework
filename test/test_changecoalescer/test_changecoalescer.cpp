#include <unity.h>

#include <oc/state/ChangeCoalescer.hpp>
#include <oc/state/NotificationQueue.hpp>
#include <oc/time/Time.hpp>

using namespace oc::state;

namespace {

uint32_t mockTime = 0;
int actionCount = 0;

uint32_t getMockTime() {
    return mockTime;
}

void resetTestState() {
    mockTime = 100;
    actionCount = 0;
}

}  // namespace

void setUp() {
    resetTestState();
    oc::time::setProvider(getMockTime);
    NotificationQueue::instance().setDeferredMode(false);
}

void tearDown() {
    NotificationQueue::instance().setDeferredMode(true);
}

void test_no_action_before_window_expires() {
    ChangeCoalescer<1> coalescer{[]() { ++actionCount; }, 1000};
    Signal<float> signal{0.0f};
    coalescer.watch(signal);

    signal.set(1.0f);
    mockTime = 1099;
    coalescer.update();

    TEST_ASSERT_EQUAL(0, actionCount);
    TEST_ASSERT_TRUE(coalescer.hasPendingChanges());
}

void test_action_runs_after_window_expires() {
    ChangeCoalescer<1> coalescer{[]() { ++actionCount; }, 1000};
    Signal<float> signal{0.0f};
    coalescer.watch(signal);

    signal.set(1.0f);
    mockTime = 1100;
    coalescer.update();

    TEST_ASSERT_EQUAL(1, actionCount);
    TEST_ASSERT_FALSE(coalescer.hasPendingChanges());
}

void test_flush_runs_pending_action_immediately() {
    ChangeCoalescer<> coalescer{[]() { ++actionCount; }, 1000};
    coalescer.markChanged();

    coalescer.flush();

    TEST_ASSERT_EQUAL(1, actionCount);
    TEST_ASSERT_FALSE(coalescer.hasPendingChanges());
}

void test_multiple_signals_coalesce_into_one_action() {
    ChangeCoalescer<3> coalescer{[]() { ++actionCount; }, 1000};
    Signal<float> first{0.0f};
    Signal<float> second{0.0f};
    Signal<float> third{0.0f};
    coalescer.watch(first);
    coalescer.watch(second);
    coalescer.watch(third);

    first.set(1.0f);
    second.set(2.0f);
    third.set(3.0f);
    mockTime = 1100;
    coalescer.update();

    TEST_ASSERT_EQUAL(1, actionCount);
    TEST_ASSERT_EQUAL(3, coalescer.subscriptionCount());
}

void test_window_is_measured_from_first_change() {
    ChangeCoalescer<> coalescer{[]() { ++actionCount; }, 1000};

    mockTime = 100;
    coalescer.markChanged();
    mockTime = 900;
    coalescer.markChanged();
    mockTime = 1100;
    coalescer.update();

    TEST_ASSERT_EQUAL(1, actionCount);
}

void test_explicit_change_requires_no_subscription() {
    ChangeCoalescer<> coalescer{[]() { ++actionCount; }, 1000};

    coalescer.markChanged();
    TEST_ASSERT_TRUE(coalescer.hasPendingChanges());

    mockTime = 1100;
    coalescer.update();
    TEST_ASSERT_EQUAL(1, actionCount);
}

void test_interval_can_be_changed() {
    ChangeCoalescer<> coalescer{[]() { ++actionCount; }, 500};
    TEST_ASSERT_EQUAL(500, coalescer.coalesceMs());

    coalescer.setCoalesceMs(2000);
    TEST_ASSERT_EQUAL(2000, coalescer.coalesceMs());
}

void test_change_raised_by_action_starts_new_window() {
    ChangeCoalescer<>* target = nullptr;
    ChangeCoalescer<> coalescer{
        [&]() {
            ++actionCount;
            if (actionCount == 1) {
                target->markChanged();
            }
        },
        1000
    };
    target = &coalescer;

    coalescer.markChanged();
    mockTime = 1100;
    coalescer.update();
    TEST_ASSERT_EQUAL(1, actionCount);
    TEST_ASSERT_TRUE(coalescer.hasPendingChanges());

    mockTime = 2100;
    coalescer.update();
    TEST_ASSERT_EQUAL(2, actionCount);
    TEST_ASSERT_FALSE(coalescer.hasPendingChanges());
}

void test_watch_reports_capacity_overflow_without_subscribing() {
    ChangeCoalescer<1> coalescer{[]() { ++actionCount; }, 1000};
    Signal<float> first{0.0f};
    Signal<float> second{0.0f};

    TEST_ASSERT_TRUE(coalescer.watch(first));
    TEST_ASSERT_FALSE(coalescer.watch(second));
    TEST_ASSERT_FALSE(coalescer.valid());
    TEST_ASSERT_EQUAL(1, coalescer.subscriptionCount());
    TEST_ASSERT_EQUAL(0, second.subscriberCount());
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_no_action_before_window_expires);
    RUN_TEST(test_action_runs_after_window_expires);
    RUN_TEST(test_flush_runs_pending_action_immediately);
    RUN_TEST(test_multiple_signals_coalesce_into_one_action);
    RUN_TEST(test_window_is_measured_from_first_change);
    RUN_TEST(test_explicit_change_requires_no_subscription);
    RUN_TEST(test_interval_can_be_changed);
    RUN_TEST(test_change_raised_by_action_starts_new_window);
    RUN_TEST(test_watch_reports_capacity_overflow_without_subscribing);

    return UNITY_END();
}
