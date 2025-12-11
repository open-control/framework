#include <unity.h>

#include <array>
#include <cstring>

#include <oc/state/Settings.hpp>

using namespace oc::state;
using namespace oc::hal;

// Mock storage backend for testing
class MockStorageBackend : public IStorageBackend {
public:
    std::array<uint8_t, 1024> storage{};
    bool available_ = true;
    bool failRead_ = false;
    bool failWrite_ = false;
    bool failCommit_ = false;

    bool available() const override { return available_; }

    size_t read(uint32_t address, uint8_t* buffer, size_t size) override {
        if (failRead_ || address + size > storage.size()) return 0;
        std::memcpy(buffer, &storage[address], size);
        return size;
    }

    size_t write(uint32_t address, const uint8_t* buffer, size_t size) override {
        if (failWrite_ || address + size > storage.size()) return 0;
        std::memcpy(&storage[address], buffer, size);
        return size;
    }

    bool commit() override { return !failCommit_; }

    bool erase(uint32_t address, size_t size) override {
        if (address + size > storage.size()) return false;
        std::memset(&storage[address], 0xFF, size);
        return true;
    }

    size_t capacity() const override { return storage.size(); }

    void clear() { storage.fill(0xFF); }
};

struct TestSettings {
    uint8_t value1 = 42;
    float value2 = 3.14f;
    char name[16] = "default";
};

MockStorageBackend backend;

void setUp() {
    backend.clear();
    backend.failRead_ = false;
    backend.failWrite_ = false;
    backend.failCommit_ = false;
}

void tearDown() {}

void test_settings_default_values() {
    Settings<TestSettings> settings(backend, 0, 1);

    TEST_ASSERT_EQUAL(42, settings.get().value1);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.14f, settings.get().value2);
    TEST_ASSERT_EQUAL_STRING("default", settings.get().name);
}

void test_settings_save_and_load() {
    // Save
    {
        Settings<TestSettings> settings(backend, 0, 1);
        settings.modify([](auto& s) {
            s.value1 = 100;
            s.value2 = 2.5f;
            setString(s.name, "saved");
        });
        auto result = settings.save();
        TEST_ASSERT_TRUE(result.isOk());
    }

    // Load in new instance
    {
        Settings<TestSettings> settings(backend, 0, 1);
        auto result = settings.load();
        TEST_ASSERT_TRUE(result.isOk());
        TEST_ASSERT_EQUAL(100, settings.get().value1);
        TEST_ASSERT_FLOAT_WITHIN(0.01f, 2.5f, settings.get().value2);
        TEST_ASSERT_EQUAL_STRING("saved", settings.get().name);
    }
}

void test_settings_dirty_tracking() {
    Settings<TestSettings> settings(backend, 0, 1);

    TEST_ASSERT_TRUE(settings.isDirty());  // Initially dirty

    settings.save();
    TEST_ASSERT_FALSE(settings.isDirty());  // Clean after save

    settings.modify([](auto& s) { s.value1 = 99; });
    TEST_ASSERT_TRUE(settings.isDirty());  // Dirty after modify
}

void test_settings_first_boot_uses_defaults() {
    backend.clear();  // Simulate fresh EEPROM

    Settings<TestSettings> settings(backend, 0, 1);
    auto result = settings.load();

    TEST_ASSERT_TRUE(result.isOk());
    TEST_ASSERT_EQUAL(42, settings.get().value1);  // Default value
    TEST_ASSERT_TRUE(settings.isDirty());  // Should save defaults
}

void test_settings_corrupt_checksum_resets() {
    // Save valid data
    {
        Settings<TestSettings> settings(backend, 0, 1);
        settings.modify([](auto& s) { s.value1 = 123; });
        settings.save();
    }

    // Corrupt one byte
    backend.storage[20] ^= 0xFF;

    // Load should fail checksum and reset
    {
        Settings<TestSettings> settings(backend, 0, 1);
        auto result = settings.load();
        TEST_ASSERT_TRUE(result.isErr());
        TEST_ASSERT_EQUAL(oc::core::ErrorCode::STORAGE_CORRUPT, result.error().code);
        TEST_ASSERT_EQUAL(42, settings.get().value1);  // Reset to default
    }
}

