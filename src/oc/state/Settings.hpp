#pragma once

#include <cstdint>
#include <cstring>
#include <type_traits>

#include <oc/core/Result.hpp>
#include <oc/hal/IStorageBackend.hpp>

namespace oc::state {

/**
 * @brief Header stored at beginning of settings block
 */
struct SettingsHeader {
    uint32_t magic = 0x4F435354;  ///< "OCST" magic number
    uint16_t version;             ///< Data format version
    uint16_t size;                ///< Size of data (excluding header)
    uint32_t checksum;            ///< CRC32 of data
};

/**
 * @brief Generic settings container with persistence
 *
 * @tparam T Settings struct type (must be trivially copyable / POD)
 *
 * Features:
 * - Automatic checksum validation
 * - Version-based migration support
 * - Dirty tracking (only saves when changed)
 * - Safe defaults on corruption/version mismatch
 *
 * Usage:
 * @code
 * struct MySettings {
 *     uint8_t midiChannel = 1;
 *     float volume = 0.5f;
 *     char presetName[32] = "Default";
 * };
 *
 * EEPROMBackend eeprom;
 * Settings<MySettings> settings(eeprom, 0x0000, 1);
 *
 * settings.load();
 * settings.modify([](auto& s) { s.volume = 0.75f; });
 * settings.save();
 * @endcode
 */
template <typename T>
class Settings {
    static_assert(std::is_trivially_copyable_v<T>,
                  "Settings type must be trivially copyable (POD)");

public:
    /**
     * @brief Construct settings container
     * @param backend Storage backend
     * @param address Start address in storage
     * @param version Data format version (increment when struct changes)
     */
    Settings(hal::IStorageBackend& backend, uint32_t address, uint16_t version)
        : backend_(backend), address_(address), version_(version) {}

    /**
     * @brief Load settings from storage
     *
     * On success: data_ contains loaded values, dirty_ = false
     * On failure (missing/corrupt/version mismatch): data_ = defaults, dirty_ = true
     *
     * @return Ok if loaded successfully, Err with reason otherwise
     */
    core::Result<void> load() {
        using R = core::Result<void>;
        using E = core::ErrorCode;

        SettingsHeader header;

        // Read header
        if (backend_.read(address_, reinterpret_cast<uint8_t*>(&header),
                          sizeof(header)) != sizeof(header)) {
            resetToDefaults();
            return R::err({E::STORAGE_READ_FAILED, "header"});
        }

        // Check magic
        if (header.magic != SettingsHeader{}.magic) {
            resetToDefaults();
            return R::ok();  // First boot, defaults OK
        }

        // Check version
        if (header.version != version_) {
            if (!migrate(header.version)) {
                resetToDefaults();
            }
            dirty_ = true;
            return R::ok();
        }

        // Check size
        if (header.size != sizeof(T)) {
            resetToDefaults();
            return R::err({E::STORAGE_CORRUPT, "size mismatch"});
        }

        // Read data
        if (backend_.read(address_ + sizeof(header),
                          reinterpret_cast<uint8_t*>(&data_),
                          sizeof(T)) != sizeof(T)) {
            resetToDefaults();
            return R::err({E::STORAGE_READ_FAILED, "data"});
        }

        // Verify checksum
        if (computeCRC32() != header.checksum) {
            resetToDefaults();
            return R::err({E::STORAGE_CORRUPT, "checksum"});
        }

        dirty_ = false;
        return R::ok();
    }

    /**
     * @brief Save settings to storage (only if dirty)
     * @return Ok if saved (or not dirty), Err on write failure
     */
    core::Result<void> save() {
        using R = core::Result<void>;
        using E = core::ErrorCode;

        if (!dirty_) return R::ok();

        SettingsHeader header{};
        header.version = version_;
        header.size = sizeof(T);
        header.checksum = computeCRC32();

        // Write header
        if (backend_.write(address_, reinterpret_cast<const uint8_t*>(&header),
                           sizeof(header)) != sizeof(header)) {
            return R::err({E::STORAGE_WRITE_FAILED, "header"});
        }

        // Write data
        if (backend_.write(address_ + sizeof(header),
                           reinterpret_cast<const uint8_t*>(&data_),
                           sizeof(T)) != sizeof(T)) {
            return R::err({E::STORAGE_WRITE_FAILED, "data"});
        }

        // Commit to storage
        if (!backend_.commit()) {
            return R::err({E::STORAGE_WRITE_FAILED, "commit"});
        }

        dirty_ = false;
        return R::ok();
    }

    /**
     * @brief Reload from storage (discard unsaved changes)
     */
    core::Result<void> reload() { return load(); }

    /**
     * @brief Reset to default values (marks dirty)
     */
    void resetToDefaults() {
        data_ = T{};
        dirty_ = true;
    }

    /**
     * @brief Factory reset (erase + defaults + save)
     */
    core::Result<void> factoryReset() {
        using R = core::Result<void>;
        using E = core::ErrorCode;

        size_t totalSize = sizeof(SettingsHeader) + sizeof(T);
        if (!backend_.erase(address_, totalSize)) {
            return R::err({E::STORAGE_WRITE_FAILED, "erase"});
        }
        resetToDefaults();
        return save();
    }

    /// Read-only access to data
    const T& get() const { return data_; }

    /// Modify data (marks dirty)
    template <typename F>
    void modify(F&& fn) {
        fn(data_);
        dirty_ = true;
    }

    /// Check for unsaved changes
    bool isDirty() const { return dirty_; }

    /// Total bytes used in storage
    static constexpr size_t storageSize() {
        return sizeof(SettingsHeader) + sizeof(T);
    }

protected:
    /**
     * @brief Override for custom migration logic
     * @param oldVersion Version of data in storage
     * @return true if migration successful, false to reset to defaults
     */
    virtual bool migrate(uint16_t oldVersion) {
        (void)oldVersion;
        return false;
    }

private:
    uint32_t computeCRC32() const {
        // CRC-32 (IEEE 802.3 polynomial)
        const uint8_t* data = reinterpret_cast<const uint8_t*>(&data_);
        uint32_t crc = 0xFFFFFFFF;

        for (size_t i = 0; i < sizeof(T); ++i) {
            crc ^= data[i];
            for (int j = 0; j < 8; ++j) {
                crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
            }
        }

        return ~crc;
    }

    hal::IStorageBackend& backend_;
    uint32_t address_;
    uint16_t version_;
    T data_{};
    bool dirty_ = true;
};

/**
 * @brief Helper for setting fixed-size strings
 * @tparam N Array size
 * @param dest Destination array
 * @param src Source string (can be longer, will be truncated)
 */
template <size_t N>
void setString(char (&dest)[N], const char* src) {
    if (src) {
        std::strncpy(dest, src, N - 1);
        dest[N - 1] = '\0';
    }
}

}  // namespace oc::state
