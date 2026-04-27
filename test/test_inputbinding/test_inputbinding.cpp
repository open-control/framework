#include <unity.h>
#include <vector>

#include <oc/Config.hpp>
#include <oc/core/input/InputBinding.hpp>
#include <oc/core/input/AuthorityResolver.hpp>
#include <oc/core/event/Events.hpp>
#include "../mocks/MockEventBus.hpp"
#include "../mocks/FakeTime.hpp"

using namespace oc::core::input;
using namespace oc::core::event;
using namespace oc::core;
using namespace oc::test;

static MockEventBus bus;
static FakeTime fakeTime;
static int actionCount = 0;
static float lastEncoderValue = 0.0f;

void setUp() {
    bus.reset();
    fakeTime.reset();
    actionCount = 0;
    lastEncoderValue = 0.0f;
}

void tearDown() {}

// ═══════════════════════════════════════════════════════════════════
// Basic Registration
// ═══════════════════════════════════════════════════════════════════

void test_register_button_binding_returns_id() {
    InputBinding binding(bus, fakeTime.provider());

    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.action = []() { actionCount++; };

    oc::type::BindingID id = binding.registerButtonBinding(btn);
    TEST_ASSERT_GREATER_THAN(0, id);
}

void test_register_encoder_binding_returns_id() {
    InputBinding binding(bus, fakeTime.provider());

    EncoderBinding enc{};
    enc.type = EncoderBindingType::TURN;
    enc.encoderId = 0;
    enc.action = [](float v) { lastEncoderValue = v; };

    oc::type::BindingID id = binding.registerEncoderBinding(enc);
    TEST_ASSERT_GREATER_THAN(0, id);
}

void test_sequential_ids() {
    InputBinding binding(bus, fakeTime.provider());

    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.action = []() {};

    oc::type::BindingID id1 = binding.registerButtonBinding(btn);
    oc::type::BindingID id2 = binding.registerButtonBinding(btn);
    oc::type::BindingID id3 = binding.registerButtonBinding(btn);

    TEST_ASSERT_EQUAL(id1 + 1, id2);
    TEST_ASSERT_EQUAL(id2 + 1, id3);
}

// ═══════════════════════════════════════════════════════════════════
// Button Press Events
// ═══════════════════════════════════════════════════════════════════

void test_button_press_triggers_action() {
    InputBinding binding(bus, fakeTime.provider());

    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    bus.emit(ButtonPressEvent{1, true});

    TEST_ASSERT_EQUAL(1, actionCount);
}

void test_button_press_wrong_id_no_trigger() {
    InputBinding binding(bus, fakeTime.provider());

    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    bus.emit(ButtonPressEvent{2, true});  // Different button

    TEST_ASSERT_EQUAL(0, actionCount);
}

void test_button_release_triggers_action() {
    InputBinding binding(bus, fakeTime.provider());

    ButtonBinding btn{};
    btn.type = ButtonBindingType::RELEASE;
    btn.buttonId = 1;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    // Press then release
    bus.emit(ButtonPressEvent{1, true});
    bus.emit(ButtonReleaseEvent{1});

    TEST_ASSERT_EQUAL(1, actionCount);
}

// ═══════════════════════════════════════════════════════════════════
// Long Press Detection
// ═══════════════════════════════════════════════════════════════════

void test_long_press_triggers_after_duration() {
    InputConfig config;
    config.longPressMs = 500;
    InputBinding binding(bus, fakeTime.provider(), config);

    ButtonBinding btn{};
    btn.type = ButtonBindingType::LONG_PRESS;
    btn.buttonId = 1;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    // Press button
    fakeTime.set(0);
    binding.processTick();
    bus.emit(ButtonPressEvent{1, true});

    // Not enough time
    fakeTime.set(400);
    binding.processTick();
    TEST_ASSERT_EQUAL(0, actionCount);

    // Enough time
    fakeTime.set(600);
    binding.processTick();
    TEST_ASSERT_EQUAL(1, actionCount);
}

void test_long_press_only_triggers_once() {
    InputConfig config;
    config.longPressMs = 500;
    InputBinding binding(bus, fakeTime.provider(), config);

    ButtonBinding btn{};
    btn.type = ButtonBindingType::LONG_PRESS;
    btn.buttonId = 1;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    fakeTime.set(0);
    binding.processTick();
    bus.emit(ButtonPressEvent{1, true});

    fakeTime.set(600);
    binding.processTick();
    binding.processTick();
    binding.processTick();

    TEST_ASSERT_EQUAL(1, actionCount);  // Should only trigger once
}

void test_long_press_custom_duration() {
    InputConfig config;
    config.longPressMs = 500;  // Default
    InputBinding binding(bus, fakeTime.provider(), config);

    ButtonBinding btn{};
    btn.type = ButtonBindingType::LONG_PRESS;
    btn.buttonId = 1;
    btn.longPressMs = 1000;  // Custom: 1 second
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    fakeTime.set(0);
    binding.processTick();
    bus.emit(ButtonPressEvent{1, true});

    fakeTime.set(600);
    binding.processTick();
    TEST_ASSERT_EQUAL(0, actionCount);  // Not yet

    fakeTime.set(1100);
    binding.processTick();
    TEST_ASSERT_EQUAL(1, actionCount);  // Now triggers
}

// ═══════════════════════════════════════════════════════════════════
// Double Tap Detection
// ═══════════════════════════════════════════════════════════════════

void test_double_tap_triggers() {
    InputConfig config;
    config.doubleTapWindowMs = 300;
    InputBinding binding(bus, fakeTime.provider(), config);

    ButtonBinding btn{};
    btn.type = ButtonBindingType::DOUBLE_TAP;
    btn.buttonId = 1;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    // First tap
    fakeTime.set(0);
    binding.processTick();
    bus.emit(ButtonPressEvent{1, true});
    fakeTime.set(50);
    binding.processTick();
    bus.emit(ButtonReleaseEvent{1});

    // Second tap within window
    fakeTime.set(150);
    binding.processTick();
    bus.emit(ButtonPressEvent{1, true});
    fakeTime.set(200);
    binding.processTick();
    bus.emit(ButtonReleaseEvent{1});

    TEST_ASSERT_EQUAL(1, actionCount);
}

void test_double_tap_outside_window_no_trigger() {
    InputConfig config;
    config.doubleTapWindowMs = 300;
    InputBinding binding(bus, fakeTime.provider(), config);

    ButtonBinding btn{};
    btn.type = ButtonBindingType::DOUBLE_TAP;
    btn.buttonId = 1;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    // First tap
    fakeTime.set(0);
    binding.processTick();
    bus.emit(ButtonPressEvent{1, true});
    fakeTime.set(50);
    binding.processTick();
    bus.emit(ButtonReleaseEvent{1});

    // Second tap OUTSIDE window
    fakeTime.set(500);
    binding.processTick();
    bus.emit(ButtonPressEvent{1, true});
    fakeTime.set(550);
    binding.processTick();
    bus.emit(ButtonReleaseEvent{1});

    TEST_ASSERT_EQUAL(0, actionCount);
}

