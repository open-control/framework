#include <unity.h>

#include <oc/state/FixedSubscriptionList.hpp>
#include <oc/state/NotificationQueue.hpp>

using namespace oc::state;

void setUp() {
    NotificationQueue::instance().setDeferredMode(false);
}

void tearDown() {
    NotificationQueue::instance().setDeferredMode(true);
}

void test_list_owns_and_clears_subscriptions() {
    Signal<int> first{0};
    Signal<int> second{0};
    FixedSubscriptionList<2> subscriptions;
    int callbackCount = 0;

    subscriptions.add(first.subscribe([&](int) { ++callbackCount; }));
    subscriptions.add(second.subscribe([&](int) { ++callbackCount; }));

    TEST_ASSERT_EQUAL_UINT32(2, subscriptions.size());
    TEST_ASSERT_TRUE(subscriptions.full());
    first.set(1);
    second.set(1);
    TEST_ASSERT_EQUAL_INT(2, callbackCount);

    subscriptions.clear();
    TEST_ASSERT_TRUE(subscriptions.empty());
    first.set(2);
    second.set(2);
    TEST_ASSERT_EQUAL_INT(2, callbackCount);
}

void test_try_add_rejects_invalid_or_excess_handles() {
    Signal<int> first{0};
    Signal<int> second{0};
    FixedSubscriptionList<1> subscriptions;

    TEST_ASSERT_FALSE(subscriptions.tryAdd(Subscription{}));
    TEST_ASSERT_TRUE(subscriptions.tryAdd(first.subscribe([](int) {})));
    TEST_ASSERT_FALSE(subscriptions.tryAdd(second.subscribe([](int) {})));
    TEST_ASSERT_EQUAL_UINT32(1, subscriptions.size());
    TEST_ASSERT_EQUAL_UINT32(0, second.subscriberCount());
}

void test_checked_list_reports_batch_failure_without_asserting() {
    Signal<int> first{0};
    Signal<int> second{0};
    CheckedSubscriptionList<1> subscriptions;

    subscriptions.push_back(first.subscribe([](int) {}));
    TEST_ASSERT_TRUE(subscriptions.valid());
    TEST_ASSERT_EQUAL_UINT32(1, subscriptions.size());

    subscriptions.push_back(second.subscribe([](int) {}));
    TEST_ASSERT_FALSE(subscriptions.valid());
    TEST_ASSERT_EQUAL_UINT32(1, subscriptions.size());
    TEST_ASSERT_EQUAL_UINT32(0, second.subscriberCount());

    subscriptions.clear();
    TEST_ASSERT_TRUE(subscriptions.valid());
    TEST_ASSERT_TRUE(subscriptions.empty());

    subscriptions.push_back(Subscription{});
    TEST_ASSERT_FALSE(subscriptions.valid());
    TEST_ASSERT_TRUE(subscriptions.empty());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_list_owns_and_clears_subscriptions);
    RUN_TEST(test_try_add_rejects_invalid_or_excess_handles);
    RUN_TEST(test_checked_list_reports_batch_failure_without_asserting);
    return UNITY_END();
}
