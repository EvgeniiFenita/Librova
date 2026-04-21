#include <catch2/catch_test_macros.hpp>
#include "TestWorkspace.hpp"
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>

#include "Storage/ManagedTrashService.hpp"

namespace {

bool TryCreateDirectorySymlink(const std::filesystem::path& target, const std::filesystem::path& linkPath)
{
    std::error_code errorCode;
    std::filesystem::create_directory_symlink(target, linkPath, errorCode);
    return !errorCode;
}

} // namespace

TEST_CASE("Managed trash service moves managed file under library trash while preserving relative path", "[managed-trash]")
{
    const auto sandbox = MakeUniqueTestPath(L"librova-managed-trash");
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Objects/5a/68");

    const auto sourcePath = sandbox / "Library/Objects/5a/68/0000000001.book.epub";
    std::ofstream(sourcePath, std::ios::binary) << "epub";

    Librova::ManagedTrash::CManagedTrashService service(sandbox / "Library");
    const auto trashedPath = service.MoveToTrash(sourcePath);

    REQUIRE_FALSE(std::filesystem::exists(sourcePath));
    REQUIRE(std::filesystem::exists(trashedPath));
    REQUIRE(trashedPath == sandbox / "Library/Trash/Objects/5a/68/0000000001.book.epub");

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Managed trash service accepts a non-canonical library root", "[managed-trash]")
{
    const auto sandbox = MakeUniqueTestPath(L"librova-managed-trash-noncanonical-root");
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Parent/Library/Objects/5a/68");

    const auto libraryRoot = sandbox / "Parent" / ".." / "Parent" / "Library";
    const auto sourcePath = sandbox / "Parent/Library/Objects/5a/68/0000000001.book.epub";
    std::ofstream(sourcePath, std::ios::binary) << "epub";

    Librova::ManagedTrash::CManagedTrashService service(libraryRoot);
    const auto trashedPath = service.MoveToTrash(sourcePath);

    REQUIRE_FALSE(std::filesystem::exists(sourcePath));
    REQUIRE(std::filesystem::exists(trashedPath));
    REQUIRE(trashedPath == sandbox / "Parent/Library/Trash/Objects/5a/68/0000000001.book.epub");

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Managed trash service restores managed file from trash", "[managed-trash]")
{
    const auto sandbox = MakeUniqueTestPath(L"librova-managed-trash-restore");
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Trash/Objects/5a/68");

    const auto trashedPath = sandbox / "Library/Trash/Objects/5a/68/0000000001.book.epub";
    const auto destinationPath = sandbox / "Library/Objects/5a/68/0000000001.book.epub";
    std::ofstream(trashedPath, std::ios::binary) << "epub";

    Librova::ManagedTrash::CManagedTrashService service(sandbox / "Library");
    service.RestoreFromTrash(trashedPath, destinationPath);

    REQUIRE_FALSE(std::filesystem::exists(trashedPath));
    REQUIRE(std::filesystem::exists(destinationPath));

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Managed trash service rejects paths outside library root", "[managed-trash]")
{
    const auto sandbox = MakeUniqueTestPath(L"librova-managed-trash-unsafe");
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library");

    Librova::ManagedTrash::CManagedTrashService service(sandbox / "Library");

    REQUIRE_THROWS(service.MoveToTrash(sandbox / "Outside" / "book.epub"));

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Managed trash service does not remove top-level library directories after last file is trashed", "[managed-trash]")
{
    const auto sandbox = MakeUniqueTestPath(L"librova-managed-trash-toplevel");
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Objects/5a/68");

    // Single cover object — the only entry under the shard.
    const auto coverPath = sandbox / "Library/Objects/5a/68/0000000001.cover.jpg";
    std::ofstream(coverPath, std::ios::binary) << "cover";

    Librova::ManagedTrash::CManagedTrashService service(sandbox / "Library");
    const auto trashedPath = service.MoveToTrash(coverPath);

    // File must be in trash
    REQUIRE(std::filesystem::exists(trashedPath));
    REQUIRE_FALSE(std::filesystem::exists(coverPath));

    // Objects/ must still exist — it is a required top-level layout directory
    REQUIRE(std::filesystem::exists(sandbox / "Library/Objects"));

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Managed trash service removes per-book subdirectory when it becomes empty after trash", "[managed-trash]")
{
    const auto sandbox = MakeUniqueTestPath(L"librova-managed-trash-subdir");
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Objects/5a/68");

    const auto bookPath = sandbox / "Library/Objects/5a/68/0000000001.book.fb2";
    std::ofstream(bookPath, std::ios::binary) << "fb2";

    Librova::ManagedTrash::CManagedTrashService service(sandbox / "Library");
    (void)service.MoveToTrash(bookPath);

    // The shard directory becomes empty and is removed.
    REQUIRE_FALSE(std::filesystem::exists(sandbox / "Library/Objects/5a/68"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox / "Library/Objects/5a"));
    // But Objects/ itself must remain.
    REQUIRE(std::filesystem::exists(sandbox / "Library/Objects"));

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Managed trash service cleans up empty trash subdirectory after restore but preserves parent dirs", "[managed-trash]")
{
    const auto sandbox = MakeUniqueTestPath(L"librova-managed-trash-restore-cleanup");
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Objects/5a/68");
    std::filesystem::create_directories(sandbox / "Library/Trash/Objects/5a/68");

    const auto trashedPath = sandbox / "Library/Trash/Objects/5a/68/0000000001.book.epub";
    const auto destinationPath = sandbox / "Library/Objects/5a/68/0000000001.book.epub";
    std::ofstream(trashedPath, std::ios::binary) << "epub";

    Librova::ManagedTrash::CManagedTrashService service(sandbox / "Library");
    service.RestoreFromTrash(trashedPath, destinationPath);

    REQUIRE(std::filesystem::exists(destinationPath));

    // Empty shard trash directory must be removed.
    REQUIRE_FALSE(std::filesystem::exists(sandbox / "Library/Trash/Objects/5a/68"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox / "Library/Trash/Objects/5a"));

    // Trash/Objects/ and Trash/ itself are preserved.
    REQUIRE(std::filesystem::exists(sandbox / "Library/Trash/Objects"));
    REQUIRE(std::filesystem::exists(sandbox / "Library/Trash"));

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Managed trash service rejects symlinked managed path escaping library root", "[managed-trash]")
{
    const auto sandbox = MakeUniqueTestPath(L"librova-managed-trash-symlink");
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Objects/c7");
    std::filesystem::create_directories(sandbox / "Outside");
    std::ofstream(sandbox / "Outside/book.epub", std::ios::binary) << "epub";

    if (!TryCreateDirectorySymlink(sandbox / "Outside", sandbox / "Library/Objects/c7/66"))
    {
        std::filesystem::remove_all(sandbox);
        SKIP("Directory symlinks are not available in this environment.");
    }

    Librova::ManagedTrash::CManagedTrashService service(sandbox / "Library");
    REQUIRE_THROWS_WITH(
        service.MoveToTrash(sandbox / "Library/Objects/c7/66/0000000002.book.epub"),
        Catch::Matchers::ContainsSubstring("unsafe"));

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Managed trash service retries with a suffixed trash path when a collision appears during rename", "[managed-trash]")
{
    const auto sandbox = MakeUniqueTestPath(L"librova-managed-trash-race");
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Objects/34/65");

    const auto sourcePath = sandbox / "Library/Objects/34/65/0000000003.book.epub";
    const auto initialTrashPath = sandbox / "Library/Trash/Objects/34/65/0000000003.book.epub";
    std::ofstream(sourcePath, std::ios::binary) << "epub";

    auto firstAttempt = true;
    Librova::ManagedTrash::CManagedTrashService service(
        sandbox / "Library",
        [&firstAttempt, &initialTrashPath](const std::filesystem::path& source, const std::filesystem::path& destination) {
            if (firstAttempt)
            {
                firstAttempt = false;
                std::filesystem::create_directories(initialTrashPath.parent_path());
                std::ofstream(initialTrashPath, std::ios::binary) << "collision";
                return std::make_error_code(std::errc::file_exists);
            }

            std::error_code errorCode;
            std::filesystem::rename(source, destination, errorCode);
            return errorCode;
        });

    const auto trashedPath = service.MoveToTrash(sourcePath);

    REQUIRE(trashedPath == sandbox / "Library/Trash/Objects/34/65/0000000003.book.trashed-1.epub");
    REQUIRE(std::filesystem::exists(initialTrashPath));
    REQUIRE(std::filesystem::exists(trashedPath));
    REQUIRE_FALSE(std::filesystem::exists(sourcePath));

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Managed trash service removes shard residue when only Thumbs.db remains after trash", "[managed-trash]")
{
    const auto sandbox = MakeUniqueTestPath(L"librova-managed-trash-thumbs");
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Objects/5a/68");

    const auto bookPath = sandbox / "Library/Objects/5a/68/0000000001.book.fb2";
    const auto thumbsPath = sandbox / "Library/Objects/5a/68/Thumbs.db";
    std::ofstream(bookPath, std::ios::binary) << "fb2";
    std::ofstream(thumbsPath, std::ios::binary) << "thumbs";

    Librova::ManagedTrash::CManagedTrashService service(sandbox / "Library");
    (void)service.MoveToTrash(bookPath);

    REQUIRE_FALSE(std::filesystem::exists(sandbox / "Library/Objects/5a/68"));
    REQUIRE_FALSE(std::filesystem::exists(sandbox / "Library/Objects/5a"));
    REQUIRE(std::filesystem::exists(sandbox / "Library/Objects"));

    std::filesystem::remove_all(sandbox);
}
