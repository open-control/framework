#include <unity.h>

#include <array>
#include <cstring>

#include <oc/state/AutoPersist.hpp>
#include <oc/state/NotificationQueue.hpp>
#include <oc/time/Time.hpp>

using namespace oc::state;
using namespace oc;

// =============================================================================
// Mock Time Provider
// =============================================================================

static uint32_t mockTime = 0;

uint32_t getMockTime() {
    return mockTime;
}

// =============================================================================
// Mock Storage Backend
// =============================================================================

class MockStorageBackend : public interface::IStorage {
public:
    std::array<uint8_t, 1024> storage{};
    int saveCount = 0;

    oc::type::Result<void> init() override { return oc::type::Result<void>::ok(); }

    bool available() const override { return true; }

    size_t read(uint32_t address, uint8_t* buffer, size_t size) override {
        if (address + size > storage.size()) return 0;
        std::memcpy(buffer, &storage[address], size);
        return size;
    }

    size_t write(uint32_t address, const uint8_t* buffer, size_t size) override {
        if (address + size > storage.size()) return 0;
        std::memcpy(&storage[address], buffer, size);
        return size;
    }

    bool commit() override {
        saveCount++;
        return true;
    }

    bool erase(uint32_t address, size_t size) override {
        if (address + size > storage.size()) return false;
        std::memset(&storage[address], 0xFF, size);
        return true;
    }

    size_t capacity() const override { return storage.size(); }

    void clear() {
        storage.fill(0xFF);
        saveCount = 0;
    }
};

// =============================================================================
// Test Data
// =============================================================================

struct TestData {
    int value1 = 0;
    float value2 = 0.0f;
};

MockStorageBackend backend;

void setUp() {
    mockTime = 0;
    oc::time::setProvider(getMockTime);
    backend.clear();
    // Use immediate mode for unit tests
    NotificationQueue::instance().setDeferredMode(false);
}

void tearDown() {
    NotificationQueue::instance().setDeferredMode(true);
}

// =============================================================================
// AutoPersist Tests
// =============================================================================

void test_autopersist_no_save_before_debounce() {
    Settings<TestData> settings{backend, 0, 1};
    AutoPersist<TestData> persist{settings, 1000};  // 1s debounce

    Signal<int> sig{0};
    persist.watch(sig, [](TestData& data, int val) {
        data.value1 = val;
    });

    // Change value at time 0
    mockTime = 0;
    sig.set(42);

    // Update at time 500ms (before debounce)
    mockTime = 500;
    persist.update();

    // Should not have saved yet
    TEST_ASSERT_EQUAL(0, backend.saveCount);
    TEST_ASSERT_TRUE(persist.hasPendingChanges());
}

void test_autopersist_saves_after_debounce() {
    Settings<TestData> settings{backend, 0, 1};
    AutoPersist<TestData> persist{settings, 1000};

    Signal<int> sig{0};
    persist.watch(sig, [](TestData& data, int val) {
        data.value1 = val;
    });

    // Change value at time 100 (avoid edge case with time 0)
    mockTime = 100;
    sig.set(42);

    // Update at time 1100ms (debounce threshold reached)
    mockTime = 1100;
    persist.update();

    // Should have saved
    TEST_ASSERT_EQUAL(1, backend.saveCount);
    TEST_ASSERT_FALSE(persist.hasPendingChanges());
}

void test_autopersist_flush_saves_immediately() {
    Settings<TestData> settings{backend, 0, 1};
    AutoPersist<TestData> persist{settings, 1000};

    Signal<int> sig{0};
    persist.watch(sig, [](TestData& data, int val) {
        data.value1 = val;
    });

    // Change value at time 0
    mockTime = 0;
    sig.set(42);

    // Flush immediately (before debounce)
    persist.flush();

    // Should have saved
    TEST_ASSERT_EQUAL(1, backend.saveCount);
    TEST_ASSERT_FALSE(persist.hasPendingChanges());
}

void test_autopersist_multiple_changes_single_save() {
    Settings<TestData> settings{backend, 0, 1};
    AutoPersist<TestData> persist{settings, 1000};

    Signal<int> sig{0};
    persist.watch(sig, [](TestData& data, int val) {
        data.value1 = val;
    });

    // Multiple changes in quick succession
    mockTime = 0;
    sig.set(1);
    mockTime = 100;
    sig.set(2);
    mockTime = 200;
    sig.set(3);

    // Update before debounce
    mockTime = 500;
    persist.update();
    TEST_ASSERT_EQUAL(0, backend.saveCount);

    // Update after debounce from last change
    mockTime = 1200;
    persist.update();
    TEST_ASSERT_EQUAL(1, backend.saveCount);

    // Verify final value was saved
    TEST_ASSERT_EQUAL(3, settings.get().value1);
}

void test_autopersist_watch_multiple_signals() {
    Settings<TestData> settings{backend, 0, 1};
    AutoPersist<TestData> persist{settings, 1000};

    Signal<int> sig1{0};
    Signal<float> sig2{0.0f};

    persist.watch(sig1, [](TestData& data, int val) {
        data.value1 = val;
    });
    persist.watch(sig2, [](TestData& data, float val) {
        data.value2 = val;
    });

    // Change both signals
    mockTime = 0;
    sig1.set(42);
    sig2.set(3.14f);

    // Flush and verify
    persist.flush();

    TEST_ASSERT_EQUAL(1, backend.saveCount);
    TEST_ASSERT_EQUAL(42, settings.get().value1);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.14f, settings.get().value2);
}

void test_autopersist_no_pending_changes_initially() {
    Settings<TestData> settings{backend, 0, 1};
    AutoPersist<TestData> persist{settings, 1000};

    TEST_ASSERT_FALSE(persist.hasPendingChanges());

    // Update should do nothing
    persist.update();
    TEST_ASSERT_EQUAL(0, backend.saveCount);

    // Flush should do nothing
    persist.flush();
    TEST_ASSERT_EQUAL(0, backend.saveCount);
}

void test_autopersist_debounce_ms_accessors() {
    Settings<TestData> settings{backend, 0, 1};
    AutoPersist<TestData> persist{settings, 500};

    TEST_ASSERT_EQUAL(500, persist.debounceMs());

    persist.setDebounceMs(2000);
    TEST_ASSERT_EQUAL(2000, persist.debounceMs());
}

void test_autopersist_debounce_resets_on_change() {
    Settings<TestData> settings{backend, 0, 1};
    AutoPersist<TestData> persist{settings, 1000};

    Signal<int> sig{0};
    persist.watch(sig, [](TestData& data, int val) {
        data.value1 = val;
    });

    // First change at time 100 (avoid edge case with time 0)
    mockTime = 100;
    sig.set(1);

    // Almost at debounce, then another change
    mockTime = 1000;
    persist.update();
    TEST_ASSERT_EQUAL(0, backend.saveCount);  // Not saved yet

    // 1000ms after first change, should save
    mockTime = 1100;
    persist.update();
    TEST_ASSERT_EQUAL(1, backend.saveCount);
}

// =============================================================================
// Test Runner
// =============================================================================

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_autopersist_no_save_before_debounce);
    RUN_TEST(test_autopersist_saves_after_debounce);
    RUN_TEST(test_autopersist_flush_saves_immediately);
    RUN_TEST(test_autopersist_multiple_changes_single_save);
    RUN_TEST(test_autopersist_watch_multiple_signals);
    RUN_TEST(test_autopersist_no_pending_changes_initially);
    RUN_TEST(test_autopersist_debounce_ms_accessors);
    RUN_TEST(test_autopersist_debounce_resets_on_change);

    return UNITY_END();
}
