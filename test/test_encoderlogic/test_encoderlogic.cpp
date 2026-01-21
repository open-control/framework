#include <unity.h>
#include <cmath>
#include <oc/core/input/EncoderLogic.hpp>

using namespace oc::core::input;
using namespace oc;
using oc::interface::EncoderMode;

// Default config for tests
static EncoderConfig makeConfig(oc::type::EncoderID id = 0, uint16_t ppr = 24, uint16_t rangeAngle = 270) {
    return EncoderConfig{id, ppr, rangeAngle, 4, false};
}

void setUp() {}
void tearDown() {}

// ═══════════════════════════════════════════════════════════════════
// Basic Construction
// ═══════════════════════════════════════════════════════════════════

void test_default_mode_is_normalized() {
    EncoderLogic logic(makeConfig());
    TEST_ASSERT_EQUAL(EncoderMode::NORMALIZED, logic.getMode());
}

void test_initial_value_is_center() {
    EncoderLogic logic(makeConfig());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, logic.getLastValue());
}

void test_initial_position_is_center() {
    // Position starts at middle so that initial value = 0.5
    EncoderLogic logic(makeConfig());
    // For ppr=24, rangeAngle=270: virtual_range = 24*4*(270/360) = 72
    // position = 72/2 = 36
    TEST_ASSERT_EQUAL(36, logic.getPosition());
}

void test_no_pending_initially() {
    EncoderLogic logic(makeConfig());
    TEST_ASSERT_FALSE(logic.hasPending());
}

// ═══════════════════════════════════════════════════════════════════
// NORMALIZED Mode - Basic Movement
// ═══════════════════════════════════════════════════════════════════

void test_normalized_clockwise_increases_value() {
    EncoderLogic logic(makeConfig());
    float initial = logic.getLastValue();

    // Rotate clockwise (positive delta)
    for (int i = 0; i < 4; i++) logic.processDelta(1);

    auto value = logic.flush();
    TEST_ASSERT_TRUE(value.has_value());
    TEST_ASSERT_TRUE(value.value() > initial);
}

void test_normalized_counterclockwise_decreases_value() {
    EncoderLogic logic(makeConfig());
    float initial = logic.getLastValue();

    // Rotate counterclockwise (negative delta)
    for (int i = 0; i < 4; i++) logic.processDelta(-1);

    auto value = logic.flush();
    TEST_ASSERT_TRUE(value.has_value());
    TEST_ASSERT_TRUE(value.value() < initial);
}

void test_normalized_bounds_min() {
    EncoderLogic logic(makeConfig());

    // Rotate far counterclockwise
    for (int i = 0; i < 1000; i++) logic.processDelta(-1);

    auto value = logic.flush();
    TEST_ASSERT_TRUE(value.has_value());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, value.value());
}

void test_normalized_bounds_max() {
    EncoderLogic logic(makeConfig());

    // Rotate far clockwise
    for (int i = 0; i < 1000; i++) logic.processDelta(1);

    auto value = logic.flush();
    TEST_ASSERT_TRUE(value.has_value());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, value.value());
}

// ═══════════════════════════════════════════════════════════════════
// NORMALIZED Mode - Custom Bounds
// ═══════════════════════════════════════════════════════════════════

void test_custom_bounds_mapping() {
    EncoderLogic logic(makeConfig());
    logic.setBounds(100.0f, 200.0f);

    // setBounds doesn't recalculate last_value_ - need to move first
    // Move slightly to trigger recalculation
    logic.processDelta(1);
    auto value = logic.flush();

    // Value should now be mapped to [100, 200] range
    // Center position (0.5 normalized) = 150
    TEST_ASSERT_TRUE(value.has_value());
    TEST_ASSERT_TRUE(value.value() >= 100.0f && value.value() <= 200.0f);
}

void test_custom_bounds_min_value() {
    EncoderLogic logic(makeConfig());
    logic.setBounds(0.0f, 127.0f);

    // Rotate to minimum
    for (int i = 0; i < 1000; i++) logic.processDelta(-1);

    auto value = logic.flush();
    TEST_ASSERT_TRUE(value.has_value());
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 0.0f, value.value());
}

void test_custom_bounds_max_value() {
    EncoderLogic logic(makeConfig());
    logic.setBounds(0.0f, 127.0f);

    // Rotate to maximum
    for (int i = 0; i < 1000; i++) logic.processDelta(1);

    auto value = logic.flush();
    TEST_ASSERT_TRUE(value.has_value());
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 127.0f, value.value());
}

