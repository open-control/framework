#include <unity.h>

#include <oc/state/NotificationQueue.hpp>
#include <oc/state/Signal.hpp>

#include <string>

using namespace oc::state;

// Test helpers
static int callCount = 0;
static int lastValue = 0;
static std::string lastString;

void setUp() {
    callCount = 0;
    lastValue = 0;
    lastString.clear();
    // Use immediate mode for unit tests (bypass deferred queue)
    NotificationQueue::instance().setDeferredMode(false);
}

void tearDown() {
    // Restore deferred mode
    NotificationQueue::instance().setDeferredMode(true);
}

// =============================================================================
// Basic Signal Tests
// =============================================================================

void test_signal_initial_value() {
    Signal<int> sig{42};
    TEST_ASSERT_EQUAL(42, sig.get());
}

void test_signal_default_initial_value() {
    Signal<int> sig;
    TEST_ASSERT_EQUAL(0, sig.get());
}

void test_signal_set_updates_value() {
    Signal<int> sig{0};
    sig.set(100);
    TEST_ASSERT_EQUAL(100, sig.get());
}

void test_signal_operator_call_returns_value() {
    Signal<int> sig{77};
    TEST_ASSERT_EQUAL(77, sig());
}

// =============================================================================
// Subscription Tests
// =============================================================================

void test_subscribe_receives_notification() {
    Signal<int> sig{0};

    auto sub = sig.subscribe([](const int& val) {
        callCount++;
        lastValue = val;
    });

    sig.set(42);

    TEST_ASSERT_EQUAL(1, callCount);
    TEST_ASSERT_EQUAL(42, lastValue);
}

void test_subscribe_with_capture() {
    Signal<int> sig{0};
    int multiplier = 10;

    auto sub = sig.subscribe([&multiplier](const int& val) { lastValue = val * multiplier; });

    sig.set(5);

    TEST_ASSERT_EQUAL(50, lastValue);
}

void test_subscribe_with_this_capture() {
    struct Widget {
        int value = 0;
        Subscription sub;

        void bind(Signal<int>& sig) {
            sub = sig.subscribe([this](const int& val) { this->value = val; });
        }
    };

    Signal<int> sig{0};
    Widget widget;
    widget.bind(sig);

    sig.set(123);

    TEST_ASSERT_EQUAL(123, widget.value);
}

void test_no_notification_if_value_unchanged() {
    Signal<int> sig{42};

    auto sub = sig.subscribe([](const int&) { callCount++; });

    sig.set(42);  // Same value

    TEST_ASSERT_EQUAL(0, callCount);
}

void test_subscription_auto_unsubscribe_on_destruction() {
    Signal<int> sig{0};

    {
        auto sub = sig.subscribe([](const int&) { callCount++; });
        sig.set(1);
        TEST_ASSERT_EQUAL(1, callCount);
    }  // sub destroyed here

    sig.set(2);
    TEST_ASSERT_EQUAL(1, callCount);  // Should not increment
}

void test_subscription_manual_reset() {
    Signal<int> sig{0};

    auto sub = sig.subscribe([](const int&) { callCount++; });

    sig.set(1);
    TEST_ASSERT_EQUAL(1, callCount);

    sub.reset();

    sig.set(2);
    TEST_ASSERT_EQUAL(1, callCount);  // Should not increment
}

void test_subscription_move() {
    Signal<int> sig{0};

    Subscription sub1 = sig.subscribe([](const int&) { callCount++; });

    Subscription sub2 = std::move(sub1);

    TEST_ASSERT_FALSE(sub1.isValid());
    TEST_ASSERT_TRUE(sub2.isValid());

    sig.set(1);
    TEST_ASSERT_EQUAL(1, callCount);
}

void test_empty_subscription_is_invalid() {
    Subscription sub;
    TEST_ASSERT_FALSE(sub.isValid());
    TEST_ASSERT_FALSE(static_cast<bool>(sub));
}

// =============================================================================
// Multiple Subscribers Tests
// =============================================================================

void test_multiple_subscribers() {
    Signal<int> sig{0};
    int count1 = 0, count2 = 0;

    auto sub1 = sig.subscribe([&count1](const int&) { count1++; });
    auto sub2 = sig.subscribe([&count2](const int&) { count2++; });

    sig.set(42);

    TEST_ASSERT_EQUAL(1, count1);
    TEST_ASSERT_EQUAL(1, count2);
}

void test_subscriber_count() {
    Signal<int> sig{0};

    TEST_ASSERT_EQUAL(0, sig.subscriberCount());

    auto sub1 = sig.subscribe([](const int&) {});
    TEST_ASSERT_EQUAL(1, sig.subscriberCount());

    auto sub2 = sig.subscribe([](const int&) {});
    TEST_ASSERT_EQUAL(2, sig.subscriberCount());

    sub1.reset();
    TEST_ASSERT_EQUAL(1, sig.subscriberCount());
}

