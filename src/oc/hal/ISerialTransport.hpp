#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

#include <oc/core/Result.hpp>

namespace oc::hal {

/**
 * @brief Interface for Serial I/O abstraction
 *
 * Provides a platform-agnostic interface for serial communication.
 * Used by Serial8Protocol for binary message transport.
 *
 * The implementation handles:
 * - COBS framing (encode on send, decode on receive)
 * - Buffering and packetization
 *
 * @note This is for raw byte transport. Protocol encoding (8-bit binary)
 *       is handled by the protocol layer above.
 */
class ISerialTransport {
public:
    virtual ~ISerialTransport() = default;

    /**
     * @brief Initialize serial hardware
     * @return Result<void> - ok() on success, err() with ErrorCode on failure
     */
    virtual core::Result<void> init() = 0;

    /**
     * @brief Poll for incoming data
     *
     * Should be called regularly (e.g., in main loop).
     * Reads available bytes, decodes COBS frames, and invokes callbacks.
     */
    virtual void update() = 0;

    // ═══════════════════════════════════════════════════
    // Output
    // ═══════════════════════════════════════════════════

    /**
     * @brief Send a complete frame
     *
     * The implementation will COBS-encode the data and transmit.
     *
     * @param data Pointer to frame data
     * @param length Number of bytes to send
     */
    virtual void send(const uint8_t* data, size_t length) = 0;

    // ═══════════════════════════════════════════════════
    // Input callback
    // ═══════════════════════════════════════════════════

    using ReceiveCallback = std::function<void(const uint8_t* data, size_t len)>;

    /**
     * @brief Set callback for received frames
     *
     * Called when a complete COBS frame is decoded.
     *
     * @param cb Callback function receiving decoded frame data
     */
    virtual void setOnReceive(ReceiveCallback cb) = 0;
};

}  // namespace oc::hal
