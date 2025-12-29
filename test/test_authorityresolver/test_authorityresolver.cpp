#include <unity.h>

#include <oc/core/input/AuthorityResolver.hpp>

using namespace oc::core::input;
using namespace oc::core;

static ScopeID overlayScope = 0;
static ScopeID viewScope = 0;
static ScopeID customScope = 0;

void setUp() {
    overlayScope = 0;
    viewScope = 0;
    customScope = 0;
}

void tearDown() {}

// ═══════════════════════════════════════════════════════════════════
// Basic Authority Resolution
// ═══════════════════════════════════════════════════════════════════

void test_no_providers_returns_global() {
    AuthorityResolver resolver;

    TEST_ASSERT_EQUAL(0, resolver.getAuthority());
}

void test_overlay_has_highest_priority() {
    AuthorityResolver resolver;

    overlayScope = 100;
    viewScope = 200;

    resolver.setOverlayProvider([]() { return overlayScope; });
    resolver.setActiveViewProvider([]() { return viewScope; });

    TEST_ASSERT_EQUAL(100, resolver.getAuthority());
}

void test_view_used_when_no_overlay() {
    AuthorityResolver resolver;

    overlayScope = 0;  // No overlay
    viewScope = 200;

    resolver.setOverlayProvider([]() { return overlayScope; });
    resolver.setActiveViewProvider([]() { return viewScope; });

    TEST_ASSERT_EQUAL(200, resolver.getAuthority());
}

void test_global_when_no_overlay_or_view() {
    AuthorityResolver resolver;

    overlayScope = 0;
    viewScope = 0;

    resolver.setOverlayProvider([]() { return overlayScope; });
    resolver.setActiveViewProvider([]() { return viewScope; });

    TEST_ASSERT_EQUAL(0, resolver.getAuthority());
}

// ═══════════════════════════════════════════════════════════════════
// hasAuthority
// ═══════════════════════════════════════════════════════════════════

void test_hasAuthority_global_always_true() {
    AuthorityResolver resolver;

    overlayScope = 100;
    resolver.setOverlayProvider([]() { return overlayScope; });

    // Global (scope=0) always has authority (participates in dispatch)
    TEST_ASSERT_TRUE(resolver.hasAuthority(0));
}

void test_hasAuthority_matching_scope() {
    AuthorityResolver resolver;

    overlayScope = 100;
    resolver.setOverlayProvider([]() { return overlayScope; });

    TEST_ASSERT_TRUE(resolver.hasAuthority(100));
}

void test_hasAuthority_non_matching_scope() {
    AuthorityResolver resolver;

    overlayScope = 100;
    resolver.setOverlayProvider([]() { return overlayScope; });

    TEST_ASSERT_FALSE(resolver.hasAuthority(200));  // Wrong scope
}

// ═══════════════════════════════════════════════════════════════════
// isExclusiveAuthority
// ═══════════════════════════════════════════════════════════════════

void test_isExclusiveAuthority_not_for_global() {
    AuthorityResolver resolver;

    // Even with no other authority, global (0) is not "exclusive"
    TEST_ASSERT_FALSE(resolver.isExclusiveAuthority(0));
}

void test_isExclusiveAuthority_for_overlay() {
    AuthorityResolver resolver;

    overlayScope = 100;
    resolver.setOverlayProvider([]() { return overlayScope; });

    TEST_ASSERT_TRUE(resolver.isExclusiveAuthority(100));
    TEST_ASSERT_FALSE(resolver.isExclusiveAuthority(200));
}

// ═══════════════════════════════════════════════════════════════════
// isGlobalBlocked
// ═══════════════════════════════════════════════════════════════════

void test_isGlobalBlocked_when_overlay_active() {
    AuthorityResolver resolver;

    overlayScope = 100;
    resolver.setOverlayProvider([]() { return overlayScope; });

    TEST_ASSERT_TRUE(resolver.isGlobalBlocked());
}

void test_isGlobalBlocked_when_view_active() {
    AuthorityResolver resolver;

    viewScope = 200;
    resolver.setActiveViewProvider([]() { return viewScope; });

    TEST_ASSERT_TRUE(resolver.isGlobalBlocked());
}

void test_isGlobalBlocked_false_when_none_active() {
    AuthorityResolver resolver;

    overlayScope = 0;
    viewScope = 0;

    resolver.setOverlayProvider([]() { return overlayScope; });
    resolver.setActiveViewProvider([]() { return viewScope; });

    TEST_ASSERT_FALSE(resolver.isGlobalBlocked());
}

// ═══════════════════════════════════════════════════════════════════
// Custom Layers
// ═══════════════════════════════════════════════════════════════════

void test_custom_layer_between_overlay_and_view() {
    AuthorityResolver resolver;

    overlayScope = 0;    // No overlay
    customScope = 150;   // Custom layer active
    viewScope = 200;     // View also set

    resolver.setOverlayProvider([]() { return overlayScope; });
    resolver.addCustomLayer([]() { return customScope; });
    resolver.setActiveViewProvider([]() { return viewScope; });

    // Custom layer has priority over view
    TEST_ASSERT_EQUAL(150, resolver.getAuthority());
}

