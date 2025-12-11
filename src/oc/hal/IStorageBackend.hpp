#pragma once

#include <cstddef>
#include <cstdint>

namespace oc::hal {

/**
 * @brief Interface for persistent storage backends
 *
 * Abstracts different storage types: EEPROM, Flash, SD card, etc.
 * Implementations handle platform-specific details.
 *
 * Usage:
 * @code
 * class EEPROMBackend : public IStorageBackend {
 *     // ... platform-specific implementation
 * };
 *
 * EEPROMBackend storage;
 * Settings<MySettings> settings(storage, 0x0000, 1);
 * @endcode
 */
class IStorageBackend {
public:
    virtual ~IStorageBackend() = default;

    /**
     * @brief Check if storage hardware is available
     */
    virtual bool available() const = 0;

    /**
     * @brief Read bytes from storage
     * @param address Start address
     * @param buffer Destination buffer
     * @param size Number of bytes to read
     * @return Bytes actually read (0 on error)
     */
    virtual size_t read(uint32_t address, uint8_t* buffer, size_t size) = 0;

    /**
     * @brief Write bytes to storage (may be buffered)
     * @param address Start address
     * @param buffer Source data
     * @param size Number of bytes to write
     * @return Bytes actually written (0 on error)
     * @note Call commit() to ensure persistence on buffered backends
     */
    virtual size_t write(uint32_t address, const uint8_t* buffer, size_t size) = 0;

    /**
     * @brief Ensure all pending writes are persisted
     * @return true if successful
     *
     * Behavior by backend type:
     * - EEPROM: No-op (writes are immediate)
     * - Flash: Erases sector if needed, writes buffer
     * - SD/File: Flushes file buffer
     *
     * @note May be slow (Flash erase ~10-100ms). Call sparingly.
     */
    virtual bool commit() { return true; }

    /**
     * @brief Erase a region of storage
     * @param address Start address
     * @param size Number of bytes to erase
     * @return true if successful
     *
     * Sets region to 0xFF (erased state).
     * Use for factory reset or clearing corrupted data.
     */
    virtual bool erase(uint32_t address, size_t size) { return true; }

    /**
     * @brief Total storage capacity in bytes
     */
    virtual size_t capacity() const = 0;
};

}  // namespace oc::hal
