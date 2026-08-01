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

void test_consume_cancels_only_watched_pending_notifications() {
    NotificationQueue::instance().setDeferredMode(true);
    ChangeCoalescer<1> coalescer{[]() { ++actionCount; }, 1000};
    Signal<float> watched{0.0f};
    Signal<float> unrelated{0.0f};
    int unrelatedCount = 0;
    TEST_ASSERT_TRUE(coalescer.watch(watched));
    auto unrelatedSubscription = unrelated.subscribe(
        [&](const float&) { ++unrelatedCount; }
    );
    TEST_ASSERT_TRUE(unrelatedSubscription.isValid());

    watched.set(1.0f);
    unrelated.set(1.0f);
    coalescer.consumePendingChangesWithoutAction();
    NotificationQueue::instance().flush();

    TEST_ASSERT_EQUAL(0, actionCount);
    TEST_ASSERT_FALSE(coalescer.hasPendingChanges());
    TEST_ASSERT_EQUAL(1, unrelatedCount);
}

void test_consume_cancels_watched_callback_later_in_active_wave() {
    NotificationQueue::instance().setDeferredMode(true);
    ChangeCoalescer<1> coalescer{[]() { ++actionCount; }, 1000};
    Signal<float> watched{0.0f};
    auto consumingSubscription = watched.subscribe(
        [&](const float&) { coalescer.consumePendingChangesWithoutAction(); }
    );
    TEST_ASSERT_TRUE(consumingSubscription.isValid());
    TEST_ASSERT_TRUE(coalescer.watch(watched));

    watched.set(1.0f);
    NotificationQueue::instance().flush();

    TEST_ASSERT_EQUAL(0, actionCount);
    TEST_ASSERT_FALSE(coalescer.hasPendingChanges());
}

void test_consume_clears_an_already_armed_change() {
    NotificationQueue::instance().setDeferredMode(true);
    ChangeCoalescer<1> coalescer{[]() { ++actionCount; }, 1000};
    Signal<float> watched{0.0f};
    TEST_ASSERT_TRUE(coalescer.watch(watched));

    watched.set(1.0f);
    NotificationQueue::instance().flush();
    TEST_ASSERT_TRUE(coalescer.hasPendingChanges());

    coalescer.consumePendingChangesWithoutAction();
    coalescer.flush();

    TEST_ASSERT_EQUAL(0, actionCount);
    TEST_ASSERT_FALSE(coalescer.hasPendingChanges());
}

void test_consume_retains_subscriptions_and_restarts_the_window() {
    NotificationQueue::instance().setDeferredMode(true);
    ChangeCoalescer<1> coalescer{[]() { ++actionCount; }, 1000};
    Signal<float> watched{0.0f};
    TEST_ASSERT_TRUE(coalescer.watch(watched));

    watched.set(1.0f);
    NotificationQueue::instance().flush();
    TEST_ASSERT_TRUE(coalescer.hasPendingChanges());
    coalescer.consumePendingChangesWithoutAction();

    mockTime = 400;
    watched.set(2.0f);
    NotificationQueue::instance().flush();
    TEST_ASSERT_TRUE(coalescer.hasPendingChanges());

    mockTime = 1399;
    coalescer.update();
    TEST_ASSERT_EQUAL(0, actionCount);
    mockTime = 1400;
    coalescer.update();

    TEST_ASSERT_EQUAL(1, actionCount);
    TEST_ASSERT_FALSE(coalescer.hasPendingChanges());
    TEST_ASSERT_EQUAL(1, coalescer.subscriptionCount());
}

void test_consume_preserves_a_later_subscriber_on_the_same_signal() {
    NotificationQueue::instance().setDeferredMode(true);
    ChangeCoalescer<1> coalescer{[]() { ++actionCount; }, 1000};
    Signal<float> watched{0.0f};
    Signal<float> trigger{0.0f};
    int foreignCount = 0;
    TEST_ASSERT_TRUE(coalescer.watch(watched));
    auto foreignSubscription = watched.subscribe(
        [&](const float&) { ++foreignCount; }
    );
    auto consumingSubscription = trigger.subscribe(
        [&](const float&) { coalescer.consumePendingChangesWithoutAction(); }
    );
    TEST_ASSERT_TRUE(foreignSubscription.isValid());
    TEST_ASSERT_TRUE(consumingSubscription.isValid());

    trigger.set(1.0f);
    watched.set(1.0f);
    NotificationQueue::instance().flush();

    TEST_ASSERT_EQUAL(0, actionCount);
    TEST_ASSERT_FALSE(coalescer.hasPendingChanges());
    TEST_ASSERT_EQUAL(1, foreignCount);
}

