#pragma once

#include <oc/context/IContext.hpp>
#include <oc/context/Requirements.hpp>

namespace oc::test {

/**
 * @brief Mock context for testing ContextManager
 */
class MockContext : public context::IContext {
public:
    static constexpr context::Requirements REQUIRES{
        .button = false,
        .encoder = false,
        .midi = false
    };

    bool initialize() override {
        initialized_ = true;
        initialize_count_++;
        return should_init_succeed_;
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
class MockContextRequiresButton : public context::IContext {
public:
    static constexpr context::Requirements REQUIRES{
        .button = true,
        .encoder = false,
        .midi = false
    };

    bool initialize() override { return true; }
    void update() override {}
    void cleanup() override {}
    const char* getName() const override { return "MockContextRequiresButton"; }
};

/**
 * @brief Another mock context for testing switching
 */
class MockContextB : public context::IContext {
public:
    static constexpr context::Requirements REQUIRES{
        .button = false,
        .encoder = false,
        .midi = false
    };

    bool initialize() override {
        initialized_ = true;
        return true;
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
