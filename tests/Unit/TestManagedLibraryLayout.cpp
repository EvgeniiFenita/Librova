#include <catch2/catch_test_macros.hpp>

#include "StoragePlanning/ManagedLibraryLayout.hpp"

TEST_CASE("Managed library layout builds stable root directories", "[storage-planning]")
{
    const auto layout = LibriFlow::StoragePlanning::CManagedLibraryLayout::Build("D:/Library");

    REQUIRE(layout.DatabaseDirectory == std::filesystem::path{"D:/Library/Database"});
    REQUIRE(layout.BooksDirectory == std::filesystem::path{"D:/Library/Books"});
    REQUIRE(layout.CoversDirectory == std::filesystem::path{"D:/Library/Covers"});
    REQUIRE(layout.TempDirectory == std::filesystem::path{"D:/Library/Temp"});
    REQUIRE(layout.LogsDirectory == std::filesystem::path{"D:/Library/Logs"});
}

TEST_CASE("Managed library layout uses zero-padded stable book folders", "[storage-planning]")
{
    REQUIRE(LibriFlow::StoragePlanning::CManagedLibraryLayout::GetBookFolderName({1}) == "0000000001");
    REQUIRE(LibriFlow::StoragePlanning::CManagedLibraryLayout::GetBookFolderName({42}) == "0000000042");

    REQUIRE_THROWS_AS(
        LibriFlow::StoragePlanning::CManagedLibraryLayout::GetBookFolderName({0}),
        std::invalid_argument);
}

TEST_CASE("Managed library layout builds managed book, cover, and staging paths", "[storage-planning]")
{
    REQUIRE(
        LibriFlow::StoragePlanning::CManagedLibraryLayout::GetManagedBookPath(
            "D:/Library",
            {7},
            LibriFlow::Domain::EBookFormat::Epub) ==
        std::filesystem::path{"D:/Library/Books/0000000007/book.epub"});

    REQUIRE(
        LibriFlow::StoragePlanning::CManagedLibraryLayout::GetCoverPath("D:/Library", {7}, ".jpg") ==
        std::filesystem::path{"D:/Library/Covers/0000000007.jpg"});

    REQUIRE(
        LibriFlow::StoragePlanning::CManagedLibraryLayout::GetStagingDirectory("D:/Library", {7}) ==
        std::filesystem::path{"D:/Library/Temp/0000000007"});
}
