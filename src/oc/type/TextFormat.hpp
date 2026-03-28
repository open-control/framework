#pragma once

#include <cstddef>
#include <cstdint>

namespace oc::type::text {

inline void clear(char* buffer, size_t size) {
    if (buffer && size) {
        buffer[0] = '\0';
    }
}

inline void terminate(char* buffer, size_t size, size_t pos) {
    if (!buffer || size == 0) return;
    buffer[(pos < size) ? pos : (size - 1)] = '\0';
}

inline size_t appendChar(char* buffer, size_t size, size_t pos, char ch) {
    if (!buffer || size == 0) return pos;
    if (pos + 1 < size) {
        buffer[pos] = ch;
    }
    return pos + 1;
}

inline size_t appendString(char* buffer, size_t size, size_t pos, const char* text) {
    if (!text) return pos;
    while (*text) {
        pos = appendChar(buffer, size, pos, *text++);
    }
    return pos;
}

inline size_t appendUnsigned(
    char* buffer,
    size_t size,
    size_t pos,
    unsigned value,
    unsigned minDigits = 1
) {
    char digits[10];
    unsigned count = 0;

    do {
        digits[count++] = static_cast<char>('0' + (value % 10U));
        value /= 10U;
    } while (value != 0U && count < sizeof(digits));

    while (count < minDigits && count < sizeof(digits)) {
        digits[count++] = '0';
    }

    while (count > 0) {
        pos = appendChar(buffer, size, pos, digits[--count]);
    }
    return pos;
}

inline size_t appendSigned(
    char* buffer,
    size_t size,
    size_t pos,
    int value,
    bool alwaysSign = false
) {
    unsigned magnitude = static_cast<unsigned>(value);
    if (value < 0) {
        pos = appendChar(buffer, size, pos, '-');
        magnitude = static_cast<unsigned>(-value);
    } else if (alwaysSign) {
        pos = appendChar(buffer, size, pos, '+');
    }
    return appendUnsigned(buffer, size, pos, magnitude);
}

inline void copy(char* buffer, size_t size, const char* text) {
    size_t pos = appendString(buffer, size, 0, text);
    terminate(buffer, size, pos);
}

inline void formatUnsigned(char* buffer, size_t size, unsigned value) {
    size_t pos = appendUnsigned(buffer, size, 0, value);
    terminate(buffer, size, pos);
}

inline void formatSigned(char* buffer, size_t size, int value, bool alwaysSign = false) {
    size_t pos = appendSigned(buffer, size, 0, value, alwaysSign);
    terminate(buffer, size, pos);
}

inline void formatUnsignedPercent(char* buffer, size_t size, unsigned value) {
    size_t pos = appendUnsigned(buffer, size, 0, value);
    pos = appendChar(buffer, size, pos, '%');
    terminate(buffer, size, pos);
}

inline void formatSignedPercent(char* buffer, size_t size, int value) {
    size_t pos = appendSigned(buffer, size, 0, value, true);
    pos = appendChar(buffer, size, pos, '%');
    terminate(buffer, size, pos);
}

inline void formatFraction(char* buffer, size_t size, unsigned num, unsigned den) {
    size_t pos = appendUnsigned(buffer, size, 0, num);
    pos = appendChar(buffer, size, pos, '/');
    pos = appendUnsigned(buffer, size, pos, den);
    terminate(buffer, size, pos);
}

inline void formatFixed1(char* buffer, size_t size, float value) {
    size_t pos = 0;
    if (value < 0.0f) {
        pos = appendChar(buffer, size, pos, '-');
        value = -value;
    }
    const unsigned scaled = static_cast<unsigned>(value * 10.0f + 0.5f);
    pos = appendUnsigned(buffer, size, pos, scaled / 10U);
    pos = appendChar(buffer, size, pos, '.');
    pos = appendUnsigned(buffer, size, pos, scaled % 10U);
    terminate(buffer, size, pos);
}

inline void formatFixed(char* buffer, size_t size, float value, uint8_t decimals) {
    if (decimals == 0) {
        const int rounded = (value >= 0.0f)
            ? static_cast<int>(value + 0.5f)
            : static_cast<int>(value - 0.5f);
        formatSigned(buffer, size, rounded, false);
        return;
    }

    uint32_t scale = 1;
    for (uint8_t i = 0; i < decimals; ++i) {
        scale *= 10U;
    }

    size_t pos = 0;
    if (value < 0.0f) {
        pos = appendChar(buffer, size, pos, '-');
        value = -value;
    }

    const uint32_t scaled = static_cast<uint32_t>(value * static_cast<float>(scale) + 0.5f);
    pos = appendUnsigned(buffer, size, pos, scaled / scale);
    pos = appendChar(buffer, size, pos, '.');
    pos = appendUnsigned(buffer, size, pos, scaled % scale, decimals);
    terminate(buffer, size, pos);
}

}  // namespace oc::type::text
