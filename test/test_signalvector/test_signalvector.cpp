#include <unity.h>

#include <oc/state/NotificationQueue.hpp>
#include <oc/state/SignalVector.hpp>

#include <string>

using namespace oc::state;

// Test helpers
static int callCount = 0;

void setUp() {
    callCount = 0;
    // Use immediate mode for unit tests (bypass deferred queue)
    NotificationQueue::instance().setDeferredMode(false);
}

void tearDown() {
    // Restore deferred mode
    NotificationQueue::instance().setDeferredMode(true);
}

// =============================================================================
// Basic Construction Tests
// =============================================================================

void test_default_construction_is_empty() {
    SignalVector<int, 8> vec;
    TEST_ASSERT_EQUAL(0, vec.size());
    TEST_ASSERT_TRUE(vec.empty());
}

void test_max_size_returns_template_param() {
    SignalVector<int, 16> vec;
    TEST_ASSERT_EQUAL(16, vec.maxSize());
}

// =============================================================================
// Set Tests
// =============================================================================

void test_set_from_array() {
    SignalVector<int, 8> vec;
    int values[] = {1, 2, 3, 4, 5};

    vec.set(values, 5);

    TEST_ASSERT_EQUAL(5, vec.size());
    TEST_ASSERT_EQUAL(1, vec[0]);
    TEST_ASSERT_EQUAL(2, vec[1]);
    TEST_ASSERT_EQUAL(3, vec[2]);
    TEST_ASSERT_EQUAL(4, vec[3]);
    TEST_ASSERT_EQUAL(5, vec[4]);
}

void test_set_from_initializer_list() {
    SignalVector<int, 8> vec;

    vec.set({10, 20, 30});

    TEST_ASSERT_EQUAL(3, vec.size());
    TEST_ASSERT_EQUAL(10, vec[0]);
    TEST_ASSERT_EQUAL(20, vec[1]);
    TEST_ASSERT_EQUAL(30, vec[2]);
}

void test_set_truncates_at_max_size() {
    SignalVector<int, 4> vec;
    int values[] = {1, 2, 3, 4, 5, 6, 7, 8};

    vec.set(values, 8);

    TEST_ASSERT_EQUAL(4, vec.size());  // Truncated to MaxN
    TEST_ASSERT_EQUAL(1, vec[0]);
    TEST_ASSERT_EQUAL(4, vec[3]);
}

void test_set_replaces_existing_content() {
    SignalVector<int, 8> vec;

    vec.set({1, 2, 3});
    TEST_ASSERT_EQUAL(3, vec.size());

    vec.set({10, 20});
    TEST_ASSERT_EQUAL(2, vec.size());
    TEST_ASSERT_EQUAL(10, vec[0]);
    TEST_ASSERT_EQUAL(20, vec[1]);
}

// =============================================================================
// Clear Tests
// =============================================================================

void test_clear_empties_vector() {
    SignalVector<int, 8> vec;
    vec.set({1, 2, 3});

    vec.clear();

    TEST_ASSERT_EQUAL(0, vec.size());
    TEST_ASSERT_TRUE(vec.empty());
}

void test_clear_on_empty_does_not_notify() {
    SignalVector<int, 8> vec;

    auto sub = vec.subscribe([]() { callCount++; });

    vec.clear();  // Already empty

    TEST_ASSERT_EQUAL(0, callCount);
}

// =============================================================================
// Accessor Tests
// =============================================================================

void test_data_returns_pointer_to_array() {
    SignalVector<int, 8> vec;
    vec.set({5, 10, 15});

    const int* ptr = vec.data();

    TEST_ASSERT_EQUAL(5, ptr[0]);
    TEST_ASSERT_EQUAL(10, ptr[1]);
    TEST_ASSERT_EQUAL(15, ptr[2]);
}

void test_begin_end_iteration() {
    SignalVector<int, 8> vec;
    vec.set({1, 2, 3});

    int sum = 0;
    for (const auto& val : vec) {
        sum += val;
    }

    TEST_ASSERT_EQUAL(6, sum);
}