// ═══════════════════════════════════════════════════════════════════
// Encoder Events
// ═══════════════════════════════════════════════════════════════════

void test_encoder_turn_triggers_action() {
    InputBinding binding(bus, fakeTime.provider());

    EncoderBinding enc{};
    enc.type = EncoderBindingType::TURN;
    enc.encoderId = 0;
    enc.action = [](float v) {
        actionCount++;
        lastEncoderValue = v;
    };
    binding.registerEncoderBinding(enc);

    bus.emit(EncoderChangedEvent{0, 0.75f});

    TEST_ASSERT_EQUAL(1, actionCount);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.75f, lastEncoderValue);
}

void test_encoder_turn_while_pressed() {
    InputBinding binding(bus, fakeTime.provider());

    EncoderBinding enc{};
    enc.type = EncoderBindingType::TURN_WHILE_PRESSED;
    enc.encoderId = 0;
    enc.requiredButton = 1;
    enc.action = [](float v) { actionCount++; };
    binding.registerEncoderBinding(enc);

    // Turn without button pressed - no trigger
    bus.emit(EncoderChangedEvent{0, 0.5f});
    TEST_ASSERT_EQUAL(0, actionCount);

    // Press button, then turn
    bus.emit(ButtonPressEvent{1, true});
    bus.emit(EncoderChangedEvent{0, 0.6f});
    TEST_ASSERT_EQUAL(1, actionCount);
}

// ═══════════════════════════════════════════════════════════════════
// Remove Binding
// ═══════════════════════════════════════════════════════════════════

void test_remove_by_id() {
    InputBinding binding(bus, fakeTime.provider());

    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.action = []() { actionCount++; };
    oc::type::BindingID id = binding.registerButtonBinding(btn);

    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(1, actionCount);

    bool removed = binding.removeById(id);
    TEST_ASSERT_TRUE(removed);

    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(1, actionCount);  // No longer triggers
}

void test_remove_invalid_id() {
    InputBinding binding(bus, fakeTime.provider());

    bool removed = binding.removeById(9999);
    TEST_ASSERT_FALSE(removed);
}

// ═══════════════════════════════════════════════════════════════════
// Clear Bindings
// ═══════════════════════════════════════════════════════════════════

void test_clear_all_bindings() {
    InputBinding binding(bus, fakeTime.provider());

    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);
    binding.registerButtonBinding(btn);

    binding.clearBindings();

    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(0, actionCount);
}

void test_clear_button_bindings_only() {
    InputBinding binding(bus, fakeTime.provider());

    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    EncoderBinding enc{};
    enc.type = EncoderBindingType::TURN;
    enc.encoderId = 0;
    enc.action = [](float) { actionCount += 10; };
    binding.registerEncoderBinding(enc);

    binding.clearButtonBindings();

    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(0, actionCount);

    bus.emit(EncoderChangedEvent{0, 0.5f});
    TEST_ASSERT_EQUAL(10, actionCount);  // Encoder still works
}

// ═══════════════════════════════════════════════════════════════════
// Scoped Bindings
// ═══════════════════════════════════════════════════════════════════

void test_scoped_binding_priority() {
    InputBinding binding(bus, fakeTime.provider());
    int globalCount = 0;
    int scopedCount = 0;

    // Global binding
    ButtonBinding global{};
    global.type = ButtonBindingType::PRESS;
    global.buttonId = 1;
    global.scopeId = 0;  // Global
    global.action = [&]() { globalCount++; };
    binding.registerButtonBinding(global);

    // Scoped binding (higher priority)
    ButtonBinding scoped{};
    scoped.type = ButtonBindingType::PRESS;
    scoped.buttonId = 1;
    scoped.scopeId = 42;  // Scoped
    scoped.action = [&]() { scopedCount++; };
    binding.registerButtonBinding(scoped);

    bus.emit(ButtonPressEvent{1, true});

    TEST_ASSERT_EQUAL(1, scopedCount);
    TEST_ASSERT_EQUAL(0, globalCount);  // Blocked by scoped
}

void test_clear_scope() {
    InputBinding binding(bus, fakeTime.provider());

    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.scopeId = 42;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);
    binding.registerButtonBinding(btn);

    bus.emit(ButtonPressEvent{1, true});
    // Only 1 trigger: scoped bindings stop after first match to prevent race conditions
    TEST_ASSERT_EQUAL(1, actionCount);

    binding.clearScope(42);

    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(1, actionCount);  // No more triggers
}

// ═══════════════════════════════════════════════════════════════════
// IsActive Predicate
// ═══════════════════════════════════════════════════════════════════

void test_is_active_predicate() {
    InputBinding binding(bus, fakeTime.provider());
    bool isActive = false;

    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.scopeId = 1;  // Must have scope for predicate to be checked
    btn.isActive = [&]() { return isActive; };
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    // Not active - no trigger
    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(0, actionCount);

    // Now active
    isActive = true;
    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(1, actionCount);
}

// ═══════════════════════════════════════════════════════════════════
// Enable/Disable
// ═══════════════════════════════════════════════════════════════════

void test_set_bindings_enabled() {
    InputBinding binding(bus, fakeTime.provider());

    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    binding.setBindingsEnabled(false);
    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(0, actionCount);

    binding.setBindingsEnabled(true);
    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(1, actionCount);
}

// ═══════════════════════════════════════════════════════════════════
// Button State
// ═══════════════════════════════════════════════════════════════════

void test_is_button_pressed() {
    InputBinding binding(bus, fakeTime.provider());

    TEST_ASSERT_FALSE(binding.isButtonPressed(1));

    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_TRUE(binding.isButtonPressed(1));

    bus.emit(ButtonReleaseEvent{1});
    TEST_ASSERT_FALSE(binding.isButtonPressed(1));
}

// ═══════════════════════════════════════════════════════════════════
// Latch State
// ═══════════════════════════════════════════════════════════════════

void test_latch_state_via_binding() {
    InputBinding binding(bus, fakeTime.provider());

    // Initially not latched
    TEST_ASSERT_FALSE(binding.isLatched(1));

    // Register a latch binding
    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.latch = true;
    btn.scopeId = 100;  // Scoped binding
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    // Press and quick release (< latchThresholdMs) should activate latch
    fakeTime.set(0);
    bus.emit(ButtonPressEvent{1, true});

    fakeTime.set(100);  // 100ms < 300ms threshold
    bus.emit(ButtonReleaseEvent{1});

    TEST_ASSERT_TRUE(binding.isLatched(1));

    // clearLatch should release it
    binding.clearLatch(1);
    TEST_ASSERT_FALSE(binding.isLatched(1));
}

