#pragma once

#include <oc/interface/IStorage.hpp>
#include <cstring>

namespace oc::impl {

class NullStorage : public interface::IStorage {
public:
    bool begin() override { return true; }
    bool available() const override { return true; }

    size_t read(uint32_t, uint8_t* buffer, size_t size) override {
        std::memset(buffer, 0xFF, size);
        return size;
    }

    size_t write(uint32_t, const uint8_t*, size_t size) override {
        return size;
    }

    bool commit() override { return true; }
    bool erase(uint32_t, size_t) override { return true; }
    size_t capacity() const override { return 64 * 1024; }
};

}  // namespace oc::impl
