#include <catch2/catch_test_macros.hpp>

#include "Domain/MetadataNormalization.hpp"

TEST_CASE("Text normalization trims, lowercases, and collapses whitespace", "[domain][normalization]")
{
    REQUIRE(Librova::Domain::NormalizeText("  Roadside   Picnic  ") == "roadside picnic");
    REQUIRE(Librova::Domain::NormalizeText("\tArkady\r\nStrugatsky ") == "arkady strugatsky");
}

TEST_CASE("Text normalization treats cyrillic yo and ye as equivalent", "[domain][normalization]")
{
    REQUIRE(Librova::Domain::NormalizeText("Ёжик") == Librova::Domain::NormalizeText("ежик"));
    REQUIRE(Librova::Domain::NormalizeText("ФЁДОР") == "федор");
    REQUIRE(Librova::Domain::NormalizeText("Борис") == "борис");
}

TEST_CASE("Optional text normalization drops empty values", "[domain][normalization]")
{
    REQUIRE_FALSE(Librova::Domain::NormalizeOptionalText(std::optional<std::string>{" \t "}).has_value());
    REQUIRE(Librova::Domain::NormalizeOptionalText(std::optional<std::string>{"  Series One "}) == "series one");
}

TEST_CASE("ISBN normalization keeps only valid canonical forms", "[domain][normalization]")
{
    REQUIRE(Librova::Domain::NormalizeIsbn(std::optional<std::string>{"978-5-17-118366-6"}) == "9785171183666");
    REQUIRE(Librova::Domain::NormalizeIsbn(std::optional<std::string>{"5-93286-159-X"}) == "593286159X");
    REQUIRE_FALSE(Librova::Domain::NormalizeIsbn(std::optional<std::string>{"invalid-isbn"}).has_value());
}

TEST_CASE("Duplicate key normalization is stable across case, spacing, yo, and author order", "[domain][normalization]")
{
    const Librova::Domain::SBookMetadata firstMetadata{
        .TitleUtf8 = "  ФЁДОР  ",
        .AuthorsUtf8 = {" Борис Стругацкий ", "Аркадий  Стругацкий"}
    };

    const Librova::Domain::SBookMetadata secondMetadata{
        .TitleUtf8 = "федор",
        .AuthorsUtf8 = {"аркадий стругацкий", "борис стругацкий"}
    };

    REQUIRE(Librova::Domain::BuildDuplicateKey(firstMetadata) == Librova::Domain::BuildDuplicateKey(secondMetadata));
}
