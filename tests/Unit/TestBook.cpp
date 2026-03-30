#include <catch2/catch_test_macros.hpp>

#include "Domain/Book.hpp"

TEST_CASE("Book metadata reports title and authors presence", "[domain][book]")
{
    Librova::Domain::SBookMetadata metadata;

    REQUIRE_FALSE(metadata.HasTitle());
    REQUIRE_FALSE(metadata.HasAuthors());

    metadata.TitleUtf8 = "Roadside Picnic";
    metadata.AuthorsUtf8 = {"Arkady Strugatsky", "Boris Strugatsky"};

    REQUIRE(metadata.HasTitle());
    REQUIRE(metadata.HasAuthors());
}

TEST_CASE("Book file info reports managed path and hash presence", "[domain][book]")
{
    Librova::Domain::SBookFileInfo fileInfo;

    REQUIRE_FALSE(fileInfo.HasManagedPath());
    REQUIRE_FALSE(fileInfo.HasHash());

    fileInfo.ManagedPath = "Books/0000000001/book.epub";
    fileInfo.Sha256Hex = "abc123";

    REQUIRE(fileInfo.HasManagedPath());
    REQUIRE(fileInfo.HasHash());
}

TEST_CASE("Book aggregate exposes identity and cover state", "[domain][book]")
{
    Librova::Domain::SBook book;

    REQUIRE_FALSE(book.HasIdentity());
    REQUIRE_FALSE(book.HasCover());

    book.Id = {7};
    book.CoverPath = "Covers/0000000007.jpg";

    REQUIRE(book.HasIdentity());
    REQUIRE(book.HasCover());
}
