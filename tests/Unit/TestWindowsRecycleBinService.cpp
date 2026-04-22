#include <catch2/catch_test_macros.hpp>
#include "TestWorkspace.hpp"
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <stdexcept>
#include <vector>

#include "Storage/WindowsRecycleBinService.hpp"

TEST_CASE("Windows recycle bin service rejects empty path list", "[recycle-bin]")
{
    Librova::RecycleBin::CWindowsRecycleBinService service;

    REQUIRE_THROWS_AS(service.MoveToRecycleBin({}), std::invalid_argument);
}

TEST_CASE("Windows recycle bin service reports failure for a missing path", "[recycle-bin]")
{
    Librova::RecycleBin::CWindowsRecycleBinService service;
    const auto missingPath = MakeUniqueTestPath(L"librova-recycle-bin-missing") / "missing.fb2";
    std::filesystem::remove_all(missingPath.parent_path());

    try
    {
        service.MoveToRecycleBin({missingPath});
        FAIL("Expected the Recycle Bin call to fail for a missing path.");
    }
    catch (const std::runtime_error& error)
    {
        const std::string message = error.what();
        REQUIRE(message.find("SHFileOperationW failed") != std::string::npos);
        REQUIRE(message.find("missing.fb2") != std::string::npos);
    }
}

TEST_CASE("Windows recycle bin service preserves Cyrillic paths when surfacing missing-file errors", "[recycle-bin]")
{
    Librova::RecycleBin::CWindowsRecycleBinService service;
    const auto missingPath = MakeUniqueTestPath(L"librova-\u043a\u043e\u0440\u0437\u0438\u043d\u0430") / u8"\u0442\u0435\u0441\u0442.fb2";
    std::filesystem::remove_all(missingPath.parent_path());

    try
    {
        service.MoveToRecycleBin({missingPath});
        FAIL("Expected the Recycle Bin call to fail for a missing path.");
    }
    catch (const std::runtime_error& error)
    {
        const std::string message = error.what();
        REQUIRE(message.find("SHFileOperationW failed") != std::string::npos);
        REQUIRE(message.find("librova-корзина") != std::string::npos);
        REQUIRE(message.find("тест.fb2") != std::string::npos);
    }
}
