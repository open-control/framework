#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <functional>
#include <type_traits>

#include <oc/type/Result.hpp>
#include <oc/interface/IStorage.hpp>

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
    Settings(interface::IStorage& backend, uint32_t address, uint16_t version)
        : backend_(backend), address_(address), version_(version) {}

    /**
     * @brief Load settings from storage
     *
     * On success: data_ contains loaded values, dirty_ = false
     * On failure (missing/corrupt/version mismatch): data_ = defaults, dirty_ = true
     *
     * @return Ok if loaded successfully, Err with reason otherwise
     */
    oc::type::Result<void> load() {
        using R = oc::type::Result<void>;
        using E = oc::type::ErrorCode;

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
    oc::type::Result<void> save() {
        using R = oc::type::Result<void>;
        using E = oc::type::ErrorCode;

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
    oc::type::Result<void> reload() { return load(); }

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
    oc::type::Result<void> factoryReset() {
        using R = oc::type::Result<void>;
        using E = oc::type::ErrorCode;

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

    interface::IStorage& backend_;
    uint32_t address_;
    T data_{};

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

    uint16_t version_;
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

// ═══════════════════════════════════════════════════════════════════════════════
// MIGRATION HELPERS
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Settings with chainable migration support
 *
 * Allows registering migration functions for version upgrades.
 *
 * @code
 * struct MySettingsV1 { int value = 0; };
 * struct MySettingsV2 { float value = 0.0f; };  // Changed type
 *
 * MigratableSettings<MySettingsV2> settings(backend, 0x0000, 2);
 * settings.onMigrate<MySettingsV1>(1, [](const MySettingsV1& old, MySettingsV2& current) {
 *     current.value = static_cast<float>(old.value);
 * });
 * settings.load();  // Will auto-migrate from v1 if found
 * @endcode
 *
 * @tparam T Current settings struct type (must be trivially copyable)
 * @tparam MaxMigrations Maximum number of migration handlers (default: 8)
 */
template <typename T, size_t MaxMigrations = 8>
class MigratableSettings : public Settings<T> {
public:
    using Settings<T>::Settings;

    /**
     * @brief Register a migration handler from an older version
     *
     * @tparam OldT The old settings struct type
     * @param fromVersion The version this handler migrates FROM
     * @param handler Function that converts old data to current format
     * @return Reference to this for chaining
     *
     * @code
     * settings.onMigrate<V1>(1, [](const V1& old, V2& cur) { cur.x = old.x; })
     *         .onMigrate<V0>(0, [](const V0& old, V2& cur) { cur.x = old.val; });
     * @endcode
     */
    template <typename OldT, typename Handler>
    MigratableSettings& onMigrate(uint16_t fromVersion, Handler&& handler) {
        static_assert(std::is_trivially_copyable_v<OldT>,
                      "Old settings type must be trivially copyable");

        if (migrationCount_ >= MaxMigrations) {
            return *this;  // Silent ignore if too many migrations
        }

        migrations_[migrationCount_++] = {
            fromVersion,
            sizeof(OldT),
            [h = std::forward<Handler>(handler)](const void* oldData, T& current) {
                h(*static_cast<const OldT*>(oldData), current);
            }
        };

        return *this;
    }

protected:
    bool migrate(uint16_t oldVersion) override {
        // Find matching migration handler
        for (size_t i = 0; i < migrationCount_; ++i) {
            if (migrations_[i].fromVersion == oldVersion) {
                // Read old data with the old size
                std::array<uint8_t, 512> buffer{};  // Max old struct size
                if (migrations_[i].oldSize > buffer.size()) {
                    return false;  // Old struct too large
                }

                size_t readSize = this->backend_.read(
                    this->address_ + sizeof(SettingsHeader),
                    buffer.data(),
                    migrations_[i].oldSize
                );

                if (readSize != migrations_[i].oldSize) {
                    return false;  // Read failed
                }

                // Reset to defaults, then apply migration
                this->data_ = T{};
                migrations_[i].handler(buffer.data(), this->data_);
                return true;
            }
        }

        return false;  // No handler found
    }

private:
    struct MigrationEntry {
        uint16_t fromVersion;
        size_t oldSize;
        std::function<void(const void*, T&)> handler;
    };

    std::array<MigrationEntry, MaxMigrations> migrations_{};
    size_t migrationCount_ = 0;
};

}  // namespace oc::state
