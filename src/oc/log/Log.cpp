/**
 * @file Log.cpp
 * @brief Log output implementation using dependency injection
 *
 * Stores the configured Output and dispatches print calls to it.
 * If no output is configured, calls are silently ignored (safe no-op).
 */

#include <oc/log/Log.hpp>

namespace oc::log {

// =============================================================================
// Output storage (configured via setOutput)
// =============================================================================

namespace {

// Default no-op implementations (used when not configured)
void noopChar(char) {}
void noopStr(const char*) {}
void noopInt32(int32_t) {}
void noopUint32(uint32_t) {}
void noopFloat(float) {}
void noopBool(bool) {}
uint32_t noopTime() { return 0; }

// Current output (initialized to no-op)
Output g_output = {
    noopChar,
    noopStr,
    noopInt32,
    noopUint32,
    noopFloat,
    noopBool,
    noopTime
};

bool g_configured = false;

}  // namespace

// =============================================================================
// Configuration API
// =============================================================================

void setOutput(const Output& output) {
    g_output = output;
    g_configured = true;
}

bool isConfigured() {
    return g_configured;
}

// =============================================================================
// Print functions (dispatch to configured output)
// =============================================================================

void print(char c) {
    g_output.printChar(c);
}

void print(const char* str) {
    g_output.printStr(str);
}

void print(int32_t value) {
    g_output.printInt32(value);
}

void print(uint32_t value) {
    g_output.printUint32(value);
}

void print(float value) {
    g_output.printFloat(value);
}

void print(bool value) {
    g_output.printBool(value);
}

uint32_t getTimeMs() {
    return g_output.getTimeMs();
}

}  // namespace oc::log
