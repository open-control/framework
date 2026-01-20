#pragma once

#include <cstdio>
#include <cstring>

#include "IStorageBackend.hpp"

namespace oc::hal {

/**
 * @brief File-based storage backend using standard C++ (fopen/fwrite)
 *
 * Works on:
 * - Native (Windows/Linux/macOS) → regular filesystem
 * - WASM → Emscripten IDBFS (requires FS.syncfs calls)
 *
 * File handle kept open for fast access. Call commit() to flush.
 */
class FileStorageBackend : public IStorageBackend {
public:
    /**
     * @param path File path (e.g., "/data/settings.bin" or "./settings.bin")
     * @param capacity Max addressable size (guard against wild addresses)
     */
    explicit FileStorageBackend(const char* path, size_t capacity = 64 * 1024)
        : path_(path), capacity_(capacity) {}

    ~FileStorageBackend() {
        if (file_) {
            std::fclose(file_);
        }
    }

    // Non-copyable
    FileStorageBackend(const FileStorageBackend&) = delete;
    FileStorageBackend& operator=(const FileStorageBackend&) = delete;

    bool begin() override {
        if (file_) return true;

        // Try open existing, else create
        file_ = std::fopen(path_, "r+b");
        if (!file_) {
            file_ = std::fopen(path_, "w+b");
        }
        return file_ != nullptr;
    }

    bool available() const override {
        return file_ != nullptr;
    }

    size_t read(uint32_t address, uint8_t* buffer, size_t size) override {
        if (!file_ || address + size > capacity_) return 0;

        std::fseek(file_, 0, SEEK_END);
        size_t fileSize = static_cast<size_t>(std::ftell(file_));

        if (address >= fileSize) {
            std::memset(buffer, 0xFF, size);
            return size;
        }

        std::fseek(file_, static_cast<long>(address), SEEK_SET);
        size_t bytesRead = std::fread(buffer, 1, size, file_);

        // Pad with 0xFF if file shorter than requested
        if (bytesRead < size) {
            std::memset(buffer + bytesRead, 0xFF, size - bytesRead);
        }
        return size;
    }

    size_t write(uint32_t address, const uint8_t* buffer, size_t size) override {
        if (!file_ || address + size > capacity_) return 0;

        // Extend file with 0xFF if writing beyond current size
        std::fseek(file_, 0, SEEK_END);
        size_t fileSize = static_cast<size_t>(std::ftell(file_));

        if (address > fileSize) {
            // Fill gap with 0xFF
            size_t gap = address - fileSize;
            while (gap > 0) {
                size_t chunk = (gap > sizeof(PADDING)) ? sizeof(PADDING) : gap;
                std::fwrite(PADDING, 1, chunk, file_);
                gap -= chunk;
            }
        }

        std::fseek(file_, static_cast<long>(address), SEEK_SET);
        return std::fwrite(buffer, 1, size, file_);
    }

    bool commit() override {
        if (!file_) return false;
        std::fflush(file_);
        return true;
    }

    bool erase(uint32_t address, size_t size) override {
        if (!file_ || address + size > capacity_) return false;

        std::fseek(file_, static_cast<long>(address), SEEK_SET);
        size_t remaining = size;
        while (remaining > 0) {
            size_t chunk = (remaining > sizeof(PADDING)) ? sizeof(PADDING) : remaining;
            if (std::fwrite(PADDING, 1, chunk, file_) != chunk) {
                return false;
            }
            remaining -= chunk;
        }
        return true;
    }

    size_t capacity() const override {
        return capacity_;
    }

private:
    static constexpr uint8_t PADDING[64] = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    };

    const char* path_;
    size_t capacity_;
    FILE* file_ = nullptr;
};

// Static member definition (C++17 inline would be cleaner but staying compatible)
constexpr uint8_t FileStorageBackend::PADDING[64];

}  // namespace oc::hal