void test_settings_version_mismatch_resets() {
    // Save with version 1
    {
        Settings<TestSettings> settings(backend, 0, 1);
        settings.modify([](auto& s) { s.value1 = 123; });
        settings.save();
    }

    // Load with version 2
    {
        Settings<TestSettings> settings(backend, 0, 2);
        auto result = settings.load();
        TEST_ASSERT_TRUE(result.isOk());
        TEST_ASSERT_EQUAL(42, settings.get().value1);  // Reset to default
        TEST_ASSERT_TRUE(settings.isDirty());
    }
}

void test_settings_factory_reset() {
    // Save data
    {
        Settings<TestSettings> settings(backend, 0, 1);
        settings.modify([](auto& s) { s.value1 = 123; });
        settings.save();
    }

    // Factory reset
    {
        Settings<TestSettings> settings(backend, 0, 1);
        settings.load();
        auto result = settings.factoryReset();
        TEST_ASSERT_TRUE(result.isOk());
        TEST_ASSERT_EQUAL(42, settings.get().value1);
        TEST_ASSERT_FALSE(settings.isDirty());  // Saved during reset
    }
}

void test_settings_write_failure() {
    Settings<TestSettings> settings(backend, 0, 1);
    backend.failWrite_ = true;

    settings.modify([](auto& s) { s.value1 = 99; });
    auto result = settings.save();

    TEST_ASSERT_TRUE(result.isErr());
    TEST_ASSERT_EQUAL(oc::core::ErrorCode::STORAGE_WRITE_FAILED, result.error().code);
}

void test_settings_commit_failure() {
    Settings<TestSettings> settings(backend, 0, 1);
    backend.failCommit_ = true;

    settings.modify([](auto& s) { s.value1 = 99; });
    auto result = settings.save();

    TEST_ASSERT_TRUE(result.isErr());
    TEST_ASSERT_EQUAL(oc::core::ErrorCode::STORAGE_WRITE_FAILED, result.error().code);
}

void test_set_string_helper() {
    char buffer[8];

    setString(buffer, "hello");
    TEST_ASSERT_EQUAL_STRING("hello", buffer);

    setString(buffer, "very long string");
    TEST_ASSERT_EQUAL(7u, strlen(buffer));  // Truncated
    TEST_ASSERT_EQUAL_STRING("very lo", buffer);
}

void test_settings_storage_size() {
    constexpr size_t expected = sizeof(SettingsHeader) + sizeof(TestSettings);
    TEST_ASSERT_EQUAL(expected, Settings<TestSettings>::storageSize());
}

void test_settings_reload_discards_changes() {
    // Save initial data
    {
        Settings<TestSettings> settings(backend, 0, 1);
        settings.modify([](auto& s) { s.value1 = 100; });
        settings.save();
    }

    // Modify and reload
    {
        Settings<TestSettings> settings(backend, 0, 1);
        settings.load();
        TEST_ASSERT_EQUAL(100, settings.get().value1);

        settings.modify([](auto& s) { s.value1 = 200; });
        TEST_ASSERT_EQUAL(200, settings.get().value1);

        settings.reload();
        TEST_ASSERT_EQUAL(100, settings.get().value1);  // Back to saved value
    }
}

void test_settings_not_saved_when_not_dirty() {
    Settings<TestSettings> settings(backend, 0, 1);

    // Save to make it clean
    settings.save();
    TEST_ASSERT_FALSE(settings.isDirty());

    // Mark write as failing - but save should succeed because not dirty
    backend.failWrite_ = true;
    auto result = settings.save();
    TEST_ASSERT_TRUE(result.isOk());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_settings_default_values);
    RUN_TEST(test_settings_save_and_load);
    RUN_TEST(test_settings_dirty_tracking);
    RUN_TEST(test_settings_first_boot_uses_defaults);
    RUN_TEST(test_settings_corrupt_checksum_resets);
    RUN_TEST(test_settings_version_mismatch_resets);
    RUN_TEST(test_settings_factory_reset);
    RUN_TEST(test_settings_write_failure);
    RUN_TEST(test_settings_commit_failure);
    RUN_TEST(test_set_string_helper);
    RUN_TEST(test_settings_storage_size);
    RUN_TEST(test_settings_reload_discards_changes);
    RUN_TEST(test_settings_not_saved_when_not_dirty);
    return UNITY_END();
}