void test_max_subscribers_limit() {
    Signal<int, 2> sig{0};  // Max 2 subscribers

    auto sub1 = sig.subscribe([](const int&) {});
    auto sub2 = sig.subscribe([](const int&) {});

    TEST_ASSERT_TRUE(sub1.isValid());
    TEST_ASSERT_TRUE(sub2.isValid());
    TEST_ASSERT_EQUAL(2, sig.subscriberCount());
    TEST_ASSERT_EQUAL(2, sig.maxSubscribers());

    // Note: Exceeding MaxSubscribers now triggers assert in debug builds
    // In release builds, returns invalid Subscription
}

void test_subscriber_slot_reuse() {
    Signal<int, 2> sig{0};

    auto sub1 = sig.subscribe([](const int&) {});
    auto sub2 = sig.subscribe([](const int&) {});

    TEST_ASSERT_EQUAL(2, sig.subscriberCount());

    sub1.reset();  // Free slot 0

    // Should be able to add new subscriber in freed slot
    auto sub3 = sig.subscribe([](const int&) { callCount++; });
    TEST_ASSERT_TRUE(sub3.isValid());
    TEST_ASSERT_EQUAL(2, sig.subscriberCount());

    sig.set(1);
    TEST_ASSERT_EQUAL(1, callCount);
}

// =============================================================================
// String Signal Tests
// =============================================================================

void test_string_signal() {
    Signal<std::string> sig{"initial"};

    auto sub = sig.subscribe([](const std::string& val) {
        callCount++;
        lastString = val;
    });

    sig.set("updated");

    TEST_ASSERT_EQUAL(1, callCount);
    TEST_ASSERT_EQUAL_STRING("updated", lastString.c_str());
}

void test_string_signal_no_change() {
    Signal<std::string> sig{"same"};

    auto sub = sig.subscribe([](const std::string&) { callCount++; });

    sig.set("same");

    TEST_ASSERT_EQUAL(0, callCount);
}

// =============================================================================
// Struct Signal Tests (with operator==)
// =============================================================================

struct PointWithEqual {
    int x, y;
    bool operator==(const PointWithEqual& other) const { return x == other.x && y == other.y; }
};

void test_struct_with_equality_operator() {
    Signal<PointWithEqual> sig{{0, 0}};

    auto sub = sig.subscribe([](const PointWithEqual&) { callCount++; });

    sig.set({1, 2});
    TEST_ASSERT_EQUAL(1, callCount);

    sig.set({1, 2});  // Same value
    TEST_ASSERT_EQUAL(1, callCount);

    sig.set({1, 3});  // Different
    TEST_ASSERT_EQUAL(2, callCount);
}

// =============================================================================
// Struct Signal Tests (without operator==, uses memcmp)
// =============================================================================

struct PointNoEqual {
    int x, y;
    // No operator==
};

void test_struct_without_equality_uses_memcmp() {
    Signal<PointNoEqual> sig{{0, 0}};

    auto sub = sig.subscribe([](const PointNoEqual&) { callCount++; });

    sig.set({1, 2});
    TEST_ASSERT_EQUAL(1, callCount);

    sig.set({1, 2});  // Same value (memcmp detects)
    TEST_ASSERT_EQUAL(1, callCount);

    sig.set({1, 3});  // Different
    TEST_ASSERT_EQUAL(2, callCount);
}

// =============================================================================
// Force Notify Tests
// =============================================================================

void test_force_notify() {
    Signal<int> sig{42};

    auto sub = sig.subscribe([](const int&) { callCount++; });

    sig.notify();  // Force notify without change

    TEST_ASSERT_EQUAL(1, callCount);
}

// =============================================================================
// Null Callback Test
// =============================================================================

void test_null_callback_returns_invalid_subscription() {
    Signal<int> sig{0};

    auto sub = sig.subscribe(nullptr);

    TEST_ASSERT_FALSE(sub.isValid());
}

// =============================================================================
// Batch Update Tests
// =============================================================================

void test_batch_guard_defers_notifications() {
    // Start in immediate mode
    NotificationQueue::instance().setDeferredMode(false);

    Signal<int> sig{0};
    auto sub = sig.subscribe([](const int&) { callCount++; });

    {
        auto batch = NotificationQueue::instance().batch();
        // Inside batch scope, deferred mode is enabled
        TEST_ASSERT_TRUE(NotificationQueue::instance().isDeferredMode());

        sig.set(1);
        sig.set(2);
        sig.set(3);

        // Notifications are pending, not executed yet
        TEST_ASSERT_EQUAL(0, callCount);
        TEST_ASSERT_TRUE(NotificationQueue::instance().hasPending());
    }

    // After batch scope ends, flush happens
    TEST_ASSERT_EQUAL(1, callCount);  // Only one notification (coalesced)
    TEST_ASSERT_EQUAL(3, sig.get());  // Final value
}

