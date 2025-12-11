#include <unity.h>

#include <oc/core/Result.hpp>

using namespace oc::core;

void test_result_void_ok() {
    auto result = Result<void>::ok();
    TEST_ASSERT_TRUE(result.isOk());
    TEST_ASSERT_FALSE(result.isErr());
    TEST_ASSERT_TRUE(static_cast<bool>(result));
}

void test_result_void_err() {
    auto result = Result<void>::err({ErrorCode::HARDWARE_INIT_FAILED, "test"});
    TEST_ASSERT_FALSE(result.isOk());
    TEST_ASSERT_TRUE(result.isErr());
    TEST_ASSERT_FALSE(static_cast<bool>(result));
    TEST_ASSERT_EQUAL(ErrorCode::HARDWARE_INIT_FAILED, result.error().code);
    TEST_ASSERT_EQUAL_STRING("test", result.error().context);
}

void test_result_void_err_without_context() {
    auto result = Result<void>::err({ErrorCode::INVALID_STATE});
    TEST_ASSERT_TRUE(result.isErr());
    TEST_ASSERT_EQUAL(ErrorCode::INVALID_STATE, result.error().code);
    TEST_ASSERT_NULL(result.error().context);
}

void test_result_value_ok() {
    auto result = Result<int>::ok(42);
    TEST_ASSERT_TRUE(result.isOk());
    TEST_ASSERT_EQUAL(42, result.value());
}

void test_result_value_err() {
    auto result = Result<int>::err({ErrorCode::INVALID_ARGUMENT, "param"});
    TEST_ASSERT_TRUE(result.isErr());
    TEST_ASSERT_EQUAL(ErrorCode::INVALID_ARGUMENT, result.error().code);
    TEST_ASSERT_EQUAL_STRING("param", result.error().context);
}

void test_result_value_or() {
    auto ok = Result<int>::ok(42);
    auto err = Result<int>::err({ErrorCode::RESOURCE_NOT_FOUND});

    TEST_ASSERT_EQUAL(42, ok.valueOr(0));
    TEST_ASSERT_EQUAL(0, err.valueOr(0));
}

void test_result_map() {
    auto result = Result<int>::ok(21);
    auto mapped = result.map([](int x) { return x * 2; });
    TEST_ASSERT_TRUE(mapped.isOk());
    TEST_ASSERT_EQUAL(42, mapped.value());
}

void test_result_map_err_propagates() {
    auto result = Result<int>::err({ErrorCode::INVALID_STATE});
    auto mapped = result.map([](int x) { return x * 2; });
    TEST_ASSERT_TRUE(mapped.isErr());
    TEST_ASSERT_EQUAL(ErrorCode::INVALID_STATE, mapped.error().code);
}

void test_result_or_else() {
    auto err = Result<int>::err({ErrorCode::RESOURCE_NOT_FOUND});
    auto recovered = err.orElse([](Error) { return Result<int>::ok(99); });
    TEST_ASSERT_TRUE(recovered.isOk());
    TEST_ASSERT_EQUAL(99, recovered.value());
}

void test_result_or_else_on_ok() {
    auto ok = Result<int>::ok(42);
    bool called = false;
    auto result = ok.orElse([&called](Error) {
        called = true;
        return Result<int>::ok(99);
    });
    TEST_ASSERT_FALSE(called);
    TEST_ASSERT_EQUAL(42, result.value());
}

void test_error_code_to_string() {
    TEST_ASSERT_EQUAL_STRING("HARDWARE_INIT_FAILED",
                             errorCodeToString(ErrorCode::HARDWARE_INIT_FAILED));
    TEST_ASSERT_EQUAL_STRING("OK", errorCodeToString(ErrorCode::OK));
    TEST_ASSERT_EQUAL_STRING("STORAGE_CORRUPT", errorCodeToString(ErrorCode::STORAGE_CORRUPT));
    TEST_ASSERT_EQUAL_STRING("CONTEXT_INIT_FAILED",
                             errorCodeToString(ErrorCode::CONTEXT_INIT_FAILED));
}

void test_error_code_unknown() {
    // Cast invalid value to test default case
    auto unknown = static_cast<ErrorCode>(255);
    TEST_ASSERT_EQUAL_STRING("UNKNOWN", errorCodeToString(unknown));
}

void test_result_move_semantics() {
    auto result1 = Result<int>::ok(42);
    auto result2 = std::move(result1);
    TEST_ASSERT_TRUE(result2.isOk());
    TEST_ASSERT_EQUAL(42, result2.value());
}

void test_result_move_assignment() {
    auto result1 = Result<int>::ok(42);
    auto result2 = Result<int>::err({ErrorCode::INVALID_STATE});
    result2 = std::move(result1);
    TEST_ASSERT_TRUE(result2.isOk());
    TEST_ASSERT_EQUAL(42, result2.value());
}

void setUp() {}
void tearDown() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_result_void_ok);
    RUN_TEST(test_result_void_err);
    RUN_TEST(test_result_void_err_without_context);
    RUN_TEST(test_result_value_ok);
    RUN_TEST(test_result_value_err);
    RUN_TEST(test_result_value_or);
    RUN_TEST(test_result_map);
    RUN_TEST(test_result_map_err_propagates);
    RUN_TEST(test_result_or_else);
    RUN_TEST(test_result_or_else_on_ok);
    RUN_TEST(test_error_code_to_string);
    RUN_TEST(test_error_code_unknown);
    RUN_TEST(test_result_move_semantics);
    RUN_TEST(test_result_move_assignment);
    return UNITY_END();
}
