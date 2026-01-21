#pragma once

#include <oc/interface/IStorage.hpp>
#include <vector>
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace oc::impl {

/**
 * @brief File-based storage backend for desktop platforms
 *
 * Uses standard C file I/O (fopen/fread/fwrite) - works on Linux, macOS, Windows.
 * Data is buffered in RAM and written to file on commit().
 */
class FileStorage : public interface::IStorage {
public:
    explicit FileStorage(const char* path, size_t capacity = 64 * 1024)
        : path_(path), data_(capacity, 0xFF) {}

    ~FileStorage() override {
        if (dirty_) {
            commit();
        }
    }

    oc::type::Result<void> init() override {
        FILE* f = fopen(path_, "rb");
        if (f) {
            size_t bytesRead = fread(data_.data(), 1, data_.size(), f);
            fclose(f);
            (void)bytesRead;
        }
        // File not existing is OK - we start with 0xFF (erased state)
        available_ = true;
        return oc::type::Result<void>::ok();
    }

    bool available() const override { return available_; }

    size_t read(uint32_t address, uint8_t* buffer, size_t size) override {
        if (!available_ || address >= data_.size()) return 0;
        size_t toRead = std::min(size, data_.size() - address);
        std::memcpy(buffer, data_.data() + address, toRead);
        return toRead;
    }

    size_t write(uint32_t address, const uint8_t* buffer, size_t size) override {
        if (!available_ || address >= data_.size()) return 0;
        size_t toWrite = std::min(size, data_.size() - address);
        std::memcpy(data_.data() + address, buffer, toWrite);
        dirty_ = true;
        return toWrite;
    }

    bool commit() override {
        if (!dirty_) return true;

        FILE* f = fopen(path_, "wb");
        if (!f) return false;

        size_t written = fwrite(data_.data(), 1, data_.size(), f);
        fclose(f);

        if (written == data_.size()) {
            dirty_ = false;
            return true;
        }
        return false;
    }

    bool erase(uint32_t address, size_t size) override {
        if (!available_ || address >= data_.size()) return false;
        size_t toErase = std::min(size, data_.size() - address);
        std::memset(data_.data() + address, 0xFF, toErase);
        dirty_ = true;
        return true;
    }

    size_t capacity() const override { return data_.size(); }
    bool isDirty() const override { return dirty_; }

private:
    const char* path_;
    std::vector<uint8_t> data_;
    bool available_ = false;
    bool dirty_ = false;
};

}  // namespace oc::impl
