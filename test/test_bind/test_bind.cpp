#include <unity.h>

#include <oc/state/Bind.hpp>
#include <oc/state/NotificationQueue.hpp>
#include <oc/state/Signal.hpp>
#include <oc/state/SignalString.hpp>

#include <array>
#include <vector>

using namespace oc::state;

void setUp() {
    // Use immediate mode for unit tests (bypass deferred queue)
    NotificationQueue::instance().setDeferredMode(false);
}

void tearDown() {
    // Restore deferred mode
    NotificationQueue::instance().setDeferredMode(true);
}

// =============================================================================
// Basic Bind Tests
// =============================================================================

void test_bind_single_signal() {
    Signal<int> sig{0};
    std::vector<Subscription> subs;
    int received = -1;

    bind(subs).on(sig, [&received](const int& val) { received = val; });

    TEST_ASSERT_EQUAL(1, subs.size());

    sig.set(42);
    TEST_ASSERT_EQUAL(42, received);
}

void test_bind_multiple_signals_chained() {
    Signal<int> sig1{0};
    Signal<float> sig2{0.0f};
    Signal<bool> sig3{false};
    std::vector<Subscription> subs;

    int val1 = 0;
    float val2 = 0.0f;
    bool val3 = false;

    bind(subs)
        .on(sig1, [&val1](const int& v) { val1 = v; })
        .on(sig2, [&val2](const float& v) { val2 = v; })
        .on(sig3, [&val3](const bool& v) { val3 = v; });

    TEST_ASSERT_EQUAL(3, subs.size());

    sig1.set(10);
    sig2.set(3.14f);
    sig3.set(true);

    TEST_ASSERT_EQUAL(10, val1);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.14f, val2);
    TEST_ASSERT_TRUE(val3);
}

void test_bind_with_signal_string() {
    SignalString sig;
    std::vector<Subscription> subs;
    const char* received = nullptr;

    bind(subs).on(sig, [&received](const char* val) { received = val; });

    TEST_ASSERT_EQUAL(1, subs.size());

    sig.set("Hello");
    TEST_ASSERT_EQUAL_STRING("Hello", received);
}

void test_bind_mixed_signal_types() {
    Signal<int> intSig{0};
    SignalString strSig;
    Signal<bool> boolSig{false};
    std::vector<Subscription> subs;

    int intVal = 0;
    const char* strVal = nullptr;
    bool boolVal = false;

    bind(subs)
        .on(intSig, [&intVal](const int& v) { intVal = v; })
        .on(strSig, [&strVal](const char* v) { strVal = v; })
        .on(boolSig, [&boolVal](const bool& v) { boolVal = v; });

    TEST_ASSERT_EQUAL(3, subs.size());

    intSig.set(100);
    strSig.set("Test");
    boolSig.set(true);

    TEST_ASSERT_EQUAL(100, intVal);
    TEST_ASSERT_EQUAL_STRING("Test", strVal);
    TEST_ASSERT_TRUE(boolVal);
}

// =============================================================================
// Lifecycle Tests
// =============================================================================

void test_bind_subscriptions_unsubscribe_on_vector_clear() {
    Signal<int> sig{0};
    std::vector<Subscription> subs;
    int callCount = 0;

    bind(subs).on(sig, [&callCount](const int&) { callCount++; });

    sig.set(1);
    TEST_ASSERT_EQUAL(1, callCount);

    subs.clear();  // Unsubscribe all

    sig.set(2);
    TEST_ASSERT_EQUAL(1, callCount);  // Should not increment
}

void test_bind_subscriptions_unsubscribe_on_vector_destruction() {
    Signal<int> sig{0};
    int callCount = 0;

    {
        std::vector<Subscription> subs;
        bind(subs).on(sig, [&callCount](const int&) { callCount++; });

        sig.set(1);
        TEST_ASSERT_EQUAL(1, callCount);
    }  // subs destroyed

    sig.set(2);
    TEST_ASSERT_EQUAL(1, callCount);  // Should not increment
}

// =============================================================================
// Reserve Pattern Tests
// =============================================================================

void test_bind_with_reserve() {
    Signal<int> sig1{0}, sig2{0}, sig3{0};
    std::vector<Subscription> subs;
    subs.reserve(10);  // Pre-allocate

    int sum = 0;

    bind(subs)
        .on(sig1, [&sum](const int& v) { sum += v; })
        .on(sig2, [&sum](const int& v) { sum += v; })
        .on(sig3, [&sum](const int& v) { sum += v; });

    TEST_ASSERT_EQUAL(3, subs.size());
    TEST_ASSERT_GREATER_OR_EQUAL(10, subs.capacity());

    sig1.set(1);
    sig2.set(2);
    sig3.set(3);

    TEST_ASSERT_EQUAL(6, sum);
}

