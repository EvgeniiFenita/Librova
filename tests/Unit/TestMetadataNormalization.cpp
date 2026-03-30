#include <catch2/catch_test_macros.hpp>

#include "Domain/MetadataNormalization.hpp"

TEST_CASE("Text normalization trims, lowercases, and collapses whitespace", "[domain][normalization]")
{
    REQUIRE(LibriFlow::Domain::NormalizeText("  Roadside   Picnic  ") == "roadside picnic");
    REQUIRE(LibriFlow::Domain::NormalizeText("\tArkady\r\nStrugatsky ") == "arkady strugatsky");
}

TEST_CASE("Text normalization treats cyrillic yo and ye as equivalent", "[domain][normalization]")
{
    REQUIRE(LibriFlow::Domain::NormalizeText("Ёжик") == LibriFlow::Domain::NormalizeText("ежик"));
    REQUIRE(LibriFlow::Domain::NormalizeText("ФЁДОР") == "федор");
    REQUIRE(LibriFlow::Domain::NormalizeText("Борис") == "борис");
}

TEST_CASE("Optional text normalization drops empty values", "[domain][normalization]")
{
    REQUIRE_FALSE(LibriFlow::Domain::NormalizeOptionalText(std::optional<std::string>{" \t "}).has_value());
    REQUIRE(LibriFlow::Domain::NormalizeOptionalText(std::optional<std::string>{"  Series One "}) == "series one");
}

TEST_CASE("ISBN normalization keeps only valid canonical forms", "[domain][normalization]")
{
    REQUIRE(LibriFlow::Domain::NormalizeIsbn(std::optional<std::string>{"978-5-17-118366-6"}) == "9785171183666");
    REQUIRE(LibriFlow::Domain::NormalizeIsbn(std::optional<std::string>{"5-93286-159-X"}) == "593286159X");
    REQUIRE_FALSE(LibriFlow::Domain::NormalizeIsbn(std::optional<std::string>{"invalid-isbn"}).has_value());
}

TEST_CASE("Duplicate key normalization is stable across case, spacing, yo, and author order", "[domain][normalization]")
{
    const LibriFlow::Domain::SBookMetadata firstMetadata{
        .TitleUtf8 = "  ФЁДОР  ",
        .AuthorsUtf8 = {" Борис Стругацкий ", "Аркадий  Стругацкий"}
    };

    const LibriFlow::Domain::SBookMetadata secondMetadata{
        .TitleUtf8 = "федор",
        .AuthorsUtf8 = {"аркадий стругацкий", "борис стругацкий"}
    };

    REQUIRE(LibriFlow::Domain::BuildDuplicateKey(firstMetadata) == LibriFlow::Domain::BuildDuplicateKey(secondMetadata));
}
