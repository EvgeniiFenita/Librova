#include <catch2/catch_test_macros.hpp>

#include "Domain/BookFormat.hpp"

TEST_CASE("Book format converts to stable storage strings", "[domain][book-format]")
{
    REQUIRE(LibriFlow::Domain::ToString(LibriFlow::Domain::EBookFormat::Epub) == "epub");
    REQUIRE(LibriFlow::Domain::ToString(LibriFlow::Domain::EBookFormat::Fb2) == "fb2");
    REQUIRE(LibriFlow::Domain::GetManagedFileName(LibriFlow::Domain::EBookFormat::Epub) == "book.epub");
    REQUIRE(LibriFlow::Domain::GetManagedFileName(LibriFlow::Domain::EBookFormat::Fb2) == "book.fb2");
}

TEST_CASE("Book format parsing accepts canonical MVP values", "[domain][book-format]")
{
    REQUIRE(LibriFlow::Domain::TryParseBookFormat("epub") == LibriFlow::Domain::EBookFormat::Epub);
    REQUIRE(LibriFlow::Domain::TryParseBookFormat(".fb2") == LibriFlow::Domain::EBookFormat::Fb2);
    REQUIRE(LibriFlow::Domain::TryParseBookFormat("EPUB") == LibriFlow::Domain::EBookFormat::Epub);
    REQUIRE_FALSE(LibriFlow::Domain::TryParseBookFormat("zip").has_value());
}
