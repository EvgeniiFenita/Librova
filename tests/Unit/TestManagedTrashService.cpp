#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "ManagedTrash/ManagedTrashService.hpp"

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