void test_overlay_overrides_custom_layer() {
    AuthorityResolver resolver;

    overlayScope = 100;  // Overlay active
    customScope = 150;   // Custom layer also active

    resolver.setOverlayProvider([]() { return overlayScope; });
    resolver.addCustomLayer([]() { return customScope; });

    // Overlay wins
    TEST_ASSERT_EQUAL(100, resolver.getAuthority());
}

void test_multiple_custom_layers_last_wins() {
    AuthorityResolver resolver;

    static ScopeID layer1 = 110;
    static ScopeID layer2 = 120;

    resolver.addCustomLayer([]() { return layer1; });
    resolver.addCustomLayer([]() { return layer2; });

    // Last added has highest priority
    TEST_ASSERT_EQUAL(120, resolver.getAuthority());
}

void test_inactive_custom_layer_skipped() {
    AuthorityResolver resolver;

    static ScopeID layer1 = 110;
    static ScopeID layer2 = 0;  // Inactive

    resolver.addCustomLayer([]() { return layer1; });
    resolver.addCustomLayer([]() { return layer2; });

    // layer2 inactive, falls back to layer1
    TEST_ASSERT_EQUAL(110, resolver.getAuthority());
}

void test_clearCustomLayers() {
    AuthorityResolver resolver;

    customScope = 150;
    viewScope = 200;

    resolver.addCustomLayer([]() { return customScope; });
    resolver.setActiveViewProvider([]() { return viewScope; });

    // Custom layer wins
    TEST_ASSERT_EQUAL(150, resolver.getAuthority());

    // Clear custom layers
    resolver.clearCustomLayers();

    // Now view wins
    TEST_ASSERT_EQUAL(200, resolver.getAuthority());
}

// ═══════════════════════════════════════════════════════════════════
// Dynamic Authority Changes
// ═══════════════════════════════════════════════════════════════════

void test_authority_updates_dynamically() {
    AuthorityResolver resolver;

    overlayScope = 0;
    viewScope = 200;

    resolver.setOverlayProvider([]() { return overlayScope; });
    resolver.setActiveViewProvider([]() { return viewScope; });

    // Initially view has authority
    TEST_ASSERT_EQUAL(200, resolver.getAuthority());

    // Overlay opens
    overlayScope = 100;
    TEST_ASSERT_EQUAL(100, resolver.getAuthority());

    // Overlay closes
    overlayScope = 0;
    TEST_ASSERT_EQUAL(200, resolver.getAuthority());

    // View changes
    viewScope = 300;
    TEST_ASSERT_EQUAL(300, resolver.getAuthority());
}

// ═══════════════════════════════════════════════════════════════════
// Edge Cases
// ═══════════════════════════════════════════════════════════════════

void test_null_providers_handled() {
    AuthorityResolver resolver;

    // No providers set
    TEST_ASSERT_EQUAL(0, resolver.getAuthority());
    TEST_ASSERT_TRUE(resolver.hasAuthority(0));
    TEST_ASSERT_FALSE(resolver.isGlobalBlocked());
}

void test_same_scope_for_overlay_and_view() {
    AuthorityResolver resolver;

    overlayScope = 100;
    viewScope = 100;  // Same as overlay

    resolver.setOverlayProvider([]() { return overlayScope; });
    resolver.setActiveViewProvider([]() { return viewScope; });

    // Overlay wins (even if same value)
    TEST_ASSERT_EQUAL(100, resolver.getAuthority());
    TEST_ASSERT_TRUE(resolver.hasAuthority(100));
}

// ═══════════════════════════════════════════════════════════════════
// Unity Test Runner
// ═══════════════════════════════════════════════════════════════════

int main() {
    UNITY_BEGIN();

    // Basic Authority Resolution
    RUN_TEST(test_no_providers_returns_global);
    RUN_TEST(test_overlay_has_highest_priority);
    RUN_TEST(test_view_used_when_no_overlay);
    RUN_TEST(test_global_when_no_overlay_or_view);

    // hasAuthority
    RUN_TEST(test_hasAuthority_global_always_true);
    RUN_TEST(test_hasAuthority_matching_scope);
    RUN_TEST(test_hasAuthority_non_matching_scope);

    // isExclusiveAuthority
    RUN_TEST(test_isExclusiveAuthority_not_for_global);
    RUN_TEST(test_isExclusiveAuthority_for_overlay);

    // isGlobalBlocked
    RUN_TEST(test_isGlobalBlocked_when_overlay_active);
    RUN_TEST(test_isGlobalBlocked_when_view_active);
    RUN_TEST(test_isGlobalBlocked_false_when_none_active);

    // Custom Layers
    RUN_TEST(test_custom_layer_between_overlay_and_view);
    RUN_TEST(test_overlay_overrides_custom_layer);
    RUN_TEST(test_multiple_custom_layers_last_wins);
    RUN_TEST(test_inactive_custom_layer_skipped);
    RUN_TEST(test_clearCustomLayers);

    // Dynamic Authority Changes
    RUN_TEST(test_authority_updates_dynamically);

    // Edge Cases
    RUN_TEST(test_null_providers_handled);
    RUN_TEST(test_same_scope_for_overlay_and_view);

    return UNITY_END();
}
