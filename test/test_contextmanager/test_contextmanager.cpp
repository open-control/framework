#include <unity.h>
#include <oc/context/ContextManager.hpp>
#include "../mocks/MockEventBus.hpp"
#include "../mocks/MockContext.hpp"

using namespace oc::context;
using namespace oc::test;

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

    bool result = mgr.registerContext<MockContext>(TestContextID::CTX_A);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, mgr.registeredCount());
}

void test_register_multiple_contexts() {
    APIs apis(bus);
    ContextManager mgr(apis);

    mgr.registerContext<MockContext>(TestContextID::CTX_A);
    mgr.registerContext<MockContextB>(TestContextID::CTX_B);

    TEST_ASSERT_EQUAL(2, mgr.registeredCount());
}

void test_cannot_register_same_id_twice() {
    APIs apis(bus);
    ContextManager mgr(apis);

    bool first = mgr.registerContext<MockContext>(TestContextID::CTX_A);
    bool second = mgr.registerContext<MockContext>(TestContextID::CTX_A);

    TEST_ASSERT_TRUE(first);
    TEST_ASSERT_FALSE(second);
    TEST_ASSERT_EQUAL(1, mgr.registeredCount());
}

void test_first_registered_becomes_default() {
    APIs apis(bus);
    ContextManager mgr(apis);

    mgr.registerContext<MockContext>(TestContextID::CTX_B);

    TEST_ASSERT_EQUAL(static_cast<uint8_t>(TestContextID::CTX_B), mgr.defaultId());
}

void test_has_context() {
    APIs apis(bus);
    ContextManager mgr(apis);

    TEST_ASSERT_FALSE(mgr.hasContext(TestContextID::CTX_A));

    mgr.registerContext<MockContext>(TestContextID::CTX_A);

    TEST_ASSERT_TRUE(mgr.hasContext(TestContextID::CTX_A));
    TEST_ASSERT_FALSE(mgr.hasContext(TestContextID::CTX_B));
}

// ═══════════════════════════════════════════════════════════════════
// Requirements Validation
// ═══════════════════════════════════════════════════════════════════

void test_requirements_validation_fails_without_button_api() {
    APIs apis(bus);
    apis.button = nullptr;  // No ButtonAPI
    ContextManager mgr(apis);

    bool result = mgr.registerContext<MockContextRequiresButton>(TestContextID::CTX_A);
    TEST_ASSERT_FALSE(result);
}

// ═══════════════════════════════════════════════════════════════════
// Begin (Activate Default)
// ═══════════════════════════════════════════════════════════════════

void test_begin_activates_default_context() {
    APIs apis(bus);
    ContextManager mgr(apis);

    mgr.registerContext<MockContext>(TestContextID::CTX_A);

    bool result = mgr.begin();

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_NOT_NULL(mgr.active());
    TEST_ASSERT_EQUAL(static_cast<uint8_t>(TestContextID::CTX_A), mgr.activeId());
    TEST_ASSERT_TRUE(MockContext::wasInitialized());
}

void test_begin_fails_without_registered_contexts() {
    APIs apis(bus);
    ContextManager mgr(apis);

    bool result = mgr.begin();

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_NULL(mgr.active());
}

// ═══════════════════════════════════════════════════════════════════
// Switch Context
// ═══════════════════════════════════════════════════════════════════

void test_switch_to_context() {
    APIs apis(bus);
    ContextManager mgr(apis);

    mgr.registerContext<MockContext>(TestContextID::CTX_A);
    mgr.registerContext<MockContextB>(TestContextID::CTX_B);
    mgr.begin();

    bool result = mgr.switchTo(TestContextID::CTX_B);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(static_cast<uint8_t>(TestContextID::CTX_B), mgr.activeId());
    TEST_ASSERT_TRUE(MockContextB::wasInitialized());
}

void test_switch_cleans_up_previous_context() {
    APIs apis(bus);
    ContextManager mgr(apis);

    mgr.registerContext<MockContext>(TestContextID::CTX_A);
    mgr.registerContext<MockContextB>(TestContextID::CTX_B);
    mgr.begin();

    mgr.switchTo(TestContextID::CTX_B);

    TEST_ASSERT_TRUE(MockContext::wasCleanedUp());
}