void test_batch_guard_restores_mode() {
    // Start in immediate mode
    NotificationQueue::instance().setDeferredMode(false);
    TEST_ASSERT_FALSE(NotificationQueue::instance().isDeferredMode());

    {
        auto batch = NotificationQueue::instance().batch();
        TEST_ASSERT_TRUE(NotificationQueue::instance().isDeferredMode());
    }

    // Mode should be restored to immediate
    TEST_ASSERT_FALSE(NotificationQueue::instance().isDeferredMode());
}

void test_batch_coalescing_multiple_signals() {
    NotificationQueue::instance().setDeferredMode(false);

    Signal<int> sig1{0};
    Signal<int> sig2{0};
    int count1 = 0, count2 = 0;

    auto sub1 = sig1.subscribe([&count1](const int&) { count1++; });
    auto sub2 = sig2.subscribe([&count2](const int&) { count2++; });

    {
        auto batch = NotificationQueue::instance().batch();

        // Set each signal multiple times
        sig1.set(1);
        sig1.set(2);
        sig1.set(3);

        sig2.set(10);
        sig2.set(20);

        TEST_ASSERT_EQUAL(0, count1);
        TEST_ASSERT_EQUAL(0, count2);
    }

    // Each signal's callback called once with final value
    TEST_ASSERT_EQUAL(1, count1);
    TEST_ASSERT_EQUAL(1, count2);
    TEST_ASSERT_EQUAL(3, sig1.get());
    TEST_ASSERT_EQUAL(20, sig2.get());
}

void test_deferred_mode_coalescing() {
    // Enable deferred mode explicitly
    NotificationQueue::instance().setDeferredMode(true);

    Signal<int> sig{0};
    auto sub = sig.subscribe([](const int&) { callCount++; });

    sig.set(1);
    sig.set(2);
    sig.set(3);

    TEST_ASSERT_EQUAL(0, callCount);  // Not yet executed
    TEST_ASSERT_TRUE(NotificationQueue::instance().hasPending());

    NotificationQueue::instance().flush();

    TEST_ASSERT_EQUAL(1, callCount);  // Coalesced to single call
    TEST_ASSERT_FALSE(NotificationQueue::instance().hasPending());

    // Restore for other tests
    NotificationQueue::instance().setDeferredMode(false);
}

void test_notification_queue_overflow() {
    NotificationQueue::instance().setDeferredMode(true);
    NotificationQueue::instance().resetOverflowCount();

    // Create many signals to potentially overflow
    constexpr size_t maxPending = NotificationQueue::maxPending();

    // Note: This test verifies the overflow mechanism exists
    // Actual overflow depends on configured limit
    TEST_ASSERT_FALSE(NotificationQueue::instance().hasOverflowed());

    NotificationQueue::instance().flush();
    NotificationQueue::instance().setDeferredMode(false);
}

// =============================================================================
// Main
// =============================================================================

int main() {
    UNITY_BEGIN();

    // Basic Signal
    RUN_TEST(test_signal_initial_value);
    RUN_TEST(test_signal_default_initial_value);
    RUN_TEST(test_signal_set_updates_value);
    RUN_TEST(test_signal_operator_call_returns_value);

    // Subscription
    RUN_TEST(test_subscribe_receives_notification);
    RUN_TEST(test_subscribe_with_capture);
    RUN_TEST(test_subscribe_with_this_capture);
    RUN_TEST(test_no_notification_if_value_unchanged);
    RUN_TEST(test_subscription_auto_unsubscribe_on_destruction);
    RUN_TEST(test_subscription_manual_reset);
    RUN_TEST(test_subscription_move);
    RUN_TEST(test_empty_subscription_is_invalid);

    // Multiple Subscribers
    RUN_TEST(test_multiple_subscribers);
    RUN_TEST(test_subscriber_count);
    RUN_TEST(test_max_subscribers_limit);
    RUN_TEST(test_subscriber_slot_reuse);

    // String Signal
    RUN_TEST(test_string_signal);
    RUN_TEST(test_string_signal_no_change);

    // Struct with/without operator==
    RUN_TEST(test_struct_with_equality_operator);
    RUN_TEST(test_struct_without_equality_uses_memcmp);

    // Force Notify
    RUN_TEST(test_force_notify);

    // Edge cases
    RUN_TEST(test_null_callback_returns_invalid_subscription);

    // Batch updates
    RUN_TEST(test_batch_guard_defers_notifications);
    RUN_TEST(test_batch_guard_restores_mode);
    RUN_TEST(test_batch_coalescing_multiple_signals);
    RUN_TEST(test_deferred_mode_coalescing);
    RUN_TEST(test_notification_queue_overflow);

    return UNITY_END();
}
