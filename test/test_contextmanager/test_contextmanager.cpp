#include <unity.h>
#include <oc/context/ContextManager.hpp>
#include "../mocks/MockEventBus.hpp"
#include "../mocks/MockContext.hpp"

using namespace oc::context;
using namespace oc::test;
using oc::interface::ContextInfo;

enum class TestContextID : uint8_t {
    CTX_A = 0,
    CTX_B = 1,
    CTX_C = 2
};

static MockEventBus bus;

void setUp() {
    bus.reset();
    MockContext::reset();
    MockContextB::reset();
}

void tearDown() {}

// ═══════════════════════════════════════════════════════════════════
// Registration
// ═══════════════════════════════════════════════════════════════════

void test_register_context() {
    APIs apis(bus);
    ContextManager mgr(apis);

    bool result = mgr.registerContext<MockContext>(TestContextID::CTX_A, "ContextA");
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, mgr.contextCount());
}

void test_register_multiple_contexts() {
    APIs apis(bus);
    ContextManager mgr(apis);

    mgr.registerContext<MockContext>(TestContextID::CTX_A, "ContextA");
    mgr.registerContext<MockContextB>(TestContextID::CTX_B, "ContextB");

    TEST_ASSERT_EQUAL(2, mgr.contextCount());
}

void test_cannot_register_same_id_twice() {
    APIs apis(bus);
    ContextManager mgr(apis);

    bool first = mgr.registerContext<MockContext>(TestContextID::CTX_A, "ContextA");
    bool second = mgr.registerContext<MockContext>(TestContextID::CTX_A, "ContextA2");

    TEST_ASSERT_TRUE(first);
    TEST_ASSERT_FALSE(second);
    TEST_ASSERT_EQUAL(1, mgr.contextCount());
}

void test_first_registered_becomes_default() {
    APIs apis(bus);
    ContextManager mgr(apis);

    mgr.registerContext<MockContext>(TestContextID::CTX_B, "ContextB");

    TEST_ASSERT_EQUAL(static_cast<uint8_t>(TestContextID::CTX_B), mgr.defaultId());
}

void test_has_context() {
    APIs apis(bus);
    ContextManager mgr(apis);

    TEST_ASSERT_FALSE(mgr.hasContextById(static_cast<uint8_t>(TestContextID::CTX_A)));

    mgr.registerContext<MockContext>(TestContextID::CTX_A, "ContextA");

    TEST_ASSERT_TRUE(mgr.hasContextById(static_cast<uint8_t>(TestContextID::CTX_A)));
    TEST_ASSERT_FALSE(mgr.hasContextById(static_cast<uint8_t>(TestContextID::CTX_B)));
}

void test_context_name() {
    APIs apis(bus);
    ContextManager mgr(apis);

    mgr.registerContext<MockContext>(TestContextID::CTX_A, "MyContext");
    mgr.registerContext<MockContextB>(TestContextID::CTX_B, "OtherContext");

    TEST_ASSERT_EQUAL_STRING("MyContext", mgr.contextName(static_cast<uint8_t>(TestContextID::CTX_A)));
    TEST_ASSERT_EQUAL_STRING("OtherContext", mgr.contextName(static_cast<uint8_t>(TestContextID::CTX_B)));
    TEST_ASSERT_NULL(mgr.contextName(static_cast<uint8_t>(TestContextID::CTX_C)));
}

// ═══════════════════════════════════════════════════════════════════
// Requirements Validation
// ═══════════════════════════════════════════════════════════════════

void test_requirements_validation_fails_without_button_api() {
    APIs apis(bus);
    apis.button = nullptr;  // No ButtonAPI
    ContextManager mgr(apis);

    bool result = mgr.registerContext<MockContextRequiresButton>(TestContextID::CTX_A, "RequiresButton");
    TEST_ASSERT_FALSE(result);
}

// ═══════════════════════════════════════════════════════════════════
// Begin (Activate Default)
// ═══════════════════════════════════════════════════════════════════

void test_begin_activates_default_context() {
    APIs apis(bus);
    ContextManager mgr(apis);

    mgr.registerContext<MockContext>(TestContextID::CTX_A, "ContextA");

    auto result = mgr.begin();

    TEST_ASSERT_TRUE(result.isOk());
    TEST_ASSERT_NOT_NULL(mgr.active());
    TEST_ASSERT_EQUAL(static_cast<uint8_t>(TestContextID::CTX_A), mgr.activeId());
    TEST_ASSERT_TRUE(MockContext::wasInitialized());
}

void test_begin_fails_without_registered_contexts() {
    APIs apis(bus);
    ContextManager mgr(apis);

    auto result = mgr.begin();

    TEST_ASSERT_TRUE(result.isErr());
    TEST_ASSERT_NULL(mgr.active());
}

