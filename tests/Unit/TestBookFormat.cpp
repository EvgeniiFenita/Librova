#include <catch2/catch_test_macros.hpp>

#include "Domain/BookFormat.hpp"

TEST_CASE("Book format converts to stable storage strings", "[domain][book-format]")
{
    REQUIRE(Librova::Domain::ToString(Librova::Domain::EBookFormat::Epub) == "epub");
    REQUIRE(Librova::Domain::ToString(Librova::Domain::EBookFormat::Fb2) == "fb2");
    REQUIRE(Librova::Domain::GetManagedFileName(Librova::Domain::EBookFormat::Epub) == "book.epub");
    REQUIRE(Librova::Domain::GetManagedFileName(Librova::Domain::EBookFormat::Fb2) == "book.fb2");
}

TEST_CASE("Book format parsing accepts canonical MVP values", "[domain][book-format]")
{
    REQUIRE(Librova::Domain::TryParseBookFormat("epub") == Librova::Domain::EBookFormat::Epub);
    REQUIRE(Librova::Domain::TryParseBookFormat(".fb2") == Librova::Domain::EBookFormat::Fb2);
    REQUIRE(Librova::Domain::TryParseBookFormat("EPUB") == Librova::Domain::EBookFormat::Epub);
    REQUIRE_FALSE(Librova::Domain::TryParseBookFormat("zip").has_value());
}