void test_clearLatchesForScope() {
    InputBinding binding(bus, fakeTime.provider());

    // Register latch bindings for two different scopes
    ButtonBinding btn1{};
    btn1.type = ButtonBindingType::PRESS;
    btn1.buttonId = 1;
    btn1.latch = true;
    btn1.scopeId = 100;
    btn1.action = []() {};
    binding.registerButtonBinding(btn1);

    ButtonBinding btn2{};
    btn2.type = ButtonBindingType::PRESS;
    btn2.buttonId = 2;
    btn2.latch = true;
    btn2.scopeId = 200;
    btn2.action = []() {};
    binding.registerButtonBinding(btn2);

    // Activate both latches
    fakeTime.set(0);
    bus.emit(ButtonPressEvent{1, true});
    fakeTime.set(100);
    bus.emit(ButtonReleaseEvent{1});

    fakeTime.set(200);
    bus.emit(ButtonPressEvent{2, true});
    fakeTime.set(300);
    bus.emit(ButtonReleaseEvent{2});

    TEST_ASSERT_TRUE(binding.isLatched(1));
    TEST_ASSERT_TRUE(binding.isLatched(2));

    // Clear latches for scope 100 only
    binding.clearLatchesForScope(100);

    TEST_ASSERT_FALSE(binding.isLatched(1));  // Cleared
    TEST_ASSERT_TRUE(binding.isLatched(2));   // Still latched (different scope)
}

// ═══════════════════════════════════════════════════════════════════
// Binding Priority Tests
// ═══════════════════════════════════════════════════════════════════

void test_priority_higher_triggers_first() {
    InputBinding binding(bus, fakeTime.provider());
    std::vector<int> order;

    // Register low priority first
    ButtonBinding low{};
    low.type = ButtonBindingType::PRESS;
    low.buttonId = 1;
    low.scopeId = 1;  // Same scope
    low.priority = 0;
    low.action = [&]() { order.push_back(0); };
    binding.registerButtonBinding(low);

    // Register high priority second
    ButtonBinding high{};
    high.type = ButtonBindingType::PRESS;
    high.buttonId = 1;
    high.scopeId = 1;  // Same scope
    high.priority = 10;
    high.action = [&]() { order.push_back(10); };
    binding.registerButtonBinding(high);

    bus.emit(ButtonPressEvent{1, true});

    // Only highest priority triggers (scoped bindings stop after first match)
    TEST_ASSERT_EQUAL(1, order.size());
    TEST_ASSERT_EQUAL(10, order[0]);
}

void test_priority_negative_lower() {
    InputBinding binding(bus, fakeTime.provider());
    std::vector<int> order;

    // Register normal priority
    ButtonBinding normal{};
    normal.type = ButtonBindingType::PRESS;
    normal.buttonId = 1;
    normal.scopeId = 1;
    normal.priority = 0;
    normal.action = [&]() { order.push_back(0); };
    binding.registerButtonBinding(normal);

    // Register negative (lower) priority
    ButtonBinding negative{};
    negative.type = ButtonBindingType::PRESS;
    negative.buttonId = 1;
    negative.scopeId = 1;
    negative.priority = -5;
    negative.action = [&]() { order.push_back(-5); };
    binding.registerButtonBinding(negative);

    bus.emit(ButtonPressEvent{1, true});

    // Normal priority (0) triggers first
    TEST_ASSERT_EQUAL(1, order.size());
    TEST_ASSERT_EQUAL(0, order[0]);
}

void test_priority_encoder_bindings() {
    InputBinding binding(bus, fakeTime.provider());
    std::vector<int> order;

    // Low priority encoder
    EncoderBinding low{};
    low.type = EncoderBindingType::TURN;
    low.encoderId = 0;
    low.scopeId = 1;
    low.priority = 0;
    low.action = [&](float) { order.push_back(0); };
    binding.registerEncoderBinding(low);

    // High priority encoder
    EncoderBinding high{};
    high.type = EncoderBindingType::TURN;
    high.encoderId = 0;
    high.scopeId = 1;
    high.priority = 5;
    high.action = [&](float) { order.push_back(5); };
    binding.registerEncoderBinding(high);

    bus.emit(EncoderChangedEvent{0, 0.5f});

    // Higher priority triggers first
    TEST_ASSERT_EQUAL(1, order.size());
    TEST_ASSERT_EQUAL(5, order[0]);
}

void test_priority_global_bindings_all_trigger() {
    InputBinding binding(bus, fakeTime.provider());
    int count = 0;

    // Global bindings (scopeId = 0) all trigger regardless of priority
    ButtonBinding btn1{};
    btn1.type = ButtonBindingType::PRESS;
    btn1.buttonId = 1;
    btn1.scopeId = 0;  // Global
    btn1.priority = 0;
    btn1.action = [&]() { count++; };
    binding.registerButtonBinding(btn1);

    ButtonBinding btn2{};
    btn2.type = ButtonBindingType::PRESS;
    btn2.buttonId = 1;
    btn2.scopeId = 0;  // Global
    btn2.priority = 10;
    btn2.action = [&]() { count++; };
    binding.registerButtonBinding(btn2);

    bus.emit(ButtonPressEvent{1, true});

    // Both global bindings trigger
    TEST_ASSERT_EQUAL(2, count);
}

void test_priority_default_is_zero() {
    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;

    TEST_ASSERT_EQUAL(0, btn.priority);

    EncoderBinding enc{};
    enc.type = EncoderBindingType::TURN;
    enc.encoderId = 0;

    TEST_ASSERT_EQUAL(0, enc.priority);
}

// ═══════════════════════════════════════════════════════════════════
// Combo Bindings
// ═══════════════════════════════════════════════════════════════════

void test_combo_triggers_on_release() {
    InputBinding binding(bus, fakeTime.provider());

    ButtonBinding combo{};
    combo.type = ButtonBindingType::COMBO;
    combo.buttonId = 1;
    combo.secondaryButton = 2;
    combo.action = []() { actionCount++; };
    binding.registerButtonBinding(combo);

    // Press both buttons
    bus.emit(ButtonPressEvent{1, true});
    bus.emit(ButtonPressEvent{2, true});
    TEST_ASSERT_EQUAL(0, actionCount);  // Not triggered yet

    // Release one - combo triggers
    bus.emit(ButtonReleaseEvent{1});
    TEST_ASSERT_EQUAL(1, actionCount);
}

void test_combo_wrong_buttons_no_trigger() {
    InputBinding binding(bus, fakeTime.provider());

    ButtonBinding combo{};
    combo.type = ButtonBindingType::COMBO;
    combo.buttonId = 1;
    combo.secondaryButton = 2;
    combo.action = []() { actionCount++; };
    binding.registerButtonBinding(combo);

    // Press wrong buttons
    bus.emit(ButtonPressEvent{1, true});
    bus.emit(ButtonPressEvent{3, true});  // Wrong button
    bus.emit(ButtonReleaseEvent{1});

    TEST_ASSERT_EQUAL(0, actionCount);
}