void test_failed_default_init_is_cleaned_up() {
    APIs apis(bus);
    ContextManager mgr(apis);

    mgr.registerContext<MockContext>(TestContextID::CTX_A, "ContextA");
    MockContext::setInitShouldFail();

    auto result = mgr.begin();

    TEST_ASSERT_TRUE(result.isErr());
    TEST_ASSERT_NULL(mgr.active());
    TEST_ASSERT_EQUAL(INVALID_CONTEXT_ID, mgr.activeId());
    TEST_ASSERT_TRUE(MockContext::wasCleanedUp());
    TEST_ASSERT_EQUAL(1, MockContext::cleanupCount());
}

// ═══════════════════════════════════════════════════════════════════
// Context Switching (deferred via update)
// ═══════════════════════════════════════════════════════════════════

void test_switch_to_context() {
    APIs apis(bus);
    ContextManager mgr(apis);

    mgr.registerContext<MockContext>(TestContextID::CTX_A, "ContextA");
    mgr.registerContext<MockContextB>(TestContextID::CTX_B, "ContextB");
    mgr.begin();

    mgr.switchTo(TestContextID::CTX_B);
    mgr.update();  // Process pending switch

    TEST_ASSERT_EQUAL(static_cast<uint8_t>(TestContextID::CTX_B), mgr.activeId());
    TEST_ASSERT_TRUE(MockContextB::wasInitialized());
}

void test_switch_cleans_up_previous_context() {
    APIs apis(bus);
    ContextManager mgr(apis);

    mgr.registerContext<MockContext>(TestContextID::CTX_A, "ContextA");
    mgr.registerContext<MockContextB>(TestContextID::CTX_B, "ContextB");
    mgr.begin();

    mgr.switchTo(TestContextID::CTX_B);
    mgr.update();

    TEST_ASSERT_TRUE(MockContext::wasCleanedUp());
}

void test_switch_to_unregistered_does_nothing() {
    APIs apis(bus);
    ContextManager mgr(apis);

    mgr.registerContext<MockContext>(TestContextID::CTX_A, "ContextA");
    mgr.begin();

    mgr.switchTo(TestContextID::CTX_C);
    mgr.update();

    // Should stay on A since C doesn't exist
    TEST_ASSERT_EQUAL(static_cast<uint8_t>(TestContextID::CTX_A), mgr.activeId());
}

void test_switch_to_same_context_is_noop() {
    APIs apis(bus);
    ContextManager mgr(apis);

    mgr.registerContext<MockContext>(TestContextID::CTX_A, "ContextA");
    mgr.begin();

    int initCountBefore = MockContext::initializeCount();

    mgr.switchTo(TestContextID::CTX_A);
    mgr.update();

    TEST_ASSERT_EQUAL(initCountBefore, MockContext::initializeCount());
}

// ═══════════════════════════════════════════════════════════════════
// Pending Switch Mechanics
// ═══════════════════════════════════════════════════════════════════

void test_switch_sets_pending() {
    APIs apis(bus);
    ContextManager mgr(apis);

    mgr.registerContext<MockContext>(TestContextID::CTX_A, "ContextA");
    mgr.registerContext<MockContextB>(TestContextID::CTX_B, "ContextB");
    mgr.begin();

    TEST_ASSERT_FALSE(mgr.hasPendingSwitch());

    mgr.switchTo(TestContextID::CTX_B);

    TEST_ASSERT_TRUE(mgr.hasPendingSwitch());
    // Not switched yet - still on A
    TEST_ASSERT_EQUAL(static_cast<uint8_t>(TestContextID::CTX_A), mgr.activeId());
}

void test_pending_switch_processed_after_update() {
    APIs apis(bus);
    ContextManager mgr(apis);

    mgr.registerContext<MockContext>(TestContextID::CTX_A, "ContextA");
    mgr.registerContext<MockContextB>(TestContextID::CTX_B, "ContextB");
    mgr.begin();

    mgr.switchTo(TestContextID::CTX_B);

    // Before update: still on A
    TEST_ASSERT_EQUAL(static_cast<uint8_t>(TestContextID::CTX_A), mgr.activeId());

    // After update: switch processed
    mgr.update();

    TEST_ASSERT_EQUAL(static_cast<uint8_t>(TestContextID::CTX_B), mgr.activeId());
    TEST_ASSERT_FALSE(mgr.hasPendingSwitch());
}

void test_switch_to_default() {
    APIs apis(bus);
    ContextManager mgr(apis);
    apis.contexts = &mgr;

    mgr.registerContext<MockContext>(TestContextID::CTX_A, "ContextA");
    mgr.registerContext<MockContextB>(TestContextID::CTX_B, "ContextB");
    mgr.setDefault(TestContextID::CTX_A);
    mgr.begin();

    // Switch to B first
    mgr.switchTo(TestContextID::CTX_B);
    mgr.update();
    TEST_ASSERT_EQUAL(static_cast<uint8_t>(TestContextID::CTX_B), mgr.activeId());

    // Now request switch to default
    mgr.switchToDefault();
    TEST_ASSERT_TRUE(mgr.hasPendingSwitch());

    mgr.update();
    TEST_ASSERT_EQUAL(static_cast<uint8_t>(TestContextID::CTX_A), mgr.activeId());
}

// ═══════════════════════════════════════════════════════════════════
// Set Default
// ═══════════════════════════════════════════════════════════════════