void test_begin_equals_end_when_empty() {
    SignalVector<int, 8> vec;
    TEST_ASSERT_EQUAL_PTR(vec.begin(), vec.end());
}

// =============================================================================
// Subscription Tests
// =============================================================================

void test_subscribe_receives_notification_on_set() {
    SignalVector<int, 8> vec;

    auto sub = vec.subscribe([]() { callCount++; });

    vec.set({1, 2, 3});

    TEST_ASSERT_EQUAL(1, callCount);
}

void test_subscribe_receives_notification_on_clear() {
    SignalVector<int, 8> vec;
    vec.set({1, 2, 3});

    auto sub = vec.subscribe([]() { callCount++; });

    vec.clear();

    TEST_ASSERT_EQUAL(1, callCount);
}

void test_no_notification_if_content_unchanged() {
    SignalVector<int, 8> vec;
    vec.set({1, 2, 3});

    auto sub = vec.subscribe([]() { callCount++; });

    vec.set({1, 2, 3});  // Same content

    TEST_ASSERT_EQUAL(0, callCount);
}

void test_notification_on_size_change() {
    SignalVector<int, 8> vec;
    vec.set({1, 2, 3});

    auto sub = vec.subscribe([]() { callCount++; });

    vec.set({1, 2});  // Different size, same prefix

    TEST_ASSERT_EQUAL(1, callCount);
}

void test_notification_on_content_change() {
    SignalVector<int, 8> vec;
    vec.set({1, 2, 3});

    auto sub = vec.subscribe([]() { callCount++; });

    vec.set({1, 2, 99});  // Same size, different content

    TEST_ASSERT_EQUAL(1, callCount);
}

void test_subscribe_and_invoke() {
    SignalVector<int, 8> vec;
    vec.set({1, 2, 3});

    auto sub = vec.subscribeAndInvoke([]() { callCount++; });

    // Should have been called immediately
    TEST_ASSERT_EQUAL(1, callCount);

    // And on subsequent changes
    vec.set({4, 5});
    TEST_ASSERT_EQUAL(2, callCount);
}

void test_auto_unsubscribe_on_destruction() {
    SignalVector<int, 8> vec;

    {
        auto sub = vec.subscribe([]() { callCount++; });
        vec.set({1});
        TEST_ASSERT_EQUAL(1, callCount);
    }  // sub destroyed

    vec.set({2});
    TEST_ASSERT_EQUAL(1, callCount);  // Should not increment
}

void test_multiple_subscribers() {
    SignalVector<int, 8> vec;
    int count1 = 0, count2 = 0;

    auto sub1 = vec.subscribe([&count1]() { count1++; });
    auto sub2 = vec.subscribe([&count2]() { count2++; });

    vec.set({1, 2, 3});

    TEST_ASSERT_EQUAL(1, count1);
    TEST_ASSERT_EQUAL(1, count2);
}

void test_subscriber_count() {
    SignalVector<int, 8> vec;

    TEST_ASSERT_EQUAL(0, vec.subscriberCount());

    auto sub1 = vec.subscribe([]() {});
    TEST_ASSERT_EQUAL(1, vec.subscriberCount());

    auto sub2 = vec.subscribe([]() {});
    TEST_ASSERT_EQUAL(2, vec.subscriberCount());

    sub1.reset();
    TEST_ASSERT_EQUAL(1, vec.subscriberCount());
}

// =============================================================================
// Force Notify Tests
// =============================================================================

void test_force_notify() {
    SignalVector<int, 8> vec;
    vec.set({1, 2, 3});

    auto sub = vec.subscribe([]() { callCount++; });

    vec.notify();  // Force notify without change

    TEST_ASSERT_EQUAL(1, callCount);
}

// =============================================================================
// Capacity Tests
// =============================================================================

