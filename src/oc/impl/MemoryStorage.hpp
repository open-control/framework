#pragma once

#include <oc/interface/IStorage.hpp>
#include <vector>
#include <cstring>
#include <algorithm>

namespace oc::impl {

class MemoryStorage : public interface::IStorage {
public:
    explicit MemoryStorage(size_t capacity = 4096) : data_(capacity, 0xFF) {}

    oc::type::Result<void> init() override { return oc::type::Result<void>::ok(); }
    bool available() const override { return true; }

    size_t read(uint32_t address, uint8_t* buffer, size_t size) override {
        if (address >= data_.size()) return 0;
        size_t toRead = std::min(size, data_.size() - address);
        std::memcpy(buffer, data_.data() + address, toRead);
        return toRead;
    }

    size_t write(uint32_t address, const uint8_t* buffer, size_t size) override {
        if (address >= data_.size()) return 0;
        size_t toWrite = std::min(size, data_.size() - address);
        std::memcpy(data_.data() + address, buffer, toWrite);
        return toWrite;
    }

    bool commit() override { return true; }

    bool erase(uint32_t address, size_t size) override {
        if (address >= data_.size()) return false;
        size_t toErase = std::min(size, data_.size() - address);
        std::memset(data_.data() + address, 0xFF, toErase);
        return true;
    }

    size_t capacity() const override { return data_.size(); }

private:
    std::vector<uint8_t> data_;
};

}  // namespace oc::impl
