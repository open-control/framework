#pragma once

/**
 * @file ProtocolOutput.hpp
 * @brief Log output that sends messages via protocol transport
 *
 * Buffers log output and sends complete lines via a callback.
 * Compatible with any protocol transport (SysEx, Binary, etc.).
 *
 * Usage:
 * @code
 * // Define your log message handler
 * void sendLogMessage(const char* message) {
 *     LogMessage msg{LogLevel::INFO, message};
 *     protocol.send(msg);
 * }
 *
 * // In setup()
 * oc::log::setOutput(oc::log::protocolOutput(sendLogMessage));
 * @endcode
 */

#include <oc/log/Log.hpp>
#include <Arduino.h>  // For millis()

namespace oc::log {

/// Maximum log message length
static constexpr size_t PROTOCOL_LOG_MAX_LENGTH = 128;

/**
 * @brief Callback type for sending log messages
 *
 * Called when a complete log line is ready to send.
 * The message is null-terminated and includes the full formatted line
 * (timestamp, level, message).
 */
using LogSendCallback = void (*)(const char* message);

namespace detail {

/**
 * @brief Internal state for protocol output buffering
 */
struct ProtocolOutputState {
    char buffer[PROTOCOL_LOG_MAX_LENGTH];
    size_t pos = 0;
    LogSendCallback sendCallback = nullptr;

    void append(char c) {
        if (c == '\n') {
            flush();
        } else if (pos < PROTOCOL_LOG_MAX_LENGTH - 1) {
            buffer[pos++] = c;
        }
    }

    void append(const char* str) {
        while (*str) {
            append(*str++);
        }
    }

    void flush() {
        if (pos > 0 && sendCallback) {
            buffer[pos] = '\0';
            sendCallback(buffer);
            pos = 0;
        }
    }
};

// Global state (single instance per app)
inline ProtocolOutputState& getProtocolState() {
    static ProtocolOutputState state;
    return state;
}

// Helper to convert number to string
inline void appendNumber(int32_t value) {
    char buf[12];
    int i = 0;
    bool negative = value < 0;
    if (negative) value = -value;

    do {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    } while (value > 0);

    if (negative) buf[i++] = '-';

    auto& state = getProtocolState();
    while (i > 0) {
        state.append(buf[--i]);
    }
}

inline void appendNumber(uint32_t value) {
    char buf[11];
    int i = 0;

    do {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    } while (value > 0);

    auto& state = getProtocolState();
    while (i > 0) {
        state.append(buf[--i]);
    }
}

inline void appendFloat(float value) {
    auto& state = getProtocolState();

    if (value < 0) {
        state.append('-');
        value = -value;
    }

    auto intPart = static_cast<uint32_t>(value);
    appendNumber(intPart);
    state.append('.');

    value -= intPart;
    for (int i = 0; i < 2; ++i) {  // 2 decimal places
        value *= 10;
        state.append('0' + static_cast<int>(value) % 10);
    }
}

}  // namespace detail

/**
 * @brief Create a protocol-based log output
 *
 * Returns an Output that buffers log lines and sends them via callback.
 * ANSI color codes are stripped (not useful for structured logs).
 *
 * @param sendCallback Function to call with complete log messages
 * @return Output configured for protocol transport
 */
inline const Output& protocolOutput(LogSendCallback sendCallback) {
    detail::getProtocolState().sendCallback = sendCallback;

    static const Output output = {
        // printChar
        [](char c) {
            // Skip ANSI escape sequences
            static bool inEscape = false;
            if (c == '\033') {
                inEscape = true;
                return;
            }
            if (inEscape) {
                if (c == 'm') inEscape = false;
                return;
            }
            detail::getProtocolState().append(c);
        },
        // printStr
        [](const char* str) {
            while (*str) {
                // Skip ANSI escape sequences
                if (*str == '\033') {
                    while (*str && *str != 'm') ++str;
                    if (*str) ++str;
                    continue;
                }
                detail::getProtocolState().append(*str++);
            }
        },
        // printInt32
        [](int32_t value) {
            detail::appendNumber(value);
        },
        // printUint32
        [](uint32_t value) {
            detail::appendNumber(value);
        },
        // printFloat
        [](float value) {
            detail::appendFloat(value);
        },
        // printBool
        [](bool value) {
            detail::getProtocolState().append(value ? "true" : "false");
        },
        // getTimeMs
        []() -> uint32_t { return millis(); }
    };

    return output;
}

}  // namespace oc::log