void test_combo_scoped_priority() {
    InputBinding binding(bus, fakeTime.provider());
    int globalCount = 0;
    int scopedCount = 0;

    // Global combo
    ButtonBinding globalCombo{};
    globalCombo.type = ButtonBindingType::COMBO;
    globalCombo.buttonId = 1;
    globalCombo.secondaryButton = 2;
    globalCombo.scopeId = 0;
    globalCombo.action = [&]() { globalCount++; };
    binding.registerButtonBinding(globalCombo);

    // Scoped combo
    ButtonBinding scopedCombo{};
    scopedCombo.type = ButtonBindingType::COMBO;
    scopedCombo.buttonId = 1;
    scopedCombo.secondaryButton = 2;
    scopedCombo.scopeId = 42;
    scopedCombo.action = [&]() { scopedCount++; };
    binding.registerButtonBinding(scopedCombo);

    bus.emit(ButtonPressEvent{1, true});
    bus.emit(ButtonPressEvent{2, true});
    bus.emit(ButtonReleaseEvent{1});

    TEST_ASSERT_EQUAL(1, scopedCount);
    TEST_ASSERT_EQUAL(0, globalCount);  // Blocked by scoped
}

// ═══════════════════════════════════════════════════════════════════
// Authority Resolver Integration
// ═══════════════════════════════════════════════════════════════════

void test_authority_resolver_blocks_non_authority_scope() {
    InputBinding binding(bus, fakeTime.provider());
    AuthorityResolver resolver;

    oc::type::ScopeID currentAuthority = 100;
    resolver.setOverlayProvider([&]() { return currentAuthority; });
    binding.setAuthorityResolver(&resolver);

    // Binding in scope 200 (NOT the authority)
    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.scopeId = 200;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(0, actionCount);  // Blocked - wrong scope
}

void test_authority_resolver_allows_authority_scope() {
    InputBinding binding(bus, fakeTime.provider());
    AuthorityResolver resolver;

    oc::type::ScopeID currentAuthority = 100;
    resolver.setOverlayProvider([&]() { return currentAuthority; });
    binding.setAuthorityResolver(&resolver);

    // Binding in scope 100 (the authority)
    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.scopeId = 100;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(1, actionCount);  // Allowed
}

void test_authority_resolver_null_allows_all() {
    InputBinding binding(bus, fakeTime.provider());

    // No resolver set - all scopes allowed
    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.scopeId = 999;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(1, actionCount);
}

void test_authority_zero_allows_all_scopes() {
    InputBinding binding(bus, fakeTime.provider());
    AuthorityResolver resolver;

    // Authority = 0 means no overlay active
    resolver.setOverlayProvider([]() { return oc::type::ScopeID(0); });
    binding.setAuthorityResolver(&resolver);

    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.scopeId = 42;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(1, actionCount);  // Allowed when authority = 0
}

void test_authority_resolver_gates_long_press_scoped_bindings() {
    InputConfig config;
    config.longPressMs = 500;
    InputBinding binding(bus, fakeTime.provider(), config);
    AuthorityResolver resolver;

    oc::type::ScopeID currentAuthority = 100;
    resolver.setOverlayProvider([&]() { return currentAuthority; });
    binding.setAuthorityResolver(&resolver);

    int nonAuth = 0;
    int auth = 0;

    // Register NON-authority binding first to ensure it can't steal.
    ButtonBinding non{};
    non.type = ButtonBindingType::LONG_PRESS;
    non.buttonId = 1;
    non.scopeId = 200;
    non.action = [&]() { nonAuth++; };
    binding.registerButtonBinding(non);

    ButtonBinding ok{};
    ok.type = ButtonBindingType::LONG_PRESS;
    ok.buttonId = 1;
    ok.scopeId = 100;
    ok.action = [&]() { auth++; };
    binding.registerButtonBinding(ok);

    // Press button
    fakeTime.set(0);
    binding.processTick();
    bus.emit(ButtonPressEvent{1, true});

    // Trigger long press
    fakeTime.set(600);
    binding.processTick();

    TEST_ASSERT_EQUAL(1, auth);
    TEST_ASSERT_EQUAL(0, nonAuth);
}

void test_authority_resolver_gates_double_tap_scoped_bindings() {
    InputConfig config;
    config.doubleTapWindowMs = 300;
    InputBinding binding(bus, fakeTime.provider(), config);
    AuthorityResolver resolver;

    oc::type::ScopeID currentAuthority = 100;
    resolver.setOverlayProvider([&]() { return currentAuthority; });
    binding.setAuthorityResolver(&resolver);

    int nonAuth = 0;
    int auth = 0;

    // Register NON-authority binding first to ensure it can't steal.
    ButtonBinding non{};
    non.type = ButtonBindingType::DOUBLE_TAP;
    non.buttonId = 1;
    non.scopeId = 200;
    non.action = [&]() { nonAuth++; };
    binding.registerButtonBinding(non);

    ButtonBinding ok{};
    ok.type = ButtonBindingType::DOUBLE_TAP;
    ok.buttonId = 1;
    ok.scopeId = 100;
    ok.action = [&]() { auth++; };
    binding.registerButtonBinding(ok);

    // First tap
    fakeTime.set(0);
    binding.processTick();
    bus.emit(ButtonPressEvent{1, true});
    fakeTime.set(50);
    binding.processTick();
    bus.emit(ButtonReleaseEvent{1});

    // Second tap within window
    fakeTime.set(150);
    binding.processTick();
    bus.emit(ButtonPressEvent{1, true});
    fakeTime.set(200);
    binding.processTick();
    bus.emit(ButtonReleaseEvent{1});

    TEST_ASSERT_EQUAL(1, auth);
    TEST_ASSERT_EQUAL(0, nonAuth);
}

void test_authority_resolver_gates_combo_scoped_bindings() {
    InputBinding binding(bus, fakeTime.provider());
    AuthorityResolver resolver;

    oc::type::ScopeID currentAuthority = 100;
    resolver.setOverlayProvider([&]() { return currentAuthority; });
    binding.setAuthorityResolver(&resolver);

    int nonAuth = 0;
    int auth = 0;

    // Register NON-authority binding first to ensure it can't steal.
    ButtonBinding non{};
    non.type = ButtonBindingType::COMBO;
    non.buttonId = 1;
    non.secondaryButton = 2;
    non.scopeId = 200;
    non.action = [&]() { nonAuth++; };
    binding.registerButtonBinding(non);

    ButtonBinding ok{};
    ok.type = ButtonBindingType::COMBO;
    ok.buttonId = 1;
    ok.secondaryButton = 2;
    ok.scopeId = 100;
    ok.action = [&]() { auth++; };
    binding.registerButtonBinding(ok);

    bus.emit(ButtonPressEvent{1, true});
    bus.emit(ButtonPressEvent{2, true});
    bus.emit(ButtonReleaseEvent{1});

    TEST_ASSERT_EQUAL(1, auth);
    TEST_ASSERT_EQUAL(0, nonAuth);
}

