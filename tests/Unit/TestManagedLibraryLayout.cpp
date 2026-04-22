#include <catch2/catch_test_macros.hpp>

#include "Domain/StorageEncoding.hpp"
#include "Storage/ManagedLibraryLayout.hpp"

TEST_CASE("Managed library layout builds stable root directories", "[storage-planning]")
{
    const auto layout = Librova::StoragePlanning::CManagedLibraryLayout::Build("D:/Library");

    REQUIRE(layout.DatabaseDirectory == std::filesystem::path{"D:/Library/Database"});
    REQUIRE(layout.ObjectsDirectory == std::filesystem::path{"D:/Library/Objects"});
    REQUIRE(layout.TrashDirectory == std::filesystem::path{"D:/Library/Trash"});
    REQUIRE(layout.LogsDirectory == std::filesystem::path{"D:/Library/Logs"});
}

TEST_CASE("Managed library layout uses zero-padded stable book folders", "[storage-planning]")
{
    REQUIRE(Librova::StoragePlanning::CManagedLibraryLayout::GetBookFolderName({1}) == "0000000001");
    REQUIRE(Librova::StoragePlanning::CManagedLibraryLayout::GetBookFolderName({42}) == "0000000042");

    REQUIRE_THROWS_AS(
        Librova::StoragePlanning::CManagedLibraryLayout::GetBookFolderName({0}),
        std::invalid_argument);
}

TEST_CASE("Managed library layout builds managed book and cover paths", "[storage-planning]")
{
    REQUIRE(
        Librova::StoragePlanning::CManagedLibraryLayout::GetObjectShardDirectory(
            "D:/Library",
            {7}) ==
        std::filesystem::path{"D:/Library/Objects/e8/5e"});

    REQUIRE(
        Librova::StoragePlanning::CManagedLibraryLayout::GetManagedBookPath(
            "D:/Library",
            {7},
            Librova::Domain::EBookFormat::Epub) ==
        std::filesystem::path{"D:/Library/Objects/e8/5e/0000000007.book.epub"});

    REQUIRE(
        Librova::StoragePlanning::CManagedLibraryLayout::GetManagedBookPath(
            "D:/Library",
            {7},
            Librova::Domain::EBookFormat::Fb2,
            Librova::Domain::EStorageEncoding::Compressed) ==
        std::filesystem::path{"D:/Library/Objects/e8/5e/0000000007.book.fb2.gz"});

    REQUIRE(
        Librova::StoragePlanning::CManagedLibraryLayout::GetManagedBookPath(
            "D:/Library",
            {7},
            Librova::Domain::EBookFormat::Fb2,
            Librova::Domain::EStorageEncoding::Plain) ==
        std::filesystem::path{"D:/Library/Objects/e8/5e/0000000007.book.fb2"});

    REQUIRE(
        Librova::StoragePlanning::CManagedLibraryLayout::GetCoverPath("D:/Library", {7}, ".jpg") ==
        std::filesystem::path{"D:/Library/Objects/e8/5e/0000000007.cover.jpg"});

    REQUIRE(
        Librova::StoragePlanning::CManagedLibraryLayout::GetObjectShardDirectory(
            "D:/Library",
            {123456}) ==
        std::filesystem::path{"D:/Library/Objects/3a/35"});

    REQUIRE(
        Librova::StoragePlanning::CManagedLibraryLayout::GetManagedBookPath(
            "D:/Library",
            {123456},
            Librova::Domain::EBookFormat::Epub) ==
        std::filesystem::path{"D:/Library/Objects/3a/35/0000123456.book.epub"});
}

