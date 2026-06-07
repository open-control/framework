#include <unity.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <oc/impl/HostFileSystem.hpp>

namespace {

struct ListedEntry {
    std::string name;
    oc::interface::FileType type = oc::interface::FileType::MISSING;
    uint32_t sizeBytes = 0;
};

struct EntryCollector {
    std::vector<ListedEntry> entries;
};

uint32_t rootCounter = 0;

std::filesystem::path makeRoot(const char* testName) {
    const auto root =
        std::filesystem::temp_directory_path() /
        ("oc-host-filesystem-" + std::string(testName) + "-" + std::to_string(++rootCounter));
    std::filesystem::remove_all(root);
    return root;
}

bool collectEntry(const oc::interface::DirectoryEntry& entry, void* context) {
    auto* collector = static_cast<EntryCollector*>(context);
    collector->entries.push_back(ListedEntry{
        entry.name,
        entry.type,
        entry.sizeBytes,
    });
    return true;
}

bool collectOneEntry(const oc::interface::DirectoryEntry& entry, void* context) {
    collectEntry(entry, context);
    return false;
}

void sortEntries(EntryCollector& collector) {
    std::sort(
        collector.entries.begin(),
        collector.entries.end(),
        [](const ListedEntry& a, const ListedEntry& b) {
            return a.name < b.name;
        }
    );
}

void writeFile(oc::impl::HostFileSystem& fs,
               const char* path,
               const uint8_t* data,
               size_t size) {
    auto result = fs.write(path, 0, data, size);
    TEST_ASSERT_TRUE(result.isOk());
    TEST_ASSERT_EQUAL(size, result.value());
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_init_creates_root_and_reports_available() {
    const auto root = makeRoot("init");
    oc::impl::HostFileSystem fs(root.string().c_str());

    TEST_ASSERT_FALSE(fs.available());
    TEST_ASSERT_TRUE(fs.init().isOk());
    TEST_ASSERT_TRUE(fs.available());
    TEST_ASSERT_TRUE(std::filesystem::is_directory(root));

    std::filesystem::remove_all(root);
}

void test_write_read_stat_and_flush() {
    const auto root = makeRoot("roundtrip");
    oc::impl::HostFileSystem fs(root.string().c_str());
    TEST_ASSERT_TRUE(fs.init().isOk());
    TEST_ASSERT_TRUE(fs.createDirectory("/midi-studio/tmp").isOk());

    const std::array<uint8_t, 4> first{{0x10, 0x11, 0x12, 0x13}};
    auto writeFirst = fs.write("/midi-studio/tmp/blob.bin", 0, first.data(), first.size());
    TEST_ASSERT_TRUE(writeFirst.isOk());
    TEST_ASSERT_EQUAL(first.size(), writeFirst.value());

    const std::array<uint8_t, 2> second{{0x20, 0x21}};
    auto writeSecond = fs.write("/midi-studio/tmp/blob.bin", 4, second.data(), second.size());
    TEST_ASSERT_TRUE(writeSecond.isOk());
    TEST_ASSERT_EQUAL(second.size(), writeSecond.value());

    auto stat = fs.stat("/midi-studio/tmp/blob.bin");
    TEST_ASSERT_TRUE(stat.isOk());
    TEST_ASSERT_EQUAL(oc::interface::FileType::FILE, stat.value().type);
    TEST_ASSERT_EQUAL_UINT32(6, stat.value().sizeBytes);

    std::array<uint8_t, 6> buffer{};
    auto read = fs.read("/midi-studio/tmp/blob.bin", 0, buffer.data(), buffer.size());
    TEST_ASSERT_TRUE(read.isOk());
    TEST_ASSERT_EQUAL(buffer.size(), read.value());
    TEST_ASSERT_EQUAL_UINT8(0x10, buffer[0]);
    TEST_ASSERT_EQUAL_UINT8(0x13, buffer[3]);
    TEST_ASSERT_EQUAL_UINT8(0x20, buffer[4]);
    TEST_ASSERT_EQUAL_UINT8(0x21, buffer[5]);

    auto eofRead = fs.read("/midi-studio/tmp/blob.bin", 6, buffer.data(), buffer.size());
    TEST_ASSERT_TRUE(eofRead.isOk());
    TEST_ASSERT_EQUAL(0u, eofRead.value());

    TEST_ASSERT_TRUE(fs.flush("/midi-studio/tmp/blob.bin").isOk());

    std::filesystem::remove_all(root);
}

void test_list_streams_directory_entries() {
    const auto root = makeRoot("list");
    oc::impl::HostFileSystem fs(root.string().c_str());
    TEST_ASSERT_TRUE(fs.init().isOk());
    TEST_ASSERT_TRUE(fs.createDirectory("/midi-studio/projects").isOk());
    TEST_ASSERT_TRUE(fs.createDirectory("/midi-studio/tmp").isOk());

    const std::array<uint8_t, 3> payload{{0x01, 0x02, 0x03}};
    writeFile(fs, "/midi-studio/tmp/a.bin", payload.data(), payload.size());

    EntryCollector collector;
    auto listed = fs.list("/midi-studio", collectEntry, &collector);
    TEST_ASSERT_TRUE(listed.isOk());
    sortEntries(collector);

    TEST_ASSERT_EQUAL(2u, collector.entries.size());
    TEST_ASSERT_EQUAL_STRING("projects", collector.entries[0].name.c_str());
    TEST_ASSERT_EQUAL(oc::interface::FileType::DIRECTORY, collector.entries[0].type);
    TEST_ASSERT_EQUAL_STRING("tmp", collector.entries[1].name.c_str());
    TEST_ASSERT_EQUAL(oc::interface::FileType::DIRECTORY, collector.entries[1].type);

    EntryCollector limited;
    auto limitedResult = fs.list("/midi-studio", collectOneEntry, &limited);
    TEST_ASSERT_TRUE(limitedResult.isOk());
    TEST_ASSERT_EQUAL(1u, limited.entries.size());

    std::filesystem::remove_all(root);
}

void test_rename_refuses_existing_target_and_renames() {
    const auto root = makeRoot("rename");
    oc::impl::HostFileSystem fs(root.string().c_str());
    TEST_ASSERT_TRUE(fs.init().isOk());
    TEST_ASSERT_TRUE(fs.createDirectory("/midi-studio/tmp").isOk());

    const std::array<uint8_t, 1> left{{0xA1}};
    const std::array<uint8_t, 1> right{{0xB2}};
    writeFile(fs, "/midi-studio/tmp/source.bin", left.data(), left.size());
    writeFile(fs, "/midi-studio/tmp/target.bin", right.data(), right.size());

    auto overwrite = fs.rename("/midi-studio/tmp/source.bin", "/midi-studio/tmp/target.bin");
    TEST_ASSERT_TRUE(overwrite.isErr());
    TEST_ASSERT_EQUAL(oc::type::ErrorCode::INVALID_STATE, overwrite.error().code);

    TEST_ASSERT_TRUE(fs.remove("/midi-studio/tmp/target.bin").isOk());
    TEST_ASSERT_TRUE(fs.rename("/midi-studio/tmp/source.bin", "/midi-studio/tmp/target.bin").isOk());
    TEST_ASSERT_TRUE(fs.stat("/midi-studio/tmp/source.bin").isErr());
    TEST_ASSERT_TRUE(fs.stat("/midi-studio/tmp/target.bin").isOk());

    std::filesystem::remove_all(root);
}

void test_remove_requires_recursive_mode_for_non_empty_directory() {
    const auto root = makeRoot("remove");
    oc::impl::HostFileSystem fs(root.string().c_str());
    TEST_ASSERT_TRUE(fs.init().isOk());
    TEST_ASSERT_TRUE(fs.createDirectory("/midi-studio/tmp/nested").isOk());

    const std::array<uint8_t, 1> payload{{0x42}};
    writeFile(fs, "/midi-studio/tmp/nested/blob.bin", payload.data(), payload.size());

    auto nonRecursive = fs.remove("/midi-studio/tmp");
    TEST_ASSERT_TRUE(nonRecursive.isErr());
    TEST_ASSERT_TRUE(fs.stat("/midi-studio/tmp/nested/blob.bin").isOk());

    auto recursive = fs.remove("/midi-studio/tmp", oc::interface::RemoveMode::RECURSIVE);
    TEST_ASSERT_TRUE(recursive.isOk());
    TEST_ASSERT_TRUE(fs.stat("/midi-studio/tmp").isErr());

    std::filesystem::remove_all(root);
}

void test_paths_are_bounded_and_cannot_escape_root() {
    const auto root = makeRoot("paths");
    oc::impl::HostFileSystem fs(root.string().c_str());
    TEST_ASSERT_TRUE(fs.init().isOk());

    TEST_ASSERT_EQUAL(
        oc::type::ErrorCode::INVALID_ARGUMENT,
        fs.stat("../outside.bin").error().code
    );
    TEST_ASSERT_EQUAL(
        oc::type::ErrorCode::INVALID_ARGUMENT,
        fs.stat("C:/outside.bin").error().code
    );
    TEST_ASSERT_EQUAL(
        oc::type::ErrorCode::INVALID_ARGUMENT,
        fs.stat("/midi-studio/./tmp").error().code
    );

    std::string longName(oc::interface::FILESYSTEM_MAX_NAME_LENGTH + 1, 'a');
    const std::string longPath = "/midi-studio/" + longName;
    TEST_ASSERT_EQUAL(
        oc::type::ErrorCode::INVALID_ARGUMENT,
        fs.stat(longPath.c_str()).error().code
    );

    std::filesystem::remove_all(root);
}

void test_write_rejects_missing_parent_and_sparse_gap() {
    const auto root = makeRoot("write-errors");
    oc::impl::HostFileSystem fs(root.string().c_str());
    TEST_ASSERT_TRUE(fs.init().isOk());

    const std::array<uint8_t, 2> payload{{0x01, 0x02}};
    auto missingParent = fs.write("/midi-studio/tmp/blob.bin", 0, payload.data(), payload.size());
    TEST_ASSERT_TRUE(missingParent.isErr());
    TEST_ASSERT_EQUAL(oc::type::ErrorCode::RESOURCE_NOT_FOUND, missingParent.error().code);

    TEST_ASSERT_TRUE(fs.createDirectory("/midi-studio/tmp").isOk());
    auto gap = fs.write("/midi-studio/tmp/blob.bin", 2, payload.data(), payload.size());
    TEST_ASSERT_TRUE(gap.isErr());
    TEST_ASSERT_EQUAL(oc::type::ErrorCode::INVALID_ARGUMENT, gap.error().code);

    std::filesystem::remove_all(root);
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_init_creates_root_and_reports_available);
    RUN_TEST(test_write_read_stat_and_flush);
    RUN_TEST(test_list_streams_directory_entries);
    RUN_TEST(test_rename_refuses_existing_target_and_renames);
    RUN_TEST(test_remove_requires_recursive_mode_for_non_empty_directory);
    RUN_TEST(test_paths_are_bounded_and_cannot_escape_root);
    RUN_TEST(test_write_rejects_missing_parent_and_sparse_gap);
    return UNITY_END();
}
