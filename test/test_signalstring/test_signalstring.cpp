#include <unity.h>

#include <oc/state/SignalString.hpp>

#include <string>
#include <vector>

using namespace oc::state;

// Test helpers
static int callCount = 0;
static const char* lastValue = nullptr;

void setUp() {
    callCount = 0;
    lastValue = nullptr;
}

void tearDown() {}

// =============================================================================
// Basic Construction Tests
// =============================================================================

void test_default_construction_is_empty() {
    SignalString sig;
    TEST_ASSERT_EQUAL_STRING("", sig.get());
    TEST_ASSERT_TRUE(sig.empty());
    TEST_ASSERT_EQUAL(0, sig.length());
}

void test_construction_with_cstring() {
    SignalString sig{"Hello"};
    TEST_ASSERT_EQUAL_STRING("Hello", sig.get());
    TEST_ASSERT_FALSE(sig.empty());
    TEST_ASSERT_EQUAL(5, sig.length());
}

void test_construction_with_std_string() {
    std::string init = "World";
    SignalString sig{init};
    TEST_ASSERT_EQUAL_STRING("World", sig.get());
}

void test_implicit_conversion_to_const_char() {
    SignalString sig{"Test"};
    const char* ptr = sig;  // Implicit conversion
    TEST_ASSERT_EQUAL_STRING("Test", ptr);
}

// =============================================================================
// Set Tests
// =============================================================================

void test_set_from_cstring() {
    SignalString sig;
    sig.set("Updated");
    TEST_ASSERT_EQUAL_STRING("Updated", sig.get());
}

void test_set_from_std_string() {
    SignalString sig;
    std::string value = "FromStdString";
    sig.set(value);
    TEST_ASSERT_EQUAL_STRING("FromStdString", sig.get());
}

void test_set_nullptr_is_ignored() {
    SignalString sig{"Initial"};
    sig.set(static_cast<const char*>(nullptr));
    TEST_ASSERT_EQUAL_STRING("Initial", sig.get());
}

void test_set_empty_string() {
    SignalString sig{"NotEmpty"};
    sig.set("");
    TEST_ASSERT_EQUAL_STRING("", sig.get());
    TEST_ASSERT_TRUE(sig.empty());
}

// =============================================================================
// Truncation Tests
// =============================================================================

void test_truncation_at_capacity() {
    SignalStringBase<8> sig;  // 7 chars max + null
    sig.set("1234567890");    // 10 chars
    TEST_ASSERT_EQUAL_STRING("1234567", sig.get());
    TEST_ASSERT_EQUAL(7, sig.length());
}

void test_truncation_preserves_null_terminator() {
    SignalStringBase<4> sig;
    sig.set("ABCDEFGH");
    TEST_ASSERT_EQUAL(3, sig.length());
    TEST_ASSERT_EQUAL('\0', sig.get()[3]);
}

// =============================================================================
// Subscription Tests
// =============================================================================

void test_subscribe_receives_notification_on_change() {
    SignalString sig;

    auto sub = sig.subscribe([](const char* val) {
        callCount++;
        lastValue = val;
    });

    sig.set("Changed");

    TEST_ASSERT_EQUAL(1, callCount);
    TEST_ASSERT_EQUAL_STRING("Changed", lastValue);
}

void test_no_notification_if_value_unchanged() {
    SignalString sig{"Same"};

    auto sub = sig.subscribe([](const char*) { callCount++; });

    sig.set("Same");  // Same value

    TEST_ASSERT_EQUAL(0, callCount);
}

void test_notification_on_content_change_not_pointer() {
    SignalString sig{"AAA"};

    auto sub = sig.subscribe([](const char*) { callCount++; });

    // Different string with same content from different source
    std::string different = "AAA";
    sig.set(different);

    TEST_ASSERT_EQUAL(0, callCount);  // Should NOT notify (content same)
}

void test_subscribe_and_invoke() {
    SignalString sig{"Initial"};

    auto sub = sig.subscribeAndInvoke([](const char* val) {
        callCount++;
        lastValue = val;
    });

    // Should have been called immediately
    TEST_ASSERT_EQUAL(1, callCount);
    TEST_ASSERT_EQUAL_STRING("Initial", lastValue);

    // And on subsequent changes
    sig.set("Updated");
    TEST_ASSERT_EQUAL(2, callCount);
    TEST_ASSERT_EQUAL_STRING("Updated", lastValue);
}

void test_auto_unsubscribe_on_destruction() {
    SignalString sig;

    {
        auto sub = sig.subscribe([](const char*) { callCount++; });
        sig.set("First");
        TEST_ASSERT_EQUAL(1, callCount);
    }  // sub destroyed

    sig.set("Second");
    TEST_ASSERT_EQUAL(1, callCount);  // Should not increment
}