void test_release_owner_dispatch_ignores_authority_changes() {
    InputBinding binding(bus, fakeTime.provider());
    AuthorityResolver resolver;

    oc::type::ScopeID currentAuthority = 100;
    resolver.setOverlayProvider([&]() { return currentAuthority; });
    binding.setAuthorityResolver(&resolver);

    int ownerPress = 0;
    int ownerRelease = 0;
    int otherRelease = 0;

    ButtonBinding press{};
    press.type = ButtonBindingType::PRESS;
    press.buttonId = 1;
    press.scopeId = 100;
    press.action = [&]() { ownerPress++; };
    binding.registerButtonBinding(press);

    ButtonBinding ownerRel{};
    ownerRel.type = ButtonBindingType::RELEASE;
    ownerRel.buttonId = 1;
    ownerRel.scopeId = 100;
    ownerRel.action = [&]() { ownerRelease++; };
    binding.registerButtonBinding(ownerRel);

    ButtonBinding otherRel{};
    otherRel.type = ButtonBindingType::RELEASE;
    otherRel.buttonId = 1;
    otherRel.scopeId = 200;
    otherRel.action = [&]() { otherRelease++; };
    binding.registerButtonBinding(otherRel);

    fakeTime.set(0);
    binding.processTick();
    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(1, ownerPress);

    // Authority changes after press ownership capture.
    currentAuthority = 200;

    fakeTime.set(50);
    binding.processTick();
    bus.emit(ButtonReleaseEvent{1});

    TEST_ASSERT_EQUAL(1, ownerRelease);
    TEST_ASSERT_EQUAL(0, otherRelease);
}

void test_release_owner_only_blocks_cross_scope_fallback() {
    InputConfig config;
    config.releaseRoutingPolicy = ReleaseRoutingPolicy::OwnerOnly;
    InputBinding binding(bus, fakeTime.provider(), config);
    AuthorityResolver resolver;

    oc::type::ScopeID currentAuthority = 100;
    resolver.setOverlayProvider([&]() { return currentAuthority; });
    binding.setAuthorityResolver(&resolver);

    int ownerPress = 0;
    int fallbackRelease = 0;

    ButtonBinding press{};
    press.type = ButtonBindingType::PRESS;
    press.buttonId = 1;
    press.scopeId = 100;
    press.action = [&]() { ownerPress++; };
    binding.registerButtonBinding(press);

    // No release handler in owner scope on purpose.
    ButtonBinding fallback{};
    fallback.type = ButtonBindingType::RELEASE;
    fallback.buttonId = 1;
    fallback.scopeId = 200;
    fallback.action = [&]() { fallbackRelease++; };
    binding.registerButtonBinding(fallback);

    fakeTime.set(0);
    binding.processTick();
    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(1, ownerPress);

    // Switch authority so fallback scope is now active.
    currentAuthority = 200;

    fakeTime.set(40);
    binding.processTick();
    bus.emit(ButtonReleaseEvent{1});

    TEST_ASSERT_EQUAL(0, fallbackRelease);
}

void test_release_owner_then_fallback_uses_owner_first_policy() {
    InputConfig config;
    config.releaseRoutingPolicy = ReleaseRoutingPolicy::OwnerThenFallback;
    InputBinding binding(bus, fakeTime.provider(), config);
    AuthorityResolver resolver;

    oc::type::ScopeID currentAuthority = 100;
    resolver.setOverlayProvider([&]() { return currentAuthority; });
    binding.setAuthorityResolver(&resolver);

    int ownerPress = 0;
    int fallbackRelease = 0;

    ButtonBinding press{};
    press.type = ButtonBindingType::PRESS;
    press.buttonId = 1;
    press.scopeId = 100;
    press.action = [&]() { ownerPress++; };
    binding.registerButtonBinding(press);

    // No owner release handler.
    ButtonBinding fallback{};
    fallback.type = ButtonBindingType::RELEASE;
    fallback.buttonId = 1;
    fallback.scopeId = 200;
    fallback.action = [&]() { fallbackRelease++; };
    binding.registerButtonBinding(fallback);

    fakeTime.set(0);
    binding.processTick();
    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(1, ownerPress);

    currentAuthority = 200;

    fakeTime.set(40);
    binding.processTick();
    bus.emit(ButtonReleaseEvent{1});

    TEST_ASSERT_EQUAL(1, fallbackRelease);
}

void test_owner_only_does_not_regress_latch_release_cycle() {
    InputConfig config;
    config.releaseRoutingPolicy = ReleaseRoutingPolicy::OwnerOnly;
    config.latchThresholdMs = 300;
    InputBinding binding(bus, fakeTime.provider(), config);
    AuthorityResolver resolver;

    oc::type::ScopeID currentAuthority = 100;
    resolver.setOverlayProvider([&]() { return currentAuthority; });
    binding.setAuthorityResolver(&resolver);

    ButtonBinding latchBtn{};
    latchBtn.type = ButtonBindingType::PRESS;
    latchBtn.buttonId = 1;
    latchBtn.latch = true;
    latchBtn.scopeId = 100;
    latchBtn.action = []() {};
    binding.registerButtonBinding(latchBtn);

    // First quick press-release activates latch.
    fakeTime.set(0);
    binding.processTick();
    bus.emit(ButtonPressEvent{1, true});

    fakeTime.set(100);
    binding.processTick();
    bus.emit(ButtonReleaseEvent{1});
    TEST_ASSERT_TRUE(binding.isLatched(1));

    // Authority change should not prevent latch release cycle.
    currentAuthority = 200;

    fakeTime.set(200);
    binding.processTick();
    bus.emit(ButtonPressEvent{1, true});

    fakeTime.set(260);
    binding.processTick();
    bus.emit(ButtonReleaseEvent{1});
    TEST_ASSERT_FALSE(binding.isLatched(1));
}

