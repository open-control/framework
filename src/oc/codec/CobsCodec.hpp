#pragma once

/**
 * @file CobsCodec.hpp
 * @brief COBS (Consistent Overhead Byte Stuffing) framing
 *
 * Encodes data so 0x00 never appears in payload, allowing it as frame delimiter.
 * Compatible with the Rust implementation in oc-bridge.
 *
 * Properties:
 * - Worst case overhead: N + N/254 + 1 bytes
 * - Zero-copy streaming decoder for embedded use
 * - Frame delimiter: 0x00
 *
 * @see https://en.wikipedia.org/wiki/Consistent_Overhead_Byte_Stuffing
 */

#include <cstddef>
#include <cstdint>
#include <functional>

namespace oc::codec {

/// Maximum frame size (must match bridge)
static constexpr size_t COBS_MAX_FRAME_SIZE = 4096;

/// Frame delimiter byte
static constexpr uint8_t COBS_DELIMITER = 0x00;

/**
 * @brief Calculate maximum encoded size for a given input length
 * @param len Input data length
 * @return Maximum encoded size including delimiter
 */
constexpr size_t cobsMaxEncodedSize(size_t len) {
    return len + (len / 254) + 2;  // +1 for overhead, +1 for delimiter
}

/**
 * @brief Encode data using COBS
 *
 * @param input Source data
 * @param inputLen Length of source data
 * @param output Destination buffer (must be at least cobsMaxEncodedSize(inputLen))
 * @return Number of bytes written (including trailing 0x00 delimiter)
 */
inline size_t cobsEncode(const uint8_t* input, size_t inputLen, uint8_t* output) {
    size_t writeIdx = 0;
    size_t codeIdx = 0;
    uint8_t code = 1;

    output[writeIdx++] = 0;  // Placeholder for first code

    for (size_t i = 0; i < inputLen; ++i) {
        if (input[i] == 0) {
            output[codeIdx] = code;
            codeIdx = writeIdx;
            output[writeIdx++] = 0;  // Placeholder
            code = 1;
        } else {
            output[writeIdx++] = input[i];
            code++;
            if (code == 255) {
                output[codeIdx] = code;
                codeIdx = writeIdx;
                output[writeIdx++] = 0;  // Placeholder
                code = 1;
            }
        }
    }

    output[codeIdx] = code;
    output[writeIdx++] = COBS_DELIMITER;  // Frame delimiter

    return writeIdx;
}

/**
 * @brief Decode COBS-encoded data (without trailing delimiter)
 *
 * @param input Encoded data (excluding trailing 0x00)
 * @param inputLen Length of encoded data
 * @param output Destination buffer (at least inputLen bytes)
 * @return Number of bytes written, or 0 on error
 */
inline size_t cobsDecode(const uint8_t* input, size_t inputLen, uint8_t* output) {
    if (inputLen == 0) return 0;

    size_t readIdx = 0;
    size_t writeIdx = 0;

    while (readIdx < inputLen) {
        uint8_t code = input[readIdx++];
        if (code == 0) {
            return 0;  // Invalid: zero in encoded data
        }

        size_t copyLen = code - 1;
        if (readIdx + copyLen > inputLen) {
            return 0;  // Invalid: not enough data
        }

        for (size_t i = 0; i < copyLen; ++i) {
            output[writeIdx++] = input[readIdx++];
        }

        if (code < 255 && readIdx < inputLen) {
            output[writeIdx++] = 0;
        }
    }

    return writeIdx;
}

/**
 * @brief Streaming COBS frame decoder
 *
 * Accumulates bytes and invokes callback for each complete frame.
 * Zero-allocation after construction (uses fixed internal buffers).
 *
 * @note Memory usage: 2 * MaxFrameSize bytes (default: 8KB total)
 *
 * @tparam MaxFrameSize Maximum frame size to buffer
 */
template<size_t MaxFrameSize = COBS_MAX_FRAME_SIZE>
class CobsDecoder {
public:
    using FrameCallback = std::function<void(const uint8_t* data, size_t len)>;

    CobsDecoder() = default;

    /**
     * @brief Feed bytes to decoder
     *
     * Call this with incoming serial data. When a complete frame is
     * received (0x00 delimiter), it's decoded and passed to callback.
     *
     * @param byte Single byte to process
     * @param onFrame Callback invoked for each complete decoded frame
     */
    template<typename Callback>
    void feed(uint8_t byte, Callback&& onFrame) {
        if (byte == COBS_DELIMITER) {
            if (bufferLen_ > 0) {
                // Decode into member buffer (no stack allocation)
                size_t decodedLen = cobsDecode(buffer_, bufferLen_, decode_buffer_);
                if (decodedLen > 0) {
                    onFrame(decode_buffer_, decodedLen);
                }
                bufferLen_ = 0;
            }
        } else if (bufferLen_ < MaxFrameSize) {
            buffer_[bufferLen_++] = byte;
        } else {
            // Frame too large, reset
            bufferLen_ = 0;
        }
    }

    /**
     * @brief Feed multiple bytes to decoder
     *
     * @param data Pointer to byte buffer
     * @param len Number of bytes
     * @param onFrame Callback invoked for each complete decoded frame
     */
    template<typename Callback>
    void feed(const uint8_t* data, size_t len, Callback&& onFrame) {
        for (size_t i = 0; i < len; ++i) {
            feed(data[i], onFrame);
        }
    }

    /**
     * @brief Reset decoder state
     */
    void reset() {
        bufferLen_ = 0;
    }

private:
    uint8_t buffer_[MaxFrameSize]{};
    uint8_t decode_buffer_[MaxFrameSize]{};  ///< Buffer for decoded output
    size_t bufferLen_ = 0;
};

}  // namespace oc::codec
