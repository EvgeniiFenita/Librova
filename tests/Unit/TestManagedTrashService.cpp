#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>

#include "ManagedTrash/ManagedTrashService.hpp"

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
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-managed-trash";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Books/0000000001");

    const auto sourcePath = sandbox / "Library/Books/0000000001/book.epub";
    std::ofstream(sourcePath, std::ios::binary) << "epub";

    Librova::ManagedTrash::CManagedTrashService service(sandbox / "Library");
    const auto trashedPath = service.MoveToTrash(sourcePath);

    REQUIRE_FALSE(std::filesystem::exists(sourcePath));
    REQUIRE(std::filesystem::exists(trashedPath));
    REQUIRE(trashedPath == sandbox / "Library/Trash/Books/0000000001/book.epub");

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Managed trash service restores managed file from trash", "[managed-trash]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-managed-trash-restore";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Trash/Books/0000000001");

    const auto trashedPath = sandbox / "Library/Trash/Books/0000000001/book.epub";
    const auto destinationPath = sandbox / "Library/Books/0000000001/book.epub";
    std::ofstream(trashedPath, std::ios::binary) << "epub";

    Librova::ManagedTrash::CManagedTrashService service(sandbox / "Library");
    service.RestoreFromTrash(trashedPath, destinationPath);

    REQUIRE_FALSE(std::filesystem::exists(trashedPath));
    REQUIRE(std::filesystem::exists(destinationPath));

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Managed trash service rejects paths outside library root", "[managed-trash]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-managed-trash-unsafe";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library");

    Librova::ManagedTrash::CManagedTrashService service(sandbox / "Library");

    REQUIRE_THROWS(service.MoveToTrash(sandbox / "Outside" / "book.epub"));

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Managed trash service does not remove top-level library directories after last file is trashed", "[managed-trash]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-managed-trash-toplevel";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Covers");

    // Single cover file — the only entry in Covers/
    const auto coverPath = sandbox / "Library/Covers/0000000001.jpg";
    std::ofstream(coverPath, std::ios::binary) << "cover";

    Librova::ManagedTrash::CManagedTrashService service(sandbox / "Library");
    const auto trashedPath = service.MoveToTrash(coverPath);

    // File must be in trash
    REQUIRE(std::filesystem::exists(trashedPath));
    REQUIRE_FALSE(std::filesystem::exists(coverPath));

    // Covers/ must still exist — it is a required top-level layout directory
    REQUIRE(std::filesystem::exists(sandbox / "Library/Covers"));

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Managed trash service removes per-book subdirectory when it becomes empty after trash", "[managed-trash]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-managed-trash-subdir";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Books/0000000001");

    const auto bookPath = sandbox / "Library/Books/0000000001/book.fb2";
    std::ofstream(bookPath, std::ios::binary) << "fb2";

    Librova::ManagedTrash::CManagedTrashService service(sandbox / "Library");
    (void)service.MoveToTrash(bookPath);

    // The per-book subdirectory must be removed (it is two levels deep, not a top-level dir)
    REQUIRE_FALSE(std::filesystem::exists(sandbox / "Library/Books/0000000001"));
    // But Books/ itself must remain
    REQUIRE(std::filesystem::exists(sandbox / "Library/Books"));

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Managed trash service rejects symlinked managed path escaping library root", "[managed-trash]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-managed-trash-symlink";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Library/Books");
    std::filesystem::create_directories(sandbox / "Outside");
    std::ofstream(sandbox / "Outside/book.epub", std::ios::binary) << "epub";

    if (!TryCreateDirectorySymlink(sandbox / "Outside", sandbox / "Library/Books/0000000002"))
    {
        std::filesystem::remove_all(sandbox);
        SKIP("Directory symlinks are not available in this environment.");
    }

    Librova::ManagedTrash::CManagedTrashService service(sandbox / "Library");
    REQUIRE_THROWS_WITH(
        service.MoveToTrash(sandbox / "Library/Books/0000000002/book.epub"),
        Catch::Matchers::ContainsSubstring("unsafe"));

    std::filesystem::remove_all(sandbox);
}