void test_owner_then_fallback_latch_toggle_allows_cross_scope_release() {
    InputConfig config;
    config.releaseRoutingPolicy = ReleaseRoutingPolicy::OwnerThenFallback;
    config.latchThresholdMs = 300;
    InputBinding binding(bus, fakeTime.provider(), config);
    AuthorityResolver resolver;

    oc::type::ScopeID currentAuthority = 100;
    resolver.setOverlayProvider([&]() { return currentAuthority; });
    binding.setAuthorityResolver(&resolver);

    int openCount = 0;
    int closeCount = 0;

    // Typical overlay toggle pattern: latch-open in view scope.
    ButtonBinding open{};
    open.type = ButtonBindingType::PRESS;
    open.buttonId = 1;
    open.latch = true;
    open.scopeId = 100;
    open.action = [&]() { openCount++; };
    binding.registerButtonBinding(open);

    // Close on release in overlay scope (different from latch owner).
    ButtonBinding close{};
    close.type = ButtonBindingType::RELEASE;
    close.buttonId = 1;
    close.scopeId = 200;
    close.action = [&]() { closeCount++; };
    binding.registerButtonBinding(close);

    // First press-release activates latch and keeps overlay open.
    fakeTime.set(0);
    binding.processTick();
    bus.emit(ButtonPressEvent{1, true});

    fakeTime.set(100);
    binding.processTick();
    bus.emit(ButtonReleaseEvent{1});

    TEST_ASSERT_EQUAL(1, openCount);
    TEST_ASSERT_EQUAL(0, closeCount);
    TEST_ASSERT_TRUE(binding.isLatched(1));

    // Overlay now has authority.
    currentAuthority = 200;

    // Second press-release clears latch and must dispatch close release.
    fakeTime.set(200);
    binding.processTick();
    bus.emit(ButtonPressEvent{1, true});

    fakeTime.set(260);
    binding.processTick();
    bus.emit(ButtonReleaseEvent{1});

    TEST_ASSERT_EQUAL(1, closeCount);
    TEST_ASSERT_FALSE(binding.isLatched(1));
}

void test_press_owner_handoff_routes_release_to_new_scope() {
    InputConfig config;
    config.releaseRoutingPolicy = ReleaseRoutingPolicy::OwnerOnly;
    InputBinding binding(bus, fakeTime.provider(), config);
    AuthorityResolver resolver;

    // Keep authority on scope 100 for the whole sequence.
    resolver.setOverlayProvider([]() { return oc::type::ScopeID(100); });
    binding.setAuthorityResolver(&resolver);

    int ownerPress = 0;
    int handedRelease = 0;

    ButtonBinding press{};
    press.type = ButtonBindingType::PRESS;
    press.buttonId = 1;
    press.scopeId = 100;
    press.action = [&]() {
        ownerPress++;
        binding.setPressOwner(1, 200);
    };
    binding.registerButtonBinding(press);

    ButtonBinding release{};
    release.type = ButtonBindingType::RELEASE;
    release.buttonId = 1;
    release.scopeId = 200;
    release.action = [&]() { handedRelease++; };
    binding.registerButtonBinding(release);

    fakeTime.set(0);
    binding.processTick();
    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(1, ownerPress);

    fakeTime.set(20);
    binding.processTick();
    bus.emit(ButtonReleaseEvent{1});

    TEST_ASSERT_EQUAL(1, handedRelease);
}

// ═══════════════════════════════════════════════════════════════════
// Clear Encoder Bindings
// ═══════════════════════════════════════════════════════════════════

void test_clear_encoder_bindings_only() {
    InputBinding binding(bus, fakeTime.provider());

    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    EncoderBinding enc{};
    enc.type = EncoderBindingType::TURN;
    enc.encoderId = 0;
    enc.action = [](float) { actionCount += 10; };
    binding.registerEncoderBinding(enc);

    binding.clearEncoderBindings();

    bus.emit(EncoderChangedEvent{0, 0.5f});
    TEST_ASSERT_EQUAL(0, actionCount);  // Encoder cleared

    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(1, actionCount);  // Button still works
}

// ═══════════════════════════════════════════════════════════════════
// Clear Scoped Bindings (Button/Encoder separately)
// ═══════════════════════════════════════════════════════════════════

void test_clear_button_scope_only() {
    InputBinding binding(bus, fakeTime.provider());

    // Button in scope 42
    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.scopeId = 42;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    // Encoder in same scope 42
    EncoderBinding enc{};
    enc.type = EncoderBindingType::TURN;
    enc.encoderId = 0;
    enc.scopeId = 42;
    enc.action = [](float) { actionCount += 10; };
    binding.registerEncoderBinding(enc);

    binding.clearButtonScope(42);

    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(0, actionCount);  // Button cleared

    bus.emit(EncoderChangedEvent{0, 0.5f});
    TEST_ASSERT_EQUAL(10, actionCount);  // Encoder still works
}

void test_clear_encoder_scope_only() {
    InputBinding binding(bus, fakeTime.provider());

    // Button in scope 42
    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.scopeId = 42;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    // Encoder in same scope 42
    EncoderBinding enc{};
    enc.type = EncoderBindingType::TURN;
    enc.encoderId = 0;
    enc.scopeId = 42;
    enc.action = [](float) { actionCount += 10; };
    binding.registerEncoderBinding(enc);

    binding.clearEncoderScope(42);

    bus.emit(EncoderChangedEvent{0, 0.5f});
    TEST_ASSERT_EQUAL(0, actionCount);  // Encoder cleared

    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(1, actionCount);  // Button still works
}

// ═══════════════════════════════════════════════════════════════════
// Boundary Checks
// ═══════════════════════════════════════════════════════════════════

void test_button_id_out_of_range_ignored() {
    InputBinding binding(bus, fakeTime.provider());

    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    // oc::type::ButtonID >= MAX_BUTTONS should be ignored
    bus.emit(ButtonPressEvent{255, true});  // Very high ID
    TEST_ASSERT_EQUAL(0, actionCount);

    // Valid ID still works
    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(1, actionCount);
}

void test_is_button_pressed_out_of_range() {
    InputBinding binding(bus, fakeTime.provider());

    // Out of range should return false, not crash
    TEST_ASSERT_FALSE(binding.isButtonPressed(255));
}

void test_is_latched_out_of_range() {
    InputBinding binding(bus, fakeTime.provider());

    // Out of range should return false, not crash
    TEST_ASSERT_FALSE(binding.isLatched(255));
}

void test_clear_latch_out_of_range_no_crash() {
    InputBinding binding(bus, fakeTime.provider());

    // Should not crash
    binding.clearLatch(255);
    TEST_ASSERT_TRUE(true);  // If we get here, no crash
}

// ═══════════════════════════════════════════════════════════════════
// Latch Complex Flow
// ═══════════════════════════════════════════════════════════════════

void test_latch_blocks_press_allows_other_scopes() {
    InputBinding binding(bus, fakeTime.provider());
    int scope100Count = 0;
    int scope200Count = 0;

    // Latch binding in scope 100
    ButtonBinding latchBtn{};
    latchBtn.type = ButtonBindingType::PRESS;
    latchBtn.buttonId = 1;
    latchBtn.latch = true;
    latchBtn.scopeId = 100;
    latchBtn.action = [&]() { scope100Count++; };
    binding.registerButtonBinding(latchBtn);

    // Non-latch binding in scope 200
    ButtonBinding otherBtn{};
    otherBtn.type = ButtonBindingType::PRESS;
    otherBtn.buttonId = 1;
    otherBtn.scopeId = 200;
    otherBtn.action = [&]() { scope200Count++; };
    binding.registerButtonBinding(otherBtn);

    // First press/release activates latch
    fakeTime.set(0);
    bus.emit(ButtonPressEvent{1, true});
    fakeTime.set(100);
    bus.emit(ButtonReleaseEvent{1});

    TEST_ASSERT_EQUAL(1, scope100Count);
    TEST_ASSERT_TRUE(binding.isLatched(1));

    // Second press - latch scope blocked, other scope gets it
    fakeTime.set(200);
    bus.emit(ButtonPressEvent{1, true});

    TEST_ASSERT_EQUAL(1, scope100Count);  // Still 1 - blocked
    TEST_ASSERT_EQUAL(1, scope200Count);  // Got the press
}