void test_consuming_one_coalescer_preserves_another_on_the_same_signal() {
    NotificationQueue::instance().setDeferredMode(true);
    int firstActionCount = 0;
    int secondActionCount = 0;
    ChangeCoalescer<1> first{[&]() { ++firstActionCount; }, 1000};
    ChangeCoalescer<1> second{[&]() { ++secondActionCount; }, 1000};
    Signal<float> watched{0.0f};
    TEST_ASSERT_TRUE(first.watch(watched));
    TEST_ASSERT_TRUE(second.watch(watched));

    watched.set(1.0f);
    first.consumePendingChangesWithoutAction();
    NotificationQueue::instance().flush();
    first.flush();
    second.flush();

    TEST_ASSERT_EQUAL(0, firstActionCount);
    TEST_ASSERT_EQUAL(1, secondActionCount);
    TEST_ASSERT_FALSE(first.hasPendingChanges());
    TEST_ASSERT_FALSE(second.hasPendingChanges());
}

void test_consume_cancels_processing_and_requeued_watched_callbacks() {
    NotificationQueue::instance().setDeferredMode(true);
    ChangeCoalescer<1> coalescer{[]() { ++actionCount; }, 1000};
    Signal<float> watched{0.0f};
    int foreignCount = 0;
    bool requeued = false;
    auto requeueingSubscription = watched.subscribe([&](const float&) {
        ++foreignCount;
        if (!requeued) {
            requeued = true;
            watched.notify();
            coalescer.consumePendingChangesWithoutAction();
        }
    });
    TEST_ASSERT_TRUE(requeueingSubscription.isValid());
    TEST_ASSERT_TRUE(coalescer.watch(watched));

    watched.set(1.0f);
    NotificationQueue::instance().flush();
    coalescer.flush();

    TEST_ASSERT_EQUAL(0, actionCount);
    TEST_ASSERT_FALSE(coalescer.hasPendingChanges());
    TEST_ASSERT_EQUAL(2, foreignCount);
}

void test_subscription_reset_cancels_before_slot_reuse() {
    NotificationQueue::instance().setDeferredMode(true);
    Signal<float, 1> watched{0.0f};
    int replacementCount = 0;
    auto original = watched.subscribe([](const float&) {});
    TEST_ASSERT_TRUE(original.isValid());

    watched.set(1.0f);
    original.reset();
    auto replacement = watched.subscribe(
        [&](const float&) { ++replacementCount; }
    );
    TEST_ASSERT_TRUE(replacement.isValid());
    NotificationQueue::instance().flush();

    TEST_ASSERT_EQUAL(0, replacementCount);
}

void test_coalescer_destruction_cancels_before_slot_reuse() {
    NotificationQueue::instance().setDeferredMode(true);
    Signal<float, 1> watched{0.0f};
    int replacementCount = 0;
    {
        ChangeCoalescer<1> coalescer{[]() { ++actionCount; }, 1000};
        TEST_ASSERT_TRUE(coalescer.watch(watched));
        watched.set(1.0f);
    }

    auto replacement = watched.subscribe(
        [&](const float&) { ++replacementCount; }
    );
    TEST_ASSERT_TRUE(replacement.isValid());
    NotificationQueue::instance().flush();

    TEST_ASSERT_EQUAL(0, actionCount);
    TEST_ASSERT_EQUAL(0, replacementCount);
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
    RUN_TEST(test_consume_cancels_only_watched_pending_notifications);
    RUN_TEST(test_consume_cancels_watched_callback_later_in_active_wave);
    RUN_TEST(test_consume_clears_an_already_armed_change);
    RUN_TEST(test_consume_retains_subscriptions_and_restarts_the_window);
    RUN_TEST(test_consume_preserves_a_later_subscriber_on_the_same_signal);
    RUN_TEST(test_consuming_one_coalescer_preserves_another_on_the_same_signal);
    RUN_TEST(test_consume_cancels_processing_and_requeued_watched_callbacks);
    RUN_TEST(test_subscription_reset_cancels_before_slot_reuse);
    RUN_TEST(test_coalescer_destruction_cancels_before_slot_reuse);

    return UNITY_END();
}
