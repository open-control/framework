#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

#include <oc/core/Result.hpp>

namespace oc::hal {

/**
 * @brief Interface for frame-based transport abstraction
 *
 * Provides a platform-agnostic interface for sending and receiving
 * complete binary frames. Used by protocol layers (e.g., BitwigProtocol)
 * for structured communication.
 *
 * Implementations handle their own framing internally:
 * - Stream transports (Serial, UART): use COBS encoding
 * - Datagram transports (UDP): frames are datagrams (no encoding needed)
 *
 * @note This is for raw frame transport. Protocol encoding
 *       is handled by the protocol layer above.
 */
class IFrameTransport {
public:
    virtual ~IFrameTransport() = default;

    /**
     * @brief Initialize transport hardware
     * @return Result<void> - ok() on success, err() with ErrorCode on failure
     */
    virtual core::Result<void> init() = 0;

    /**
     * @brief Poll for incoming data
     *
     * Should be called regularly (e.g., in main loop).
     * Reads available data and invokes callbacks for complete frames.
     */
    virtual void update() = 0;

    // ═══════════════════════════════════════════════════
    // Output
    // ═══════════════════════════════════════════════════

    /**
     * @brief Send a complete frame
     *
     * The implementation handles framing as needed for the transport.
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
     * Called when a complete frame is received.
     *
     * @param cb Callback function receiving frame data
     */
    virtual void setOnReceive(ReceiveCallback cb) = 0;

    // ═══════════════════════════════════════════════════
    // Connection state (optional, with defaults for retrocompat)
    // ═══════════════════════════════════════════════════

    /**
     * @brief Check if transport is ready to send/receive
     *
     * For connection-oriented transports (WebSocket), indicates if connected.
     * For connectionless transports (UDP, Serial), always returns true.
     *
     * @return true if transport is operational
     */
    virtual bool isReady() const { return true; }
};

}  // namespace oc::hal