void test_latch_release_triggers_release_binding() {
    InputBinding binding(bus, fakeTime.provider());
    int pressCount = 0;
    int releaseCount = 0;

    // Press binding with latch
    ButtonBinding press{};
    press.type = ButtonBindingType::PRESS;
    press.buttonId = 1;
    press.latch = true;
    press.scopeId = 100;
    press.action = [&]() { pressCount++; };
    binding.registerButtonBinding(press);

    // Release binding in same scope
    ButtonBinding release{};
    release.type = ButtonBindingType::RELEASE;
    release.buttonId = 1;
    release.scopeId = 100;
    release.action = [&]() { releaseCount++; };
    binding.registerButtonBinding(release);

    // Activate latch
    fakeTime.set(0);
    bus.emit(ButtonPressEvent{1, true});
    fakeTime.set(100);
    bus.emit(ButtonReleaseEvent{1});

    TEST_ASSERT_EQUAL(1, pressCount);
    TEST_ASSERT_EQUAL(0, releaseCount);  // No release yet - latched
    TEST_ASSERT_TRUE(binding.isLatched(1));

    // Release latch
    fakeTime.set(200);
    bus.emit(ButtonPressEvent{1, true});
    fakeTime.set(300);
    bus.emit(ButtonReleaseEvent{1});

    TEST_ASSERT_EQUAL(1, releaseCount);  // Release triggered
    TEST_ASSERT_FALSE(binding.isLatched(1));
}

void test_long_press_prevents_latch() {
    InputConfig config;
    config.longPressMs = 500;
    config.latchThresholdMs = 300;
    InputBinding binding(bus, fakeTime.provider(), config);

    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.latch = true;
    btn.scopeId = 100;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    // Long press (> latchThresholdMs)
    fakeTime.set(0);
    binding.processTick();
    bus.emit(ButtonPressEvent{1, true});

    fakeTime.set(400);  // > 300ms threshold
    binding.processTick();
    bus.emit(ButtonReleaseEvent{1});

    TEST_ASSERT_FALSE(binding.isLatched(1));  // Long press = no latch
}

// ═══════════════════════════════════════════════════════════════════
// Encoder with Latch
// ═══════════════════════════════════════════════════════════════════

void test_encoder_turn_while_latched() {
    InputBinding binding(bus, fakeTime.provider());

    // Latch binding for button
    ButtonBinding latchBtn{};
    latchBtn.type = ButtonBindingType::PRESS;
    latchBtn.buttonId = 1;
    latchBtn.latch = true;
    latchBtn.scopeId = 100;
    latchBtn.action = []() {};
    binding.registerButtonBinding(latchBtn);

    // Encoder requires button 1 pressed (or latched)
    EncoderBinding enc{};
    enc.type = EncoderBindingType::TURN_WHILE_PRESSED;
    enc.encoderId = 0;
    enc.requiredButton = 1;
    enc.action = [](float) { actionCount++; };
    binding.registerEncoderBinding(enc);

    // Without latch - no trigger
    bus.emit(EncoderChangedEvent{0, 0.5f});
    TEST_ASSERT_EQUAL(0, actionCount);

    // Activate latch
    fakeTime.set(0);
    bus.emit(ButtonPressEvent{1, true});
    fakeTime.set(100);
    bus.emit(ButtonReleaseEvent{1});

    TEST_ASSERT_TRUE(binding.isLatched(1));

    // Now encoder should work (button latched)
    bus.emit(EncoderChangedEvent{0, 0.6f});
    TEST_ASSERT_EQUAL(1, actionCount);
}

// ═══════════════════════════════════════════════════════════════════
// Binding Limits
// ═══════════════════════════════════════════════════════════════════

void test_max_button_bindings_returns_zero() {
    InputBinding binding(bus, fakeTime.provider());

    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.action = []() {};

    // Fill up to MAX_BUTTON_BINDINGS
    for (size_t i = 0; i < oc::MAX_BUTTON_BINDINGS; ++i) {
        oc::type::BindingID id = binding.registerButtonBinding(btn);
        TEST_ASSERT_GREATER_THAN(0, id);
    }

    // Next one should return 0
    oc::type::BindingID overflow = binding.registerButtonBinding(btn);
    TEST_ASSERT_EQUAL(0, overflow);
}

void test_max_encoder_bindings_returns_zero() {
    InputBinding binding(bus, fakeTime.provider());

    EncoderBinding enc{};
    enc.type = EncoderBindingType::TURN;
    enc.encoderId = 0;
    enc.action = [](float) {};

    // Fill up to MAX_ENCODER_BINDINGS
    for (size_t i = 0; i < oc::MAX_ENCODER_BINDINGS; ++i) {
        oc::type::BindingID id = binding.registerEncoderBinding(enc);
        TEST_ASSERT_GREATER_THAN(0, id);
    }

    // Next one should return 0
    oc::type::BindingID overflow = binding.registerEncoderBinding(enc);
    TEST_ASSERT_EQUAL(0, overflow);
}

// ═══════════════════════════════════════════════════════════════════
// Edge Cases
// ═══════════════════════════════════════════════════════════════════

void test_remove_by_id_zero_returns_false() {
    InputBinding binding(bus, fakeTime.provider());

    // ID 0 is invalid, should return false immediately
    bool removed = binding.removeById(0);
    TEST_ASSERT_FALSE(removed);
}

void test_config_getter() {
    InputConfig config;
    config.longPressMs = 1234;
    config.doubleTapWindowMs = 567;
    config.latchThresholdMs = 890;

    InputBinding binding(bus, fakeTime.provider(), config);

    TEST_ASSERT_EQUAL(1234, binding.config().longPressMs);
    TEST_ASSERT_EQUAL(567, binding.config().doubleTapWindowMs);
    TEST_ASSERT_EQUAL(890, binding.config().latchThresholdMs);
}

