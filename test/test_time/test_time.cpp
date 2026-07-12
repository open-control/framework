#include <unity.h>

#include <cstdint>

#include <oc/time/Time.hpp>

namespace {

uint32_t mockMicros = 0;

uint32_t getMockMicros() {
    return mockMicros;
}

}  // namespace

void setUp() {
    mockMicros = 0;
    oc::time::setMicrosProvider(getMockMicros);
}

void tearDown() {}

void test_micros_provider_returns_configured_value() {
    mockMicros = 123456U;

    TEST_ASSERT_TRUE(oc::time::isMicrosConfigured());
    TEST_ASSERT_EQUAL_UINT32(123456U, oc::time::micros32());
}

void test_signed_delta_reports_late_deadline() {
    TEST_ASSERT_EQUAL_INT32(25, oc::time::signedDeltaUs(125U, 100U));
}

void test_signed_delta_reports_future_deadline() {
    TEST_ASSERT_EQUAL_INT32(-25, oc::time::signedDeltaUs(100U, 125U));
}

void test_signed_delta_reports_exact_deadline() {
    TEST_ASSERT_EQUAL_INT32(0, oc::time::signedDeltaUs(100U, 100U));
}

void test_signed_delta_handles_wrap_boundary() {
    const uint32_t deadline = 0xFFFF'FFF0U;
    const uint32_t now = 0x0000'0010U;

    TEST_ASSERT_EQUAL_INT32(32, oc::time::signedDeltaUs(now, deadline));
    TEST_ASSERT_EQUAL_INT32(-32, oc::time::signedDeltaUs(deadline, now));
}

void test_millisecond_deadline_handles_wrap_boundary() {
    const uint32_t deadline = 0x0000'0010U;

    TEST_ASSERT_FALSE(oc::time::deadlineReachedMs(0xFFFF'FFF0U, deadline));
    TEST_ASSERT_TRUE(oc::time::deadlineReachedMs(deadline, deadline));
    TEST_ASSERT_TRUE(oc::time::deadlineReachedMs(0x0000'0020U, deadline));
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_micros_provider_returns_configured_value);
    RUN_TEST(test_signed_delta_reports_late_deadline);
    RUN_TEST(test_signed_delta_reports_future_deadline);
    RUN_TEST(test_signed_delta_reports_exact_deadline);
    RUN_TEST(test_signed_delta_handles_wrap_boundary);
    RUN_TEST(test_millisecond_deadline_handles_wrap_boundary);
    return UNITY_END();
}
