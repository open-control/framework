#pragma once

#include <cstddef>
#include <cstdint>

#include <oc/type/Result.hpp>

namespace oc::interface {

inline constexpr size_t FILESYSTEM_MAX_PATH_LENGTH = 192;
inline constexpr size_t FILESYSTEM_MAX_NAME_LENGTH = 64;

enum class FileType : uint8_t {
    MISSING = 0,
    FILE,
    DIRECTORY,
    OTHER,
};

enum class RemoveMode : uint8_t {
    FILE_OR_EMPTY_DIRECTORY = 0,
    RECURSIVE,
};

struct FileInfo {
    FileType type = FileType::MISSING;
    uint32_t sizeBytes = 0;

    bool exists() const {
        return type != FileType::MISSING;
    }
};

struct DirectoryEntry {
    char name[FILESYSTEM_MAX_NAME_LENGTH] = {};
    FileType type = FileType::MISSING;
    uint32_t sizeBytes = 0;
    bool nameTruncated = false;
};

using DirectoryEntryVisitor = bool (*)(const DirectoryEntry& entry, void* context);

/**
 * Filesystem abstraction for user-visible file trees.
 *
 * This is intentionally separate from IStorage. IStorage is byte-addressed and
 * suited for fixed internal blobs; IFileSystem is for named files, directories,
 * and PC/controller transfer flows.
 *
 * The interface does not allocate and never returns dynamic containers. Callers
 * provide buffers, and directory listing streams entries through a visitor.
 */
class IFileSystem {
public:
    virtual ~IFileSystem() = default;

    virtual oc::type::Result<void> init() = 0;
    virtual bool available() const = 0;

    virtual oc::type::Result<FileInfo> stat(const char* path) = 0;

    virtual oc::type::Result<void> list(const char* path,
                                        DirectoryEntryVisitor visitor,
                                        void* context) = 0;

    virtual oc::type::Result<void> createDirectory(const char* path) = 0;

    virtual oc::type::Result<void> remove(
        const char* path,
        RemoveMode mode = RemoveMode::FILE_OR_EMPTY_DIRECTORY
    ) = 0;

    virtual oc::type::Result<void> rename(const char* fromPath, const char* toPath) = 0;

    virtual oc::type::Result<size_t> read(const char* path,
                                          uint32_t offset,
                                          uint8_t* buffer,
                                          size_t size) = 0;

    virtual oc::type::Result<size_t> write(const char* path,
                                           uint32_t offset,
                                           const uint8_t* data,
                                           size_t size) = 0;

    virtual oc::type::Result<void> flush(const char* path) = 0;

    /**
     * Start one exclusive sequential write session.
     *
     * The target path must be validated like write(). Implementations may
     * replace the target in place or stage it internally, but only one session
     * can be active at a time. expectedSize is authoritative: appendWrite()
     * must reject writes beyond it, and finishWrite() must fail if the exact
     * byte count has not been appended.
     */
    virtual oc::type::Result<void> beginWrite(const char* path,
                                              uint32_t expectedSize) = 0;

    /**
     * Append the next contiguous chunk to the active write session.
     *
     * A successful result must report exactly the number of bytes committed by
     * the backend. Callers must treat a short successful write as a storage
     * failure and abort the session.
     */
    virtual oc::type::Result<size_t> appendWrite(const uint8_t* data,
                                                 size_t size) = 0;

    /**
     * Commit the active write session.
     *
     * On success, all expected bytes are durable according to the backend's
     * normal flush/sync semantics. On failure, the session is no longer
     * usable; callers may still call abortWrite() for best-effort cleanup.
     */
    virtual oc::type::Result<void> finishWrite() = 0;

    /**
     * Abort the active write session.
     *
     * This is best-effort cleanup and must be safe to call when no session is
     * active.
     */
    virtual void abortWrite() = 0;
};

}  // namespace oc::interface