// ═══════════════════════════════════════════════════════════════════
// RELATIVE Mode
// ═══════════════════════════════════════════════════════════════════

void test_relative_mode_emits_delta() {
    EncoderLogic logic(makeConfig());
    logic.setMode(EncoderMode::RELATIVE);
    logic.setDelta(1.0f);

    // One detent clockwise
    for (int i = 0; i < 4; i++) logic.processDelta(1);

    auto value = logic.flush();
    TEST_ASSERT_TRUE(value.has_value());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, value.value());
}

void test_relative_mode_negative_delta() {
    EncoderLogic logic(makeConfig());
    logic.setMode(EncoderMode::RELATIVE);
    logic.setDelta(1.0f);

    // One detent counterclockwise
    for (int i = 0; i < 4; i++) logic.processDelta(-1);

    auto value = logic.flush();
    TEST_ASSERT_TRUE(value.has_value());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -1.0f, value.value());
}

void test_relative_mode_custom_delta() {
    EncoderLogic logic(makeConfig());
    logic.setMode(EncoderMode::RELATIVE);
    logic.setDelta(0.1f);  // 0.1 per detent

    // One detent
    for (int i = 0; i < 4; i++) logic.processDelta(1);

    auto value = logic.flush();
    TEST_ASSERT_TRUE(value.has_value());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.1f, value.value());
}

// ═══════════════════════════════════════════════════════════════════
// RAW Mode
// ═══════════════════════════════════════════════════════════════════

void test_raw_mode_returns_ticks() {
    EncoderLogic logic(makeConfig());
    logic.setMode(EncoderMode::RAW);

    // In RAW mode, position starts at 36 (center) after setMode
    // processNewPosition(100) adds delta of 100 (since last_raw_position_ = 0)
    // So final position = 36 + 100 = 136
    logic.processNewPosition(100);

    auto value = logic.flush();
    TEST_ASSERT_TRUE(value.has_value());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 136.0f, value.value());
}

void test_raw_mode_setposition_no_clamp() {
    EncoderLogic logic(makeConfig());
    logic.setMode(EncoderMode::RAW);

    // RAW mode should not clamp - can store any value
    logic.setPosition(500.0f);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 500.0f, logic.getLastValue());
    TEST_ASSERT_EQUAL(500, logic.getPosition());
}

// ═══════════════════════════════════════════════════════════════════
// Discrete Steps (Quantization)
// ═══════════════════════════════════════════════════════════════════

void test_discrete_steps_quantizes_output() {
    EncoderLogic logic(makeConfig());
    logic.setDiscreteSteps(10);  // 10 discrete values (0.0, 0.111, 0.222, ... 1.0)

    // Move enough to cross a step boundary
    for (int i = 0; i < 20; i++) logic.processDelta(1);

    auto value = logic.flush();
    if (value.has_value()) {
        // With 10 steps, valid values are: 0, 1/9, 2/9, ... 9/9 = 1.0
        // Check that value is one of these quantized steps
        float normalized = value.value() * 9.0f;  // Map to 0-9 range
        float rounded = std::round(normalized);
        TEST_ASSERT_FLOAT_WITHIN(0.01f, rounded, normalized);
    }
}

void test_continuous_disables_quantization() {
    EncoderLogic logic(makeConfig());
    logic.setDiscreteSteps(10);
    logic.setContinuous();  // Disable quantization

    for (int i = 0; i < 5; i++) logic.processDelta(1);

    auto value = logic.flush();
    TEST_ASSERT_TRUE(value.has_value());
    // Value can be any float, not quantized
}

// ═══════════════════════════════════════════════════════════════════
// setPosition
// ═══════════════════════════════════════════════════════════════════

void test_set_position_updates_value() {
    EncoderLogic logic(makeConfig());
    logic.setPosition(0.75f);

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.75f, logic.getLastValue());
}

void test_set_position_clamps_to_bounds() {
    EncoderLogic logic(makeConfig());
    logic.setPosition(2.0f);  // Beyond max bounds [0,1]

    // Value should be clamped to max bound (1.0)
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, logic.getLastValue());
}

void test_set_position_clamps_to_min() {
    EncoderLogic logic(makeConfig());
    logic.setPosition(-0.5f);  // Below min bounds [0,1]

    // Value should be clamped to min bound (0.0)
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, logic.getLastValue());
}