void test_no_time_provider_disables_gestures() {
    InputConfig config;
    config.longPressMs = 100;
    InputBinding binding(bus, nullptr, config);  // No oc::type::TimeProvider

    ButtonBinding btn{};
    btn.type = ButtonBindingType::LONG_PRESS;
    btn.buttonId = 1;
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    // Press and hold - but without oc::type::TimeProvider, current_time_ stays 0
    bus.emit(ButtonPressEvent{1, true});
    binding.processTick();
    binding.processTick();
    binding.processTick();

    // Long press should NOT trigger because time never advances
    TEST_ASSERT_EQUAL(0, actionCount);
}

void test_remove_encoder_binding_by_id() {
    InputBinding binding(bus, fakeTime.provider());

    EncoderBinding enc{};
    enc.type = EncoderBindingType::TURN;
    enc.encoderId = 0;
    enc.action = [](float) { actionCount++; };
    oc::type::BindingID id = binding.registerEncoderBinding(enc);

    bus.emit(EncoderChangedEvent{0, 0.5f});
    TEST_ASSERT_EQUAL(1, actionCount);

    bool removed = binding.removeById(id);
    TEST_ASSERT_TRUE(removed);

    bus.emit(EncoderChangedEvent{0, 0.6f});
    TEST_ASSERT_EQUAL(1, actionCount);  // No longer triggers
}

void test_disabled_binding_field() {
    InputBinding binding(bus, fakeTime.provider());

    ButtonBinding btn{};
    btn.type = ButtonBindingType::PRESS;
    btn.buttonId = 1;
    btn.enabled = false;  // Disabled
    btn.action = []() { actionCount++; };
    binding.registerButtonBinding(btn);

    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(0, actionCount);  // Disabled binding doesn't trigger
}

void test_encoder_scoped_vs_global() {
    InputBinding binding(bus, fakeTime.provider());
    int globalCount = 0;
    int scopedCount = 0;

    // Global encoder
    EncoderBinding global{};
    global.type = EncoderBindingType::TURN;
    global.encoderId = 0;
    global.scopeId = 0;
    global.action = [&](float) { globalCount++; };
    binding.registerEncoderBinding(global);

    // Scoped encoder
    EncoderBinding scoped{};
    scoped.type = EncoderBindingType::TURN;
    scoped.encoderId = 0;
    scoped.scopeId = 42;
    scoped.action = [&](float) { scopedCount++; };
    binding.registerEncoderBinding(scoped);

    bus.emit(EncoderChangedEvent{0, 0.5f});

    TEST_ASSERT_EQUAL(1, scopedCount);
    TEST_ASSERT_EQUAL(0, globalCount);  // Blocked by scoped
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Basic registration
    RUN_TEST(test_register_button_binding_returns_id);
    RUN_TEST(test_register_encoder_binding_returns_id);
    RUN_TEST(test_sequential_ids);

    // Button press
    RUN_TEST(test_button_press_triggers_action);
    RUN_TEST(test_button_press_wrong_id_no_trigger);
    RUN_TEST(test_button_release_triggers_action);

    // Long press
    RUN_TEST(test_long_press_triggers_after_duration);
    RUN_TEST(test_long_press_only_triggers_once);
    RUN_TEST(test_long_press_custom_duration);

    // Double tap
    RUN_TEST(test_double_tap_triggers);
    RUN_TEST(test_double_tap_outside_window_no_trigger);

    // Encoder
    RUN_TEST(test_encoder_turn_triggers_action);
    RUN_TEST(test_encoder_turn_while_pressed);

    // Remove
    RUN_TEST(test_remove_by_id);
    RUN_TEST(test_remove_invalid_id);

    // Clear
    RUN_TEST(test_clear_all_bindings);
    RUN_TEST(test_clear_button_bindings_only);

    // Scopes
    RUN_TEST(test_scoped_binding_priority);
    RUN_TEST(test_clear_scope);

    // Predicate
    RUN_TEST(test_is_active_predicate);

    // Enable/disable
    RUN_TEST(test_set_bindings_enabled);

    // State
    RUN_TEST(test_is_button_pressed);
    RUN_TEST(test_latch_state_via_binding);
    RUN_TEST(test_clearLatchesForScope);

    // Priority
    RUN_TEST(test_priority_higher_triggers_first);
    RUN_TEST(test_priority_negative_lower);
    RUN_TEST(test_priority_encoder_bindings);
    RUN_TEST(test_priority_global_bindings_all_trigger);
    RUN_TEST(test_priority_default_is_zero);

    // Combo
    RUN_TEST(test_combo_triggers_on_release);
    RUN_TEST(test_combo_wrong_buttons_no_trigger);
    RUN_TEST(test_combo_scoped_priority);

    // Authority Resolver
    RUN_TEST(test_authority_resolver_blocks_non_authority_scope);
    RUN_TEST(test_authority_resolver_allows_authority_scope);
    RUN_TEST(test_authority_resolver_null_allows_all);
    RUN_TEST(test_authority_zero_allows_all_scopes);
    RUN_TEST(test_authority_resolver_gates_long_press_scoped_bindings);
    RUN_TEST(test_authority_resolver_gates_double_tap_scoped_bindings);
    RUN_TEST(test_authority_resolver_gates_combo_scoped_bindings);
    RUN_TEST(test_release_owner_dispatch_ignores_authority_changes);
    RUN_TEST(test_release_owner_only_blocks_cross_scope_fallback);
    RUN_TEST(test_release_owner_then_fallback_uses_owner_first_policy);
    RUN_TEST(test_owner_only_does_not_regress_latch_release_cycle);
    RUN_TEST(test_owner_then_fallback_latch_toggle_allows_cross_scope_release);
    RUN_TEST(test_press_owner_handoff_routes_release_to_new_scope);

    // Clear encoder bindings
    RUN_TEST(test_clear_encoder_bindings_only);

    // Clear scoped bindings
    RUN_TEST(test_clear_button_scope_only);
    RUN_TEST(test_clear_encoder_scope_only);

    // Boundary checks
    RUN_TEST(test_button_id_out_of_range_ignored);
    RUN_TEST(test_is_button_pressed_out_of_range);
    RUN_TEST(test_is_latched_out_of_range);
    RUN_TEST(test_clear_latch_out_of_range_no_crash);

    // Latch complex flow
    RUN_TEST(test_latch_blocks_press_allows_other_scopes);
    RUN_TEST(test_latch_release_triggers_release_binding);
    RUN_TEST(test_long_press_prevents_latch);

    // Encoder with latch
    RUN_TEST(test_encoder_turn_while_latched);

    // Binding limits
    RUN_TEST(test_max_button_bindings_returns_zero);
    RUN_TEST(test_max_encoder_bindings_returns_zero);

    // Edge cases
    RUN_TEST(test_remove_by_id_zero_returns_false);
    RUN_TEST(test_config_getter);
    RUN_TEST(test_no_time_provider_disables_gestures);
    RUN_TEST(test_remove_encoder_binding_by_id);
    RUN_TEST(test_disabled_binding_field);
    RUN_TEST(test_encoder_scoped_vs_global);

    UNITY_END();
    return 0;
}