void test_set_default() {
    APIs apis(bus);
    ContextManager mgr(apis);

    mgr.registerContext<MockContext>(TestContextID::CTX_A, "ContextA");
    mgr.registerContext<MockContextB>(TestContextID::CTX_B, "ContextB");

    mgr.setDefault(TestContextID::CTX_B);

    TEST_ASSERT_EQUAL(static_cast<uint8_t>(TestContextID::CTX_B), mgr.defaultId());
}

// ═══════════════════════════════════════════════════════════════════
// Update
// ═══════════════════════════════════════════════════════════════════

void test_update_calls_context_update() {
    APIs apis(bus);
    ContextManager mgr(apis);

    mgr.registerContext<MockContext>(TestContextID::CTX_A, "ContextA");
    mgr.begin();

    int countBefore = MockContext::updateCount();

    mgr.update();
    mgr.update();
    mgr.update();

    TEST_ASSERT_EQUAL(countBefore + 3, MockContext::updateCount());
}

void test_update_without_active_context_does_nothing() {
    APIs apis(bus);
    ContextManager mgr(apis);

    // No begin() called - no active context
    mgr.update();  // Should not crash
}

// ═══════════════════════════════════════════════════════════════════
// ForEach Context Iteration
// ═══════════════════════════════════════════════════════════════════

void test_for_each_context() {
    APIs apis(bus);
    ContextManager mgr(apis);

    mgr.registerContext<MockContext>(TestContextID::CTX_A, "Alpha");
    mgr.registerContext<MockContextB>(TestContextID::CTX_B, "Beta");
    mgr.setDefault(TestContextID::CTX_A);

    int count = 0;
    bool foundAlpha = false;
    bool foundBeta = false;
    bool alphaIsDefault = false;

    mgr.forEachContext([&](const ContextInfo& info) {
        count++;
        if (info.id == static_cast<uint8_t>(TestContextID::CTX_A)) {
            foundAlpha = true;
            alphaIsDefault = info.isDefault;
            TEST_ASSERT_EQUAL_STRING("Alpha", info.name);
        }
        if (info.id == static_cast<uint8_t>(TestContextID::CTX_B)) {
            foundBeta = true;
            TEST_ASSERT_EQUAL_STRING("Beta", info.name);
            TEST_ASSERT_FALSE(info.isDefault);
        }
    });

    TEST_ASSERT_EQUAL(2, count);
    TEST_ASSERT_TRUE(foundAlpha);
    TEST_ASSERT_TRUE(foundBeta);
    TEST_ASSERT_TRUE(alphaIsDefault);
}

// ═══════════════════════════════════════════════════════════════════
// Initialization Failure Fallback
// ═══════════════════════════════════════════════════════════════════

void test_init_failure_falls_back_to_default() {
    APIs apis(bus);
    ContextManager mgr(apis);

    mgr.registerContext<MockContext>(TestContextID::CTX_A, "ContextA");
    mgr.registerContext<MockContextB>(TestContextID::CTX_B, "ContextB");
    mgr.setDefault(TestContextID::CTX_A);
    mgr.begin();

    MockContextB::setInitShouldFail();
    mgr.switchTo(TestContextID::CTX_B);
    mgr.update();

    TEST_ASSERT_TRUE(MockContextB::wasInitialized());
    TEST_ASSERT_TRUE(MockContextB::wasCleanedUp());
    TEST_ASSERT_EQUAL(static_cast<uint8_t>(TestContextID::CTX_A), mgr.activeId());
    TEST_ASSERT_EQUAL(2, MockContext::initializeCount());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Registration
    RUN_TEST(test_register_context);
    RUN_TEST(test_register_multiple_contexts);
    RUN_TEST(test_cannot_register_same_id_twice);
    RUN_TEST(test_first_registered_becomes_default);
    RUN_TEST(test_has_context);
    RUN_TEST(test_context_name);

    // Requirements
    RUN_TEST(test_requirements_validation_fails_without_button_api);

    // Begin
    RUN_TEST(test_begin_activates_default_context);
    RUN_TEST(test_begin_fails_without_registered_contexts);
    RUN_TEST(test_failed_default_init_is_cleaned_up);

    // Switching
    RUN_TEST(test_switch_to_context);
    RUN_TEST(test_switch_cleans_up_previous_context);
    RUN_TEST(test_switch_to_unregistered_does_nothing);
    RUN_TEST(test_switch_to_same_context_is_noop);

    // Pending Switch Mechanics
    RUN_TEST(test_switch_sets_pending);
    RUN_TEST(test_pending_switch_processed_after_update);
    RUN_TEST(test_switch_to_default);

    // Default Configuration
    RUN_TEST(test_set_default);

    // Update
    RUN_TEST(test_update_calls_context_update);
    RUN_TEST(test_update_without_active_context_does_nothing);

    // ForEach
    RUN_TEST(test_for_each_context);

    // Fallback
    RUN_TEST(test_init_failure_falls_back_to_default);

    UNITY_END();
    return 0;
}