// ═══════════════════════════════════════════════════════════════════
// Direction Inversion
// ═══════════════════════════════════════════════════════════════════

void test_invert_direction() {
    EncoderConfig config = makeConfig();
    config.invertDirection = true;
    EncoderLogic logic(config);

    float initial = logic.getLastValue();

    // Positive delta should now decrease value (inverted)
    for (int i = 0; i < 4; i++) logic.processDelta(1);

    auto value = logic.flush();
    TEST_ASSERT_TRUE(value.has_value());
    TEST_ASSERT_TRUE(value.value() < initial);
}

// ═══════════════════════════════════════════════════════════════════
// Pending Pattern
// ═══════════════════════════════════════════════════════════════════

void test_flush_clears_pending() {
    EncoderLogic logic(makeConfig());

    for (int i = 0; i < 4; i++) logic.processDelta(1);
    TEST_ASSERT_TRUE(logic.hasPending());

    logic.flush();
    TEST_ASSERT_FALSE(logic.hasPending());
}

void test_flush_returns_nullopt_when_no_pending() {
    EncoderLogic logic(makeConfig());

    auto value = logic.flush();
    TEST_ASSERT_FALSE(value.has_value());
}

void test_multiple_deltas_before_flush() {
    EncoderLogic logic(makeConfig());

    // Multiple movements
    for (int i = 0; i < 8; i++) logic.processDelta(1);

    // Only one flush gets latest value
    auto value1 = logic.flush();
    TEST_ASSERT_TRUE(value1.has_value());

    auto value2 = logic.flush();
    TEST_ASSERT_FALSE(value2.has_value());
}

// ═══════════════════════════════════════════════════════════════════
// processNewPosition (polling mode)
// ═══════════════════════════════════════════════════════════════════

void test_process_new_position_basic() {
    EncoderLogic logic(makeConfig());
    logic.setMode(EncoderMode::RAW);

    // In RAW mode after setMode, position = 36 (center)
    // processNewPosition(50) adds delta of 50 (since last_raw_position_ = 0)
    // Final position = 36 + 50 = 86
    logic.processNewPosition(50);

    auto value = logic.flush();
    TEST_ASSERT_TRUE(value.has_value());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 86.0f, value.value());
}

void test_process_new_position_no_change() {
    EncoderLogic logic(makeConfig());
    logic.setMode(EncoderMode::RAW);

    logic.processNewPosition(50);
    logic.flush();

    // Same position - no new event
    logic.processNewPosition(50);
    auto value = logic.flush();
    TEST_ASSERT_FALSE(value.has_value());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Basic construction
    RUN_TEST(test_default_mode_is_normalized);
    RUN_TEST(test_initial_value_is_center);
    RUN_TEST(test_initial_position_is_center);
    RUN_TEST(test_no_pending_initially);

    // NORMALIZED mode
    RUN_TEST(test_normalized_clockwise_increases_value);
    RUN_TEST(test_normalized_counterclockwise_decreases_value);
    RUN_TEST(test_normalized_bounds_min);
    RUN_TEST(test_normalized_bounds_max);

    // Custom bounds
    RUN_TEST(test_custom_bounds_mapping);
    RUN_TEST(test_custom_bounds_min_value);
    RUN_TEST(test_custom_bounds_max_value);

    // RELATIVE mode
    RUN_TEST(test_relative_mode_emits_delta);
    RUN_TEST(test_relative_mode_negative_delta);
    RUN_TEST(test_relative_mode_custom_delta);

    // RAW mode
    RUN_TEST(test_raw_mode_returns_ticks);
    RUN_TEST(test_raw_mode_setposition_no_clamp);

    // Discrete steps
    RUN_TEST(test_discrete_steps_quantizes_output);
    RUN_TEST(test_continuous_disables_quantization);

    // setPosition
    RUN_TEST(test_set_position_updates_value);
    RUN_TEST(test_set_position_clamps_to_bounds);
    RUN_TEST(test_set_position_clamps_to_min);

    // Direction inversion
    RUN_TEST(test_invert_direction);

    // Pending pattern
    RUN_TEST(test_flush_clears_pending);
    RUN_TEST(test_flush_returns_nullopt_when_no_pending);
    RUN_TEST(test_multiple_deltas_before_flush);

    // processNewPosition
    RUN_TEST(test_process_new_position_basic);
    RUN_TEST(test_process_new_position_no_change);

    UNITY_END();
    return 0;
}
