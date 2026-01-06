#include <unity.h>

#include <oc/state/DerivedSignal.hpp>
#include <oc/state/NotificationQueue.hpp>

#include <cstring>

using namespace oc::state;

// Test helpers
static int callCount = 0;
static float lastFloat = 0.0f;
static const char* lastString = nullptr;

void setUp() {
    callCount = 0;
    lastFloat = 0.0f;
    lastString = nullptr;
    // Use immediate mode for unit tests (bypass deferred queue)
    NotificationQueue::instance().setDeferredMode(false);
}

void tearDown() {
    // Restore deferred mode
    NotificationQueue::instance().setDeferredMode(true);
}

// =============================================================================
// DerivedSignal Tests
// =============================================================================

void test_derived_signal_initial_value() {
    Signal<int> source{100};
    DerivedSignal<int, float> derived{source, [](int v) {
        return v / 100.0f;
    }};
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, derived.get());
}

void test_derived_signal_updates_on_source_change() {
    Signal<int> source{0};
    DerivedSignal<int, float> derived{source, [](int v) {
        return v * 2.0f;
    }};

    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, derived.get());

    source.set(50);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 100.0f, derived.get());
}

void test_derived_signal_notifies_subscribers() {
    Signal<int> source{10};
    DerivedSignal<int, float> derived{source, [](int v) {
        return v * 0.1f;
    }};

    auto sub = derived.subscribe([](float v) {
        callCount++;
        lastFloat = v;
    });

    source.set(20);

    TEST_ASSERT_EQUAL(1, callCount);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, lastFloat);
}

void test_derived_signal_no_notification_if_same_value() {
    Signal<int> source{10};
    DerivedSignal<int, int> derived{source, [](int v) {
        return v / 10;  // 10 -> 1, 15 -> 1 (same output)
    }};

    auto sub = derived.subscribe([](int) {
        callCount++;
    });

    // Both should produce 1, so no notification
    source.set(15);
    TEST_ASSERT_EQUAL(0, callCount);

    // This should produce 2, so notification
    source.set(20);
    TEST_ASSERT_EQUAL(1, callCount);
}

void test_derived_signal_operator_call() {
    Signal<int> source{42};
    DerivedSignal<int, int> derived{source, [](int v) { return v * 2; }};
    TEST_ASSERT_EQUAL(84, derived());
}

void test_derived_signal_subscriber_count() {
    Signal<int> source{0};
    DerivedSignal<int, int> derived{source, [](int v) { return v; }};

    TEST_ASSERT_EQUAL(0, derived.subscriberCount());

    auto sub1 = derived.subscribe([](int) {});
    TEST_ASSERT_EQUAL(1, derived.subscriberCount());

    auto sub2 = derived.subscribe([](int) {});
    TEST_ASSERT_EQUAL(2, derived.subscriberCount());
}

// =============================================================================
// DerivedStringSignal Tests
// =============================================================================

void test_derived_string_initial_value() {
    Signal<float> source{0.5f};
    DerivedStringSignal<float, 8> derived{source, [](float v, char* buf, size_t size) {
        int cc = static_cast<int>(v * 127.0f);
        snprintf(buf, size, "%d", cc);
    }};

    TEST_ASSERT_EQUAL_STRING("63", derived.get());
}

void test_derived_string_updates_on_source_change() {
    Signal<float> source{0.0f};
    DerivedStringSignal<float, 8> derived{source, [](float v, char* buf, size_t size) {
        int cc = static_cast<int>(v * 127.0f);
        snprintf(buf, size, "%d", cc);
    }};

    TEST_ASSERT_EQUAL_STRING("0", derived.get());

    source.set(1.0f);
    TEST_ASSERT_EQUAL_STRING("127", derived.get());
}

void test_derived_string_notifies_subscribers() {
    Signal<int> source{10};
    DerivedStringSignal<int, 16> derived{source, [](int v, char* buf, size_t size) {
        snprintf(buf, size, "val=%d", v);
    }};

    auto sub = derived.subscribe([](const char* s) {
        callCount++;
        lastString = s;
    });

    source.set(20);

    TEST_ASSERT_EQUAL(1, callCount);
    TEST_ASSERT_EQUAL_STRING("val=20", lastString);
}

void test_derived_string_implicit_conversion() {
    Signal<int> source{42};
    DerivedStringSignal<int, 8> derived{source, [](int v, char* buf, size_t size) {
        snprintf(buf, size, "%d", v);
    }};

    const char* str = derived;  // Implicit conversion
    TEST_ASSERT_EQUAL_STRING("42", str);
}

void test_derived_string_max_length() {
    Signal<int> source{0};
    DerivedStringSignal<int, 8> derived{source, [](int, char*, size_t) {}};

    TEST_ASSERT_EQUAL(7, derived.maxLength());  // 8 - 1 for null terminator
}

void test_derived_string_subscriber_count() {
    Signal<int> source{0};
    DerivedStringSignal<int, 8> derived{source, [](int, char*, size_t) {}};

    TEST_ASSERT_EQUAL(0, derived.subscriberCount());

    auto sub = derived.subscribe([](const char*) {});
    TEST_ASSERT_EQUAL(1, derived.subscriberCount());
}

// =============================================================================
// Test Runner
// =============================================================================

int main() {
    UNITY_BEGIN();

    // DerivedSignal tests
    RUN_TEST(test_derived_signal_initial_value);
    RUN_TEST(test_derived_signal_updates_on_source_change);
    RUN_TEST(test_derived_signal_notifies_subscribers);
    RUN_TEST(test_derived_signal_no_notification_if_same_value);
    RUN_TEST(test_derived_signal_operator_call);
    RUN_TEST(test_derived_signal_subscriber_count);

    // DerivedStringSignal tests
    RUN_TEST(test_derived_string_initial_value);
    RUN_TEST(test_derived_string_updates_on_source_change);
    RUN_TEST(test_derived_string_notifies_subscribers);
    RUN_TEST(test_derived_string_implicit_conversion);
    RUN_TEST(test_derived_string_max_length);
    RUN_TEST(test_derived_string_subscriber_count);

    return UNITY_END();
}