// =============================================================================
// Loop Pattern Tests (Common UI Pattern)
// =============================================================================

void test_bind_in_loop() {
    std::array<Signal<int>, 4> signals = {Signal<int>{0}, Signal<int>{0}, Signal<int>{0},
                                          Signal<int>{0}};
    std::array<int, 4> values = {0, 0, 0, 0};
    std::vector<Subscription> subs;
    subs.reserve(4);

    for (size_t i = 0; i < 4; ++i) {
        bind(subs).on(signals[i], [&values, i](const int& v) { values[i] = v; });
    }

    TEST_ASSERT_EQUAL(4, subs.size());

    signals[0].set(10);
    signals[1].set(20);
    signals[2].set(30);
    signals[3].set(40);

    TEST_ASSERT_EQUAL(10, values[0]);
    TEST_ASSERT_EQUAL(20, values[1]);
    TEST_ASSERT_EQUAL(30, values[2]);
    TEST_ASSERT_EQUAL(40, values[3]);
}

void test_bind_multiple_signals_per_iteration() {
    struct ParamState {
        SignalLabel name;
        Signal<float> value{0.0f};
    };

    std::array<ParamState, 2> params;
    std::array<const char*, 2> names = {nullptr, nullptr};
    std::array<float, 2> values = {0.0f, 0.0f};
    std::vector<Subscription> subs;
    subs.reserve(4);

    for (size_t i = 0; i < 2; ++i) {
        bind(subs)
            .on(params[i].name, [&names, i](const char* n) { names[i] = n; })
            .on(params[i].value, [&values, i](const float& v) { values[i] = v; });
    }

    TEST_ASSERT_EQUAL(4, subs.size());

    params[0].name.set("Param1");
    params[0].value.set(0.5f);
    params[1].name.set("Param2");
    params[1].value.set(0.75f);

    TEST_ASSERT_EQUAL_STRING("Param1", names[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, values[0]);
    TEST_ASSERT_EQUAL_STRING("Param2", names[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.75f, values[1]);
}

// =============================================================================
// onImmediate Tests
// =============================================================================

void test_bind_on_immediate() {
    SignalString sig{"Initial"};
    std::vector<Subscription> subs;
    const char* received = nullptr;
    int callCount = 0;

    bind(subs).onImmediate(sig, [&received, &callCount](const char* val) {
        received = val;
        callCount++;
    });

    // Should have been called immediately
    TEST_ASSERT_EQUAL(1, callCount);
    TEST_ASSERT_EQUAL_STRING("Initial", received);

    // And on subsequent changes
    sig.set("Updated");
    TEST_ASSERT_EQUAL(2, callCount);
    TEST_ASSERT_EQUAL_STRING("Updated", received);
}

// =============================================================================
// Edge Cases
// =============================================================================

void test_bind_to_empty_vector() {
    Signal<int> sig{0};
    std::vector<Subscription> subs;

    TEST_ASSERT_TRUE(subs.empty());

    bind(subs).on(sig, [](const int&) {});

    TEST_ASSERT_EQUAL(1, subs.size());
}

void test_bind_returns_binder_reference() {
    Signal<int> sig{0};
    std::vector<Subscription> subs;

    // Should be able to chain from bind() return value
    Binder& ref = bind(subs).on(sig, [](const int&) {});

    // And continue chaining
    ref.on(sig, [](const int&) {});

    TEST_ASSERT_EQUAL(2, subs.size());
}

// =============================================================================
// Main
// =============================================================================

int main() {
    UNITY_BEGIN();

    // Basic Bind
    RUN_TEST(test_bind_single_signal);
    RUN_TEST(test_bind_multiple_signals_chained);
    RUN_TEST(test_bind_with_signal_string);
    RUN_TEST(test_bind_mixed_signal_types);

    // Lifecycle
    RUN_TEST(test_bind_subscriptions_unsubscribe_on_vector_clear);
    RUN_TEST(test_bind_subscriptions_unsubscribe_on_vector_destruction);

    // Reserve Pattern
    RUN_TEST(test_bind_with_reserve);

    // Loop Pattern
    RUN_TEST(test_bind_in_loop);
    RUN_TEST(test_bind_multiple_signals_per_iteration);

    // onImmediate
    RUN_TEST(test_bind_on_immediate);

    // Edge Cases
    RUN_TEST(test_bind_to_empty_vector);
    RUN_TEST(test_bind_returns_binder_reference);

    return UNITY_END();
}
