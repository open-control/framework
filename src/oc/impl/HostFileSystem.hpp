#pragma once

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include <oc/interface/IFileSystem.hpp>

namespace oc::impl {

class HostFileSystem : public interface::IFileSystem {
public:
    explicit HostFileSystem(const char* rootPath)
        : rootPath_(rootPath ? rootPath : "") {}

    oc::type::Result<void> init() override {
        if (rootPath_.empty()) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::INVALID_ARGUMENT, "empty filesystem root"}
            );
        }

        std::error_code ec;
        rootPath_ = std::filesystem::absolute(rootPath_, ec);
        if (ec) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::INVALID_ARGUMENT, "invalid filesystem root"}
            );
        }

        std::filesystem::create_directories(rootPath_, ec);
        if (ec) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::STORAGE_WRITE_FAILED, "create root failed"}
            );
        }

        rootPath_ = std::filesystem::weakly_canonical(rootPath_, ec);
        if (ec) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::INVALID_ARGUMENT, "canonical root failed"}
            );
        }

        initialized_ = true;
        return oc::type::Result<void>::ok();
    }

    bool available() const override {
        return initialized_;
    }

    oc::type::Result<interface::FileInfo> stat(const char* path) override {
        std::filesystem::path resolved;
        auto pathResult = resolvePath_(path, resolved);
        if (!pathResult) {
            return oc::type::Result<interface::FileInfo>::err(pathResult.error());
        }

        std::error_code ec;
        if (!std::filesystem::exists(resolved, ec)) {
            return oc::type::Result<interface::FileInfo>::err(
                {oc::type::ErrorCode::RESOURCE_NOT_FOUND, "path not found"}
            );
        }
        if (ec) {
            return oc::type::Result<interface::FileInfo>::err(
                {oc::type::ErrorCode::STORAGE_READ_FAILED, "stat failed"}
            );
        }

        interface::FileInfo info{};
        info.type = typeOf_(resolved);
        if (info.type == interface::FileType::FILE) {
            const auto size = std::filesystem::file_size(resolved, ec);
            if (ec || size > UINT32_MAX) {
                return oc::type::Result<interface::FileInfo>::err(
                    {oc::type::ErrorCode::STORAGE_READ_FAILED, "file size failed"}
                );
            }
            info.sizeBytes = static_cast<uint32_t>(size);
        }

        return oc::type::Result<interface::FileInfo>::ok(info);
    }

    oc::type::Result<void> list(const char* path,
                                interface::DirectoryEntryVisitor visitor,
                                void* context) override {
        if (!visitor) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::INVALID_ARGUMENT, "null directory visitor"}
            );
        }

        std::filesystem::path resolved;
        auto pathResult = resolvePath_(path, resolved);
        if (!pathResult) {
            return pathResult;
        }

        std::error_code ec;
        if (!std::filesystem::exists(resolved, ec)) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::RESOURCE_NOT_FOUND, "directory not found"}
            );
        }
        if (!std::filesystem::is_directory(resolved, ec)) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::INVALID_ARGUMENT, "path is not a directory"}
            );
        }

        std::filesystem::directory_iterator iterator(resolved, ec);
        if (ec) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::STORAGE_READ_FAILED, "list failed"}
            );
        }

        for (const auto& item : iterator) {
            interface::DirectoryEntry entry{};
            fillEntry_(item.path(), entry);
            if (!visitor(entry, context)) {
                break;
            }
        }

        return oc::type::Result<void>::ok();
    }

    oc::type::Result<void> createDirectory(const char* path) override {
        std::filesystem::path resolved;
        auto pathResult = resolvePath_(path, resolved);
        if (!pathResult) {
            return pathResult;
        }

        std::error_code ec;
        if (std::filesystem::exists(resolved, ec) && !std::filesystem::is_directory(resolved, ec)) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::INVALID_STATE, "path exists and is not a directory"}
            );
        }

        std::filesystem::create_directories(resolved, ec);
        if (ec) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::STORAGE_WRITE_FAILED, "create directory failed"}
            );
        }

        return oc::type::Result<void>::ok();
    }

    oc::type::Result<void> remove(
        const char* path,
        interface::RemoveMode mode = interface::RemoveMode::FILE_OR_EMPTY_DIRECTORY
    ) override {
        std::filesystem::path resolved;
        auto pathResult = resolvePath_(path, resolved);
        if (!pathResult) {
            return pathResult;
        }
        if (resolved == rootPath_) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::INVALID_ARGUMENT, "cannot remove filesystem root"}
            );
        }

        std::error_code ec;
        if (!std::filesystem::exists(resolved, ec)) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::RESOURCE_NOT_FOUND, "path not found"}
            );
        }

        const auto removed = mode == interface::RemoveMode::RECURSIVE
            ? std::filesystem::remove_all(resolved, ec)
            : (std::filesystem::remove(resolved, ec) ? 1u : 0u);

        if (ec) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::STORAGE_WRITE_FAILED, "remove failed"}
            );
        }
        if (removed == 0) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::STORAGE_WRITE_FAILED, "remove did not delete path"}
            );
        }

        return oc::type::Result<void>::ok();
    }

    oc::type::Result<void> rename(const char* fromPath, const char* toPath) override {
        std::filesystem::path fromResolved;
        auto fromResult = resolvePath_(fromPath, fromResolved);
        if (!fromResult) {
            return fromResult;
        }
        if (fromResolved == rootPath_) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::INVALID_ARGUMENT, "cannot rename filesystem root"}
            );
        }

        std::filesystem::path toResolved;
        auto toResult = resolvePath_(toPath, toResolved);
        if (!toResult) {
            return toResult;
        }

        std::error_code ec;
        if (!std::filesystem::exists(fromResolved, ec)) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::RESOURCE_NOT_FOUND, "source not found"}
            );
        }
        if (std::filesystem::exists(toResolved, ec)) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::INVALID_STATE, "target already exists"}
            );
        }
        const auto parent = toResolved.parent_path();
        if (!parent.empty() && !std::filesystem::exists(parent, ec)) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::RESOURCE_NOT_FOUND, "target parent not found"}
            );
        }

        std::filesystem::rename(fromResolved, toResolved, ec);
        if (ec) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::STORAGE_WRITE_FAILED, "rename failed"}
            );
        }

        return oc::type::Result<void>::ok();
    }

    oc::type::Result<size_t> read(const char* path,
                                  uint32_t offset,
                                  uint8_t* buffer,
                                  size_t size) override {
        if (!buffer && size > 0) {
            return oc::type::Result<size_t>::err(
                {oc::type::ErrorCode::INVALID_ARGUMENT, "null read buffer"}
            );
        }

        std::filesystem::path resolved;
        auto pathResult = resolvePath_(path, resolved);
        if (!pathResult) {
            return oc::type::Result<size_t>::err(pathResult.error());
        }

        std::error_code ec;
        if (!std::filesystem::exists(resolved, ec)) {
            return oc::type::Result<size_t>::err(
                {oc::type::ErrorCode::RESOURCE_NOT_FOUND, "file not found"}
            );
        }
        if (!std::filesystem::is_regular_file(resolved, ec)) {
            return oc::type::Result<size_t>::err(
                {oc::type::ErrorCode::INVALID_ARGUMENT, "path is not a file"}
            );
        }

        const auto fileSize = std::filesystem::file_size(resolved, ec);
        if (ec) {
            return oc::type::Result<size_t>::err(
                {oc::type::ErrorCode::STORAGE_READ_FAILED, "file size failed"}
            );
        }
        if (offset >= fileSize || size == 0) {
            return oc::type::Result<size_t>::ok(0);
        }

        const auto maxRead = static_cast<size_t>(
            std::min<uintmax_t>(static_cast<uintmax_t>(size), fileSize - offset)
        );

        std::ifstream file(resolved, std::ios::binary);
        if (!file) {
            return oc::type::Result<size_t>::err(
                {oc::type::ErrorCode::STORAGE_READ_FAILED, "open read failed"}
            );
        }

        file.seekg(static_cast<std::streamoff>(offset));
        file.read(reinterpret_cast<char*>(buffer), static_cast<std::streamsize>(maxRead));
        const auto readBytes = static_cast<size_t>(file.gcount());
        if (readBytes != maxRead && file.bad()) {
            return oc::type::Result<size_t>::err(
                {oc::type::ErrorCode::STORAGE_READ_FAILED, "read failed"}
            );
        }

        return oc::type::Result<size_t>::ok(readBytes);
    }

    oc::type::Result<size_t> write(const char* path,
                                   uint32_t offset,
                                   const uint8_t* data,
                                   size_t size) override {
        if (!data && size > 0) {
            return oc::type::Result<size_t>::err(
                {oc::type::ErrorCode::INVALID_ARGUMENT, "null write buffer"}
            );
        }

        std::filesystem::path resolved;
        auto pathResult = resolvePath_(path, resolved);
        if (!pathResult) {
            return oc::type::Result<size_t>::err(pathResult.error());
        }
        if (resolved == rootPath_) {
            return oc::type::Result<size_t>::err(
                {oc::type::ErrorCode::INVALID_ARGUMENT, "cannot write filesystem root"}
            );
        }

        std::error_code ec;
        const auto parent = resolved.parent_path();
        if (!parent.empty() && !std::filesystem::exists(parent, ec)) {
            return oc::type::Result<size_t>::err(
                {oc::type::ErrorCode::RESOURCE_NOT_FOUND, "parent directory not found"}
            );
        }
        if (std::filesystem::exists(resolved, ec) && std::filesystem::is_directory(resolved, ec)) {
            return oc::type::Result<size_t>::err(
                {oc::type::ErrorCode::INVALID_ARGUMENT, "path is a directory"}
            );
        }

        uintmax_t currentSize = 0;
        if (std::filesystem::exists(resolved, ec)) {
            currentSize = std::filesystem::file_size(resolved, ec);
            if (ec) {
                return oc::type::Result<size_t>::err(
                    {oc::type::ErrorCode::STORAGE_READ_FAILED, "file size failed"}
                );
            }
        }
        if (offset > currentSize) {
            return oc::type::Result<size_t>::err(
                {oc::type::ErrorCode::INVALID_ARGUMENT, "write gap not allowed"}
            );
        }
        if (size == 0) {
            return oc::type::Result<size_t>::ok(0);
        }

        if (!std::filesystem::exists(resolved, ec)) {
            std::ofstream create(resolved, std::ios::binary);
            if (!create) {
                return oc::type::Result<size_t>::err(
                    {oc::type::ErrorCode::STORAGE_WRITE_FAILED, "create file failed"}
                );
            }
        }

        std::fstream file(resolved, std::ios::binary | std::ios::in | std::ios::out);
        if (!file) {
            return oc::type::Result<size_t>::err(
                {oc::type::ErrorCode::STORAGE_WRITE_FAILED, "open write failed"}
            );
        }

        file.seekp(static_cast<std::streamoff>(offset));
        file.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
        file.flush();
        if (!file) {
            return oc::type::Result<size_t>::err(
                {oc::type::ErrorCode::STORAGE_WRITE_FAILED, "write failed"}
            );
        }

        return oc::type::Result<size_t>::ok(size);
    }

    oc::type::Result<void> flush(const char* path) override {
        std::filesystem::path resolved;
        auto pathResult = resolvePath_(path, resolved);
        if (!pathResult) {
            return pathResult;
        }

        std::error_code ec;
        if (!std::filesystem::exists(resolved, ec)) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::RESOURCE_NOT_FOUND, "path not found"}
            );
        }
        return oc::type::Result<void>::ok();
    }

    oc::type::Result<void> beginWrite(const char* path, uint32_t expectedSize) override {
        if (writeActive_) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::INVALID_STATE, "write stream already active"}
            );
        }

        std::filesystem::path resolved;
        auto pathResult = resolvePath_(path, resolved);
        if (!pathResult) {
            return pathResult;
        }
        if (resolved == rootPath_) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::INVALID_ARGUMENT, "cannot write filesystem root"}
            );
        }

        std::error_code ec;
        const auto parent = resolved.parent_path();
        if (!parent.empty() && !std::filesystem::exists(parent, ec)) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::RESOURCE_NOT_FOUND, "parent directory not found"}
            );
        }
        if (std::filesystem::exists(resolved, ec) && std::filesystem::is_directory(resolved, ec)) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::INVALID_ARGUMENT, "path is a directory"}
            );
        }

        writeStream_.open(
            resolved,
            std::ios::binary | std::ios::in | std::ios::out | std::ios::trunc
        );
        if (!writeStream_) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::STORAGE_WRITE_FAILED, "open write stream failed"}
            );
        }

        writeExpectedSize_ = expectedSize;
        writeBytes_ = 0;
        writeActive_ = true;
        return oc::type::Result<void>::ok();
    }

    oc::type::Result<size_t> appendWrite(const uint8_t* data, size_t size) override {
        if (!data && size > 0) {
            return oc::type::Result<size_t>::err(
                {oc::type::ErrorCode::INVALID_ARGUMENT, "null write buffer"}
            );
        }
        if (!writeActive_ || !writeStream_) {
            return oc::type::Result<size_t>::err(
                {oc::type::ErrorCode::INVALID_STATE, "write stream is not active"}
            );
        }
        if (writeBytes_ + size > writeExpectedSize_) {
            return oc::type::Result<size_t>::err(
                {oc::type::ErrorCode::INVALID_ARGUMENT, "write exceeds expected size"}
            );
        }
        if (size == 0) {
            return oc::type::Result<size_t>::ok(0);
        }

        writeStream_.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
        if (!writeStream_) {
            return oc::type::Result<size_t>::err(
                {oc::type::ErrorCode::STORAGE_WRITE_FAILED, "write stream failed"}
            );
        }

        writeBytes_ += size;
        return oc::type::Result<size_t>::ok(size);
    }

    oc::type::Result<void> finishWrite() override {
        if (!writeActive_ || !writeStream_) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::INVALID_STATE, "write stream is not active"}
            );
        }
        if (writeBytes_ != writeExpectedSize_) {
            abortWrite();
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::INVALID_STATE, "write stream size mismatch"}
            );
        }

        writeStream_.flush();
        const bool ok = static_cast<bool>(writeStream_);
        writeStream_.close();
        resetWriteStream_();
        if (!ok) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::STORAGE_WRITE_FAILED, "finish write stream failed"}
            );
        }
        return oc::type::Result<void>::ok();
    }

    void abortWrite() override {
        if (writeStream_.is_open()) {
            writeStream_.close();
        }
        resetWriteStream_();
    }

