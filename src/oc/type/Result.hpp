#pragma once

/**
 * @file Result.hpp
 * @brief Error handling types (Level 0 - no internal dependencies)
 */

#include <cassert>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

namespace oc::type {

/**
 * @brief Error codes for Result failures
 */
enum class ErrorCode : uint8_t {
    OK = 0,

    // Hardware errors
    HARDWARE_NOT_FOUND,    ///< Device not detected
    HARDWARE_INIT_FAILED,  ///< Initialization failed
    HARDWARE_TIMEOUT,      ///< Communication timeout
    HARDWARE_BUSY,         ///< Device busy

    // Resource errors
    RESOURCE_EXHAUSTED,  ///< No more capacity (memory, slots, etc.)
    RESOURCE_NOT_FOUND,  ///< Requested item doesn't exist

    // Validation errors
    INVALID_ARGUMENT,  ///< Parameter out of range or invalid
    INVALID_STATE,     ///< Operation not permitted in current state

    // Storage errors
    STORAGE_READ_FAILED,   ///< Could not read from storage
    STORAGE_WRITE_FAILED,  ///< Could not write to storage
    STORAGE_CORRUPT,       ///< Data integrity check failed

    // Context errors
    CONTEXT_NOT_REGISTERED,  ///< Context ID not found
    CONTEXT_INIT_FAILED,     ///< Context initialization failed
};

/**
 * @brief Convert ErrorCode to string (for logging)
 */
constexpr const char* errorCodeToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::OK: return "OK";
        case ErrorCode::HARDWARE_NOT_FOUND: return "HARDWARE_NOT_FOUND";
        case ErrorCode::HARDWARE_INIT_FAILED: return "HARDWARE_INIT_FAILED";
        case ErrorCode::HARDWARE_TIMEOUT: return "HARDWARE_TIMEOUT";
        case ErrorCode::HARDWARE_BUSY: return "HARDWARE_BUSY";
        case ErrorCode::RESOURCE_EXHAUSTED: return "RESOURCE_EXHAUSTED";
        case ErrorCode::RESOURCE_NOT_FOUND: return "RESOURCE_NOT_FOUND";
        case ErrorCode::INVALID_ARGUMENT: return "INVALID_ARGUMENT";
        case ErrorCode::INVALID_STATE: return "INVALID_STATE";
        case ErrorCode::STORAGE_READ_FAILED: return "STORAGE_READ_FAILED";
        case ErrorCode::STORAGE_WRITE_FAILED: return "STORAGE_WRITE_FAILED";
        case ErrorCode::STORAGE_CORRUPT: return "STORAGE_CORRUPT";
        case ErrorCode::CONTEXT_NOT_REGISTERED: return "CONTEXT_NOT_REGISTERED";
        case ErrorCode::CONTEXT_INIT_FAILED: return "CONTEXT_INIT_FAILED";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Error with code and optional context string
 */
struct Error {
    ErrorCode code;
    const char* context;  ///< Static string, no allocation

    constexpr Error(ErrorCode c, const char* ctx = nullptr) : code(c), context(ctx) {}
};

/**
 * @brief Result type for operations that can fail
 *
 * @tparam T Success value type (use void for no value)
 *
 * Usage:
 * @code
 * Result<void> init() {
 *     if (!hw_.init()) {
 *         return Result<void>::err({ErrorCode::HARDWARE_INIT_FAILED, "display"});
 *     }
 *     return Result<void>::ok();
 * }
 *
 * auto result = device.init();
 * if (!result) {
 *     OC_LOG_ERROR("Init failed: {} ({})",
 *                  errorCodeToString(result.error().code),
 *                  result.error().context);
 * }
 * @endcode
 */
template <typename T>
class Result {
public:
    /// Create success result with value
    static Result ok(T value) { return Result(std::move(value), true); }

    /// Create failure result with error
    static Result err(Error e) { return Result(e); }

    // Queries
    bool isOk() const { return has_value_; }
    bool isErr() const { return !has_value_; }
    explicit operator bool() const { return has_value_; }

    // Value access (asserts in debug if wrong state)
    T& value() & {
        assert(has_value_ && "Result::value() called on error");
        return value_;
    }
    const T& value() const& {
        assert(has_value_ && "Result::value() called on error");
        return value_;
    }
    T&& value() && {
        assert(has_value_ && "Result::value() called on error");
        return std::move(value_);
    }

    // Error access
    Error error() const {
        assert(!has_value_ && "Result::error() called on success");
        return error_;
    }

    /// Get value or fallback
    T valueOr(T fallback) const& { return has_value_ ? value_ : std::move(fallback); }
    T valueOr(T fallback) && { return has_value_ ? std::move(value_) : std::move(fallback); }

    /// Transform value if ok
    template <typename F>
    auto map(F&& f) -> Result<decltype(f(std::declval<T>()))> {
        using U = decltype(f(std::declval<T>()));
        if (has_value_) {
            return Result<U>::ok(f(value_));
        }
        return Result<U>::err(error_);
    }

    /// Handle error
    template <typename F>
    Result<T> orElse(F&& f) {
        if (has_value_) return std::move(*this);
        return f(error_);
    }

    ~Result() {
        if (has_value_) {
            value_.~T();
        }
    }

    Result(Result&& other) noexcept : has_value_(other.has_value_) {
        if (has_value_) {
            new (&value_) T(std::move(other.value_));
        } else {
            error_ = other.error_;
        }
    }

    Result& operator=(Result&& other) noexcept {
        if (this != &other) {
            if (has_value_) value_.~T();
            has_value_ = other.has_value_;
            if (has_value_) {
                new (&value_) T(std::move(other.value_));
            } else {
                error_ = other.error_;
            }
        }
        return *this;
    }

    Result(const Result&) = delete;
    Result& operator=(const Result&) = delete;

private:
    union {
        T value_;
        Error error_;
    };
    bool has_value_;

    Result(T v, bool) : value_(std::move(v)), has_value_(true) {}
    Result(Error e) : error_(e), has_value_(false) {}
};

/**
 * @brief Specialization for void (no value on success)
 */
template <>
class Result<void> {
public:
    static Result ok() { return Result(true); }
    static Result err(Error e) { return Result(e); }

    bool isOk() const { return ok_; }
    bool isErr() const { return !ok_; }
    explicit operator bool() const { return ok_; }

    Error error() const {
        assert(!ok_ && "Result::error() called on success");
        return error_;
    }

private:
    Error error_{ErrorCode::OK};
    bool ok_;

    explicit Result(bool ok) : ok_(ok) {}
    explicit Result(Error e) : error_(e), ok_(false) {}
};

}  // namespace oc::type
