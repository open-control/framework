#include <unity.h>

#include <oc/state/AutoPersistIncremental.hpp>
#include <oc/state/NotificationQueue.hpp>
#include <oc/time/Time.hpp>

#include <set>

using namespace oc::state;

// =============================================================================
// Mock Time Provider
// =============================================================================

static uint32_t mockTime = 0;

uint32_t getMockTime() {
    return mockTime;
}

// =============================================================================
// Test Helpers
// =============================================================================

static std::set<uint8_t> savedIndices;
static int commitCount = 0;

void resetTestState() {
    savedIndices.clear();
    commitCount = 0;
}

void setUp() {
    mockTime = 100;  // Start at non-zero to avoid edge case
    oc::time::setProvider(getMockTime);
    resetTestState();
    NotificationQueue::instance().setDeferredMode(false);
}

void tearDown() {
    NotificationQueue::instance().setDeferredMode(true);
}

// =============================================================================
// AutoPersistIncremental Tests
// =============================================================================

void test_no_save_before_debounce() {
    AutoPersistIncremental<8> persist{
        [](uint8_t i) { savedIndices.insert(i); },
        []() { commitCount++; },
        1000
    };

    Signal<float> sig{0.0f};
    persist.watchAt(0, sig);

    mockTime = 100;
    sig.set(1.0f);

    mockTime = 500;
    persist.update();

    TEST_ASSERT_TRUE(savedIndices.empty());
    TEST_ASSERT_EQUAL(0, commitCount);
    TEST_ASSERT_TRUE(persist.hasPendingChanges());
}

void test_saves_after_debounce() {
    AutoPersistIncremental<8> persist{
        [](uint8_t i) { savedIndices.insert(i); },
        []() { commitCount++; },
        1000
    };

    Signal<float> sig{0.0f};
    persist.watchAt(0, sig);

    mockTime = 100;
    sig.set(1.0f);

    mockTime = 1100;
    persist.update();

    TEST_ASSERT_EQUAL(1, savedIndices.size());
    TEST_ASSERT_TRUE(savedIndices.count(0) > 0);
    TEST_ASSERT_EQUAL(1, commitCount);
    TEST_ASSERT_FALSE(persist.hasPendingChanges());
}

void test_flush_saves_immediately() {
    AutoPersistIncremental<8> persist{
        [](uint8_t i) { savedIndices.insert(i); },
        []() { commitCount++; },
        1000
    };

    Signal<float> sig{0.0f};
    persist.watchAt(0, sig);

    mockTime = 100;
    sig.set(1.0f);

    persist.flush();

    TEST_ASSERT_EQUAL(1, savedIndices.size());
    TEST_ASSERT_EQUAL(1, commitCount);
    TEST_ASSERT_FALSE(persist.hasPendingChanges());
}

void test_multiple_signals_only_dirty_saved() {
    AutoPersistIncremental<8> persist{
        [](uint8_t i) { savedIndices.insert(i); },
        []() { commitCount++; },
        1000
    };

    Signal<float> sig0{0.0f};
    Signal<float> sig1{0.0f};
    Signal<float> sig2{0.0f};

    persist.watchAt(0, sig0);
    persist.watchAt(1, sig1);
    persist.watchAt(2, sig2);

    // Only change index 0 and 2
    mockTime = 100;
    sig0.set(1.0f);
    sig2.set(2.0f);

    mockTime = 1100;
    persist.update();

    // Only indices 0 and 2 should be saved
    TEST_ASSERT_EQUAL(2, savedIndices.size());
    TEST_ASSERT_TRUE(savedIndices.count(0) > 0);
    TEST_ASSERT_TRUE(savedIndices.count(2) > 0);
    TEST_ASSERT_FALSE(savedIndices.count(1) > 0);
    TEST_ASSERT_EQUAL(1, commitCount);
}

void test_dirty_mask_tracking() {
    AutoPersistIncremental<8> persist{
        [](uint8_t) {},
        []() {},
        1000
    };

    Signal<float> sig0{0.0f};
    Signal<float> sig3{0.0f};
    Signal<float> sig7{0.0f};

    persist.watchAt(0, sig0);
    persist.watchAt(3, sig3);
    persist.watchAt(7, sig7);

    mockTime = 100;
    sig0.set(1.0f);
    sig3.set(1.0f);
    sig7.set(1.0f);

    TEST_ASSERT_TRUE(persist.isDirty(0));
    TEST_ASSERT_FALSE(persist.isDirty(1));
    TEST_ASSERT_FALSE(persist.isDirty(2));
    TEST_ASSERT_TRUE(persist.isDirty(3));
    TEST_ASSERT_TRUE(persist.isDirty(7));

    // Binary: 10001001 = 137
    TEST_ASSERT_EQUAL(137, persist.dirtyMask());
}

void test_no_pending_changes_initially() {
    AutoPersistIncremental<8> persist{
        [](uint8_t) { savedIndices.insert(0); },
        []() { commitCount++; },
        1000
    };

    TEST_ASSERT_FALSE(persist.hasPendingChanges());
    TEST_ASSERT_EQUAL(0, persist.dirtyMask());

    persist.update();
    TEST_ASSERT_TRUE(savedIndices.empty());
    TEST_ASSERT_EQUAL(0, commitCount);

    persist.flush();
    TEST_ASSERT_TRUE(savedIndices.empty());
    TEST_ASSERT_EQUAL(0, commitCount);
}

void test_debounce_ms_accessors() {
    AutoPersistIncremental<8> persist{
        [](uint8_t) {},
        []() {},
        500
    };

    TEST_ASSERT_EQUAL(500, persist.debounceMs());

    persist.setDebounceMs(2000);
    TEST_ASSERT_EQUAL(2000, persist.debounceMs());
}

void test_multiple_changes_coalesced() {
    AutoPersistIncremental<8> persist{
        [](uint8_t i) { savedIndices.insert(i); },
        []() { commitCount++; },
        1000
    };

    Signal<float> sig{0.0f};
    persist.watchAt(0, sig);

    // Multiple changes in quick succession
    mockTime = 100;
    sig.set(1.0f);
    mockTime = 200;
    sig.set(2.0f);
    mockTime = 300;
    sig.set(3.0f);

    // Before debounce
    mockTime = 500;
    persist.update();
    TEST_ASSERT_TRUE(savedIndices.empty());

    // After debounce (from first change at 100)
    mockTime = 1100;
    persist.update();

    // Should only save once
    TEST_ASSERT_EQUAL(1, savedIndices.size());
    TEST_ASSERT_EQUAL(1, commitCount);
}

// =============================================================================
// Test Runner
// =============================================================================

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_no_save_before_debounce);
    RUN_TEST(test_saves_after_debounce);
    RUN_TEST(test_flush_saves_immediately);
    RUN_TEST(test_multiple_signals_only_dirty_saved);
    RUN_TEST(test_dirty_mask_tracking);
    RUN_TEST(test_no_pending_changes_initially);
    RUN_TEST(test_debounce_ms_accessors);
    RUN_TEST(test_multiple_changes_coalesced);

    return UNITY_END();
}