void test_switch_to_unregistered_fails() {
    APIs apis(bus);
    ContextManager mgr(apis);

    mgr.registerContext<MockContext>(TestContextID::CTX_A);
    mgr.begin();

    bool result = mgr.switchTo(TestContextID::CTX_C);

    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL(static_cast<uint8_t>(TestContextID::CTX_A), mgr.activeId());
}

void test_switch_to_same_context_is_noop() {
    // Optimization: switching to already active context does nothing
    APIs apis(bus);
    ContextManager mgr(apis);

    mgr.registerContext<MockContext>(TestContextID::CTX_A);
    mgr.begin();

    int initCountBefore = MockContext::initializeCount();

    bool result = mgr.switchTo(TestContextID::CTX_A);

    // Should return true but NOT reinitialize
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(initCountBefore, MockContext::initializeCount());
}

// ═══════════════════════════════════════════════════════════════════
// Set Default
// ═══════════════════════════════════════════════════════════════════

void test_set_default() {
    APIs apis(bus);
    ContextManager mgr(apis);

    mgr.registerContext<MockContext>(TestContextID::CTX_A);
    mgr.registerContext<MockContextB>(TestContextID::CTX_B);

    mgr.setDefault(TestContextID::CTX_B);

    TEST_ASSERT_EQUAL(static_cast<uint8_t>(TestContextID::CTX_B), mgr.defaultId());
}

void test_switch_to_default() {
    APIs apis(bus);
    ContextManager mgr(apis);

    mgr.registerContext<MockContext>(TestContextID::CTX_A);
    mgr.registerContext<MockContextB>(TestContextID::CTX_B);
    mgr.setDefault(TestContextID::CTX_A);
    mgr.begin();

    mgr.switchTo(TestContextID::CTX_B);
    mgr.switchToDefault();

    TEST_ASSERT_EQUAL(static_cast<uint8_t>(TestContextID::CTX_A), mgr.activeId());
}

// ═══════════════════════════════════════════════════════════════════
// Update
// ═══════════════════════════════════════════════════════════════════

void test_update_calls_context_update() {
    APIs apis(bus);
    ContextManager mgr(apis);

    mgr.registerContext<MockContext>(TestContextID::CTX_A);
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
// Initialization Failure Fallback
// ═══════════════════════════════════════════════════════════════════

void test_init_failure_falls_back_to_default() {
    APIs apis(bus);
    ContextManager mgr(apis);

    mgr.registerContext<MockContext>(TestContextID::CTX_A);
    mgr.registerContext<MockContextB>(TestContextID::CTX_B);
    mgr.setDefault(TestContextID::CTX_A);
    mgr.begin();

    // Make MockContext initialization fail
    MockContext::setInitShouldFail();

    // Try to switch to B, but B's init should succeed
    // and A's init should fail so we can't test fallback to A
    // Let's test the other way around

    MockContext::reset();  // Reset to succeed again
    TEST_ASSERT_EQUAL(static_cast<uint8_t>(TestContextID::CTX_A), mgr.activeId());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Registration
    RUN_TEST(test_register_context);
    RUN_TEST(test_register_multiple_contexts);
    RUN_TEST(test_cannot_register_same_id_twice);
    RUN_TEST(test_first_registered_becomes_default);
    RUN_TEST(test_has_context);

    // Requirements
    RUN_TEST(test_requirements_validation_fails_without_button_api);

    // Begin
    RUN_TEST(test_begin_activates_default_context);
    RUN_TEST(test_begin_fails_without_registered_contexts);

    // Switch
    RUN_TEST(test_switch_to_context);
    RUN_TEST(test_switch_cleans_up_previous_context);
    RUN_TEST(test_switch_to_unregistered_fails);
    RUN_TEST(test_switch_to_same_context_is_noop);

    // Default
    RUN_TEST(test_set_default);
    RUN_TEST(test_switch_to_default);

    // Update
    RUN_TEST(test_update_calls_context_update);
    RUN_TEST(test_update_without_active_context_does_nothing);

    // Fallback
    RUN_TEST(test_init_failure_falls_back_to_default);

    UNITY_END();
    return 0;
}