void test_multiple_subscribers() {
    SignalString sig;
    int count1 = 0, count2 = 0;

    auto sub1 = sig.subscribe([&count1](const char*) { count1++; });
    auto sub2 = sig.subscribe([&count2](const char*) { count2++; });

    sig.set("Value");

    TEST_ASSERT_EQUAL(1, count1);
    TEST_ASSERT_EQUAL(1, count2);
}

void test_subscriber_count() {
    SignalString sig;

    TEST_ASSERT_EQUAL(0, sig.subscriberCount());

    auto sub1 = sig.subscribe([](const char*) {});
    TEST_ASSERT_EQUAL(1, sig.subscriberCount());

    auto sub2 = sig.subscribe([](const char*) {});
    TEST_ASSERT_EQUAL(2, sig.subscriberCount());

    sub1.reset();
    TEST_ASSERT_EQUAL(1, sig.subscriberCount());
}

// =============================================================================
// Force Notify Tests
// =============================================================================

void test_force_notify() {
    SignalString sig{"Value"};

    auto sub = sig.subscribe([](const char*) { callCount++; });

    sig.notify();  // Force notify without change

    TEST_ASSERT_EQUAL(1, callCount);
}

// =============================================================================
// Capacity Tests
// =============================================================================

void test_capacity_and_max_length() {
    SignalString sig;
    TEST_ASSERT_EQUAL(128, sig.capacity());
    TEST_ASSERT_EQUAL(127, sig.maxLength());

    SignalLabel label;
    TEST_ASSERT_EQUAL(32, label.capacity());
    TEST_ASSERT_EQUAL(31, label.maxLength());

    SignalText text;
    TEST_ASSERT_EQUAL(256, text.capacity());
    TEST_ASSERT_EQUAL(255, text.maxLength());

    SignalTiny tiny;
    TEST_ASSERT_EQUAL(16, tiny.capacity());
    TEST_ASSERT_EQUAL(15, tiny.maxLength());
}

void test_max_subscribers() {
    SignalString sig;
    TEST_ASSERT_EQUAL(4, sig.maxSubscribers());

    SignalStringBase<32, 8> custom;
    TEST_ASSERT_EQUAL(8, custom.maxSubscribers());
}

// =============================================================================
// Pointer Stability Tests
// =============================================================================

void test_pointer_remains_stable_across_changes() {
    SignalString sig{"Initial"};
    const char* ptr1 = sig.get();

    sig.set("Different");
    const char* ptr2 = sig.get();

    sig.set("Another");
    const char* ptr3 = sig.get();

    // All pointers should be the same (buffer address doesn't change)
    TEST_ASSERT_EQUAL_PTR(ptr1, ptr2);
    TEST_ASSERT_EQUAL_PTR(ptr2, ptr3);
}

// =============================================================================
// Alias Types Tests
// =============================================================================

void test_signal_string_aliases_exist() {
    // These should compile
    SignalString str;
    SignalText text;
    SignalLabel label;
    SignalTiny tiny;

    str.set("String");
    text.set("Text");
    label.set("Label");
    tiny.set("Tiny");

    TEST_ASSERT_EQUAL_STRING("String", str.get());
    TEST_ASSERT_EQUAL_STRING("Text", text.get());
    TEST_ASSERT_EQUAL_STRING("Label", label.get());
    TEST_ASSERT_EQUAL_STRING("Tiny", tiny.get());
}

// =============================================================================
// Main
// =============================================================================

int main() {
    UNITY_BEGIN();

    // Basic Construction
    RUN_TEST(test_default_construction_is_empty);
    RUN_TEST(test_construction_with_cstring);
    RUN_TEST(test_construction_with_std_string);
    RUN_TEST(test_implicit_conversion_to_const_char);

    // Set
    RUN_TEST(test_set_from_cstring);
    RUN_TEST(test_set_from_std_string);
    RUN_TEST(test_set_nullptr_is_ignored);
    RUN_TEST(test_set_empty_string);

    // Truncation
    RUN_TEST(test_truncation_at_capacity);
    RUN_TEST(test_truncation_preserves_null_terminator);

    // Subscription
    RUN_TEST(test_subscribe_receives_notification_on_change);
    RUN_TEST(test_no_notification_if_value_unchanged);
    RUN_TEST(test_notification_on_content_change_not_pointer);
    RUN_TEST(test_subscribe_and_invoke);
    RUN_TEST(test_auto_unsubscribe_on_destruction);
    RUN_TEST(test_multiple_subscribers);
    RUN_TEST(test_subscriber_count);

    // Force Notify
    RUN_TEST(test_force_notify);

    // Capacity
    RUN_TEST(test_capacity_and_max_length);
    RUN_TEST(test_max_subscribers);

    // Pointer Stability
    RUN_TEST(test_pointer_remains_stable_across_changes);

    // Aliases
    RUN_TEST(test_signal_string_aliases_exist);

    return UNITY_END();
}
