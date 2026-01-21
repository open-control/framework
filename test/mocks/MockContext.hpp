#pragma once

#include <oc/interface/IContext.hpp>
#include <oc/context/Requirements.hpp>
#include <oc/type/Result.hpp>

namespace oc::test {

/**
 * @brief Mock context for testing ContextManager
 */
class MockContext : public interface::IContext {
public:
    static constexpr context::Requirements REQUIRES{
        .button = false,
        .encoder = false,
        .midi = false
    };

    oc::type::Result<void> init() override {
        initialized_ = true;
        initialize_count_++;
        if (should_init_succeed_) {
            return oc::type::Result<void>::ok();
        }
        return oc::type::Result<void>::err({oc::type::ErrorCode::CONTEXT_INIT_FAILED, "Mock init failed"});
    }

    void update() override {
        update_count_++;
    }

    void cleanup() override {
        cleaned_up_ = true;
        cleanup_count_++;
    }

    const char* getName() const override { return "MockContext"; }

    // Test helpers
    static void reset() {
        initialized_ = false;
        cleaned_up_ = false;
        initialize_count_ = 0;
        update_count_ = 0;
        cleanup_count_ = 0;
        should_init_succeed_ = true;
    }

    static void setInitShouldFail() { should_init_succeed_ = false; }

    static bool wasInitialized() { return initialized_; }
    static bool wasCleanedUp() { return cleaned_up_; }
    static int initializeCount() { return initialize_count_; }
    static int updateCount() { return update_count_; }
    static int cleanupCount() { return cleanup_count_; }

private:
    static inline bool initialized_ = false;
    static inline bool cleaned_up_ = false;
    static inline int initialize_count_ = 0;
    static inline int update_count_ = 0;
    static inline int cleanup_count_ = 0;
    static inline bool should_init_succeed_ = true;
};

/**
 * @brief Mock context that requires ButtonAPI
 */
class MockContextRequiresButton : public interface::IContext {
public:
    static constexpr context::Requirements REQUIRES{
        .button = true,
        .encoder = false,
        .midi = false
    };

    oc::type::Result<void> init() override { return oc::type::Result<void>::ok(); }
    void update() override {}
    void cleanup() override {}
    const char* getName() const override { return "MockContextRequiresButton"; }
};

/**
 * @brief Another mock context for testing switching
 */
class MockContextB : public interface::IContext {
public:
    static constexpr context::Requirements REQUIRES{
        .button = false,
        .encoder = false,
        .midi = false
    };

    oc::type::Result<void> init() override {
        initialized_ = true;
        return oc::type::Result<void>::ok();
    }

    void update() override {}
    void cleanup() override { cleaned_up_ = true; }
    const char* getName() const override { return "MockContextB"; }

    static void reset() {
        initialized_ = false;
        cleaned_up_ = false;
    }

    static bool wasInitialized() { return initialized_; }
    static bool wasCleanedUp() { return cleaned_up_; }

private:
    static inline bool initialized_ = false;
    static inline bool cleaned_up_ = false;
};

}  // namespace oc::test
