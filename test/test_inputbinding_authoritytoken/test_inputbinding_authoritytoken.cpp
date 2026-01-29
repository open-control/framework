#include <unity.h>

#include <oc/core/event/EventBus.hpp>
#include <oc/core/event/Events.hpp>
#include <oc/core/input/AuthorityResolver.hpp>
#include <oc/core/input/Binding.hpp>
#include <oc/core/input/InputBinding.hpp>

using namespace oc::core::event;
using namespace oc::core::input;

static int pressCount = 0;

static uint32_t timeNow() {
    return 0u;
}

void setUp() {
    pressCount = 0;
}

void tearDown() {}

void test_authority_token_prevents_stale_clear() {
    EventBus bus;
    InputBinding binding(bus, timeNow);

    // One scoped binding
    ButtonBinding b;
    b.type = ButtonBindingType::PRESS;
    b.buttonId = 1;
    b.scopeId = static_cast<oc::type::ScopeID>(123);
    b.action = []() { pressCount++; };
    binding.registerButtonBinding(b);

    // Resolver 1: allows scope 123
    AuthorityResolver r1;
    r1.setOverlayProvider([]() { return static_cast<oc::type::ScopeID>(123); });

    // Resolver 2: blocks scope 123
    AuthorityResolver r2;
    r2.setOverlayProvider([]() { return static_cast<oc::type::ScopeID>(999); });

    auto t1 = binding.setAuthorityResolverScoped(&r1);
    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(1, pressCount);

    auto t2 = binding.setAuthorityResolverScoped(&r2);
    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(1, pressCount);

    // Old token must not clear the current resolver
    binding.clearAuthorityResolver(t1);
    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(1, pressCount);

    // Current token clears; no resolver => all scopes allowed
    binding.clearAuthorityResolver(t2);
    bus.emit(ButtonPressEvent{1, true});
    TEST_ASSERT_EQUAL(2, pressCount);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_authority_token_prevents_stale_clear);
    UNITY_END();
    return 0;
}