private:
    oc::type::Result<void> ensureInitialized_() const {
        if (!initialized_) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::INVALID_STATE, "filesystem not initialized"}
            );
        }
        return oc::type::Result<void>::ok();
    }

    oc::type::Result<void> resolvePath_(const char* path, std::filesystem::path& out) const {
        auto initResult = ensureInitialized_();
        if (!initResult) {
            return initResult;
        }
        if (!path || path[0] == '\0') {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::INVALID_ARGUMENT, "empty path"}
            );
        }
        const size_t pathLength = std::strlen(path);
        if (pathLength > interface::FILESYSTEM_MAX_PATH_LENGTH) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::INVALID_ARGUMENT, "path too long"}
            );
        }

        std::filesystem::path relative;
        size_t segmentStart = 0;
        while (segmentStart < pathLength && path[segmentStart] == '/') {
            ++segmentStart;
        }

        while (segmentStart < pathLength) {
            size_t segmentEnd = segmentStart;
            while (segmentEnd < pathLength && path[segmentEnd] != '/') {
                if (path[segmentEnd] == '\\' || path[segmentEnd] == ':' ||
                    std::iscntrl(static_cast<unsigned char>(path[segmentEnd]))) {
                    return oc::type::Result<void>::err(
                        {oc::type::ErrorCode::INVALID_ARGUMENT, "invalid path character"}
                    );
                }
                ++segmentEnd;
            }

            const size_t segmentLength = segmentEnd - segmentStart;
            if (segmentLength > 0) {
                if (segmentLength > interface::FILESYSTEM_MAX_NAME_LENGTH) {
                    return oc::type::Result<void>::err(
                        {oc::type::ErrorCode::INVALID_ARGUMENT, "path segment too long"}
                    );
                }
                if (segmentLength == 1 && path[segmentStart] == '.') {
                    return oc::type::Result<void>::err(
                        {oc::type::ErrorCode::INVALID_ARGUMENT, "dot segment not allowed"}
                    );
                }
                if (segmentLength == 2 && path[segmentStart] == '.' &&
                    path[segmentStart + 1] == '.') {
                    return oc::type::Result<void>::err(
                        {oc::type::ErrorCode::INVALID_ARGUMENT, "parent segment not allowed"}
                    );
                }

                relative /= std::string(path + segmentStart, segmentLength);
            }

            segmentStart = segmentEnd;
            while (segmentStart < pathLength && path[segmentStart] == '/') {
                ++segmentStart;
            }
        }

        const auto candidate = (rootPath_ / relative).lexically_normal();
        if (!isWithinRoot_(candidate)) {
            return oc::type::Result<void>::err(
                {oc::type::ErrorCode::INVALID_ARGUMENT, "path escapes root"}
            );
        }
        out = candidate;
        return oc::type::Result<void>::ok();
    }

    bool isWithinRoot_(const std::filesystem::path& path) const {
        auto rootIt = rootPath_.begin();
        auto pathIt = path.begin();

        for (; rootIt != rootPath_.end(); ++rootIt, ++pathIt) {
            if (pathIt == path.end() || *rootIt != *pathIt) {
                return false;
            }
        }
        return true;
    }

    static interface::FileType typeOf_(const std::filesystem::path& path) {
        std::error_code ec;
        if (std::filesystem::is_regular_file(path, ec)) {
            return interface::FileType::FILE;
        }
        if (std::filesystem::is_directory(path, ec)) {
            return interface::FileType::DIRECTORY;
        }
        if (std::filesystem::exists(path, ec)) {
            return interface::FileType::OTHER;
        }
        return interface::FileType::MISSING;
    }

    static void copyName_(const std::string& source, interface::DirectoryEntry& entry) {
        const size_t copyLength = std::min(
            source.size(),
            interface::FILESYSTEM_MAX_NAME_LENGTH - 1
        );
        std::memcpy(entry.name, source.data(), copyLength);
        entry.name[copyLength] = '\0';
        entry.nameTruncated = source.size() >= interface::FILESYSTEM_MAX_NAME_LENGTH;
    }

    static void fillEntry_(const std::filesystem::path& path, interface::DirectoryEntry& entry) {
        copyName_(path.filename().string(), entry);
        entry.type = typeOf_(path);
        if (entry.type == interface::FileType::FILE) {
            std::error_code ec;
            const auto size = std::filesystem::file_size(path, ec);
            entry.sizeBytes = (!ec && size <= UINT32_MAX) ? static_cast<uint32_t>(size) : 0;
        }
    }

    void resetWriteStream_() {
        writeExpectedSize_ = 0;
        writeBytes_ = 0;
        writeActive_ = false;
    }

    std::filesystem::path rootPath_;
    std::fstream writeStream_;
    uint32_t writeExpectedSize_ = 0;
    uint32_t writeBytes_ = 0;
    bool writeActive_ = false;
    bool initialized_ = false;
};

}  // namespace oc::impl
