#pragma once

#include <cstddef>
#include <cstdint>

namespace oc::interface {

/**
 * @brief Interface for persistent storage backends
 *
 * Abstracts different storage types: EEPROM, Flash, SD card, filesystem, etc.
 * Implementations handle platform-specific details.
 *
 * Lifecycle:
 * 1. Construct backend with config (filename, capacity, etc.)
 * 2. Call begin() to initialize hardware/filesystem
 * 3. Use read()/write() for data access
 * 4. Call commit() to persist buffered writes (required for some backends)
 *
 * Usage:
 * @code
 * SDCardBackend storage("/settings.bin");
 * if (!storage.begin()) {
 *     // Handle init failure
 * }
 * storage.write(0x0000, data, size);
 * storage.commit();  // Persist to SD
 * @endcode
 */
class IStorage {
public:
    virtual ~IStorage() = default;

    /**
     * @brief Initialize the storage backend
     * @return true if initialization successful
     *
     * Must be called before any read/write operations.
     * May mount filesystem, open files, or initialize hardware.
     *
     * Behavior by backend type:
     * - EEPROM: Always succeeds (hardware always available)
     * - Flash/LittleFS: Mounts filesystem, may format on first use
     * - SD Card: Initializes SDIO, opens/creates file
     * - Native filesystem: Opens/creates file
     */
    virtual bool begin() = 0;

    /**
     * @brief Check if storage is initialized and available
     * @return true if begin() succeeded and storage is usable
     */
    virtual bool available() const = 0;

    /**
     * @brief Read bytes from storage
     * @param address Start address (byte offset)
     * @param buffer Destination buffer
     * @param size Number of bytes to read
     * @return Bytes actually read (0 on error)
     *
     * Reads from cache if available, otherwise from storage.
     * Addresses beyond current data return 0xFF (erased state).
     */
    virtual size_t read(uint32_t address, uint8_t* buffer, size_t size) = 0;

    /**
     * @brief Write bytes to storage (may be buffered)
     * @param address Start address (byte offset)
     * @param buffer Source data
     * @param size Number of bytes to write
     * @return Bytes actually written (0 on error)
     *
     * Behavior varies by backend:
     * - Immediate backends (EEPROM): Writes directly to hardware
     * - Buffered backends (SD, filesystem): Writes to RAM cache
     *
     * @note Call commit() to ensure persistence on buffered backends
     */
    virtual size_t write(uint32_t address, const uint8_t* buffer, size_t size) = 0;

    /**
     * @brief Persist all pending writes to storage
     * @return true if successful (or nothing to commit)
     *
     * Behavior by backend type:
     * - EEPROM: No-op (writes are immediate)
     * - Flash/LittleFS: No-op (handled internally)
     * - SD Card: Writes RAM cache to file (~3ms)
     * - Native filesystem: Flushes file buffer
     *
     * @note May block for several milliseconds. Call sparingly.
     */
    virtual bool commit() = 0;

    /**
     * @brief Erase a region of storage
     * @param address Start address
     * @param size Number of bytes to erase
     * @return true if successful
     *
     * Sets region to 0xFF (erased state).
     * Use for factory reset or clearing corrupted data.
     */
    virtual bool erase(uint32_t address, size_t size) = 0;

    /**
     * @brief Total storage capacity in bytes
     *
     * Returns the maximum addressable size.
     * Actual hardware may have more space (SD card, filesystem).
     */
    virtual size_t capacity() const = 0;

    /**
     * @brief Check if there are uncommitted writes
     * @return true if commit() would write data
     *
     * Useful for buffered backends to avoid unnecessary commits.
     * Immediate backends (EEPROM) always return false.
     */
    virtual bool isDirty() const { return false; }
};

}  // namespace oc::interface