void test_max_subscribers() {
    SignalVector<int, 8> vec;
    TEST_ASSERT_EQUAL(4, vec.maxSubscribers());  // Default

    SignalVector<int, 8, 8> vec8;
    TEST_ASSERT_EQUAL(8, vec8.maxSubscribers());
}

// =============================================================================
// String Element Tests
// =============================================================================

void test_string_elements() {
    SignalVector<std::string, 4> vec;

    vec.set({"Alpha", "Beta", "Gamma"});

    TEST_ASSERT_EQUAL(3, vec.size());
    TEST_ASSERT_EQUAL_STRING("Alpha", vec[0].c_str());
    TEST_ASSERT_EQUAL_STRING("Beta", vec[1].c_str());
    TEST_ASSERT_EQUAL_STRING("Gamma", vec[2].c_str());
}

void test_string_content_comparison() {
    SignalVector<std::string, 4> vec;
    vec.set({"A", "B", "C"});

    auto sub = vec.subscribe([]() { callCount++; });

    // Set same content from different strings
    std::string a = "A", b = "B", c = "C";
    const std::string arr[] = {a, b, c};
    vec.set(arr, 3);

    TEST_ASSERT_EQUAL(0, callCount);  // Should NOT notify
}

// =============================================================================
// Edge Cases
// =============================================================================

void test_set_empty_array() {
    SignalVector<int, 8> vec;
    vec.set({1, 2, 3});

    auto sub = vec.subscribe([]() { callCount++; });

    int empty[] = {};
    vec.set(empty, 0);

    TEST_ASSERT_EQUAL(0, vec.size());
    TEST_ASSERT_EQUAL(1, callCount);
}

void test_set_single_element() {
    SignalVector<int, 8> vec;

    vec.set({42});

    TEST_ASSERT_EQUAL(1, vec.size());
    TEST_ASSERT_EQUAL(42, vec[0]);
}

void test_set_max_elements() {
    SignalVector<int, 4> vec;

    vec.set({1, 2, 3, 4});

    TEST_ASSERT_EQUAL(4, vec.size());
    TEST_ASSERT_EQUAL(vec.maxSize(), vec.size());
}

// =============================================================================
// Main
// =============================================================================

int main() {
    UNITY_BEGIN();

    // Basic Construction
    RUN_TEST(test_default_construction_is_empty);
    RUN_TEST(test_max_size_returns_template_param);

    // Set
    RUN_TEST(test_set_from_array);
    RUN_TEST(test_set_from_initializer_list);
    RUN_TEST(test_set_truncates_at_max_size);
    RUN_TEST(test_set_replaces_existing_content);

    // Clear
    RUN_TEST(test_clear_empties_vector);
    RUN_TEST(test_clear_on_empty_does_not_notify);

    // Accessors
    RUN_TEST(test_data_returns_pointer_to_array);
    RUN_TEST(test_begin_end_iteration);
    RUN_TEST(test_begin_equals_end_when_empty);

    // Subscription
    RUN_TEST(test_subscribe_receives_notification_on_set);
    RUN_TEST(test_subscribe_receives_notification_on_clear);
    RUN_TEST(test_no_notification_if_content_unchanged);
    RUN_TEST(test_notification_on_size_change);
    RUN_TEST(test_notification_on_content_change);
    RUN_TEST(test_subscribe_and_invoke);
    RUN_TEST(test_auto_unsubscribe_on_destruction);
    RUN_TEST(test_multiple_subscribers);
    RUN_TEST(test_subscriber_count);

    // Force Notify
    RUN_TEST(test_force_notify);

    // Capacity
    RUN_TEST(test_max_subscribers);

    // String Elements
    RUN_TEST(test_string_elements);
    RUN_TEST(test_string_content_comparison);

    // Edge Cases
    RUN_TEST(test_set_empty_array);
    RUN_TEST(test_set_single_element);
    RUN_TEST(test_set_max_elements);

    return UNITY_END();
}
