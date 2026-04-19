#include <catch2/catch_test_macros.hpp>
#include "TestWorkspace.hpp"
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>

#include "Storage/ManagedPathSafety.hpp"

namespace {


bool TryCreateDirectorySymlink(const std::filesystem::path& target, const std::filesystem::path& linkPath)
{
    std::error_code errorCode;
    std::filesystem::create_directory_symlink(target, linkPath, errorCode);
    return !errorCode;
}

} // namespace

TEST_CASE("Managed path safety resolves managed file under library root", "[managed-paths]")
{
    CTestWorkspace sandbox(L"librova-managed-paths-resolve");
    const auto libraryRoot = sandbox.GetPath() / "Library";
    const auto sourcePath = libraryRoot / "Objects/5a/68/0000000001.book.epub";

    std::filesystem::create_directories(sourcePath.parent_path());
    std::ofstream(sourcePath, std::ios::binary) << "epub";

    const auto resolvedPath = Librova::ManagedPaths::ResolveExistingPathWithinRoot(
        libraryRoot,
        "Objects/5a/68/0000000001.book.epub",
        "missing",
        "unsafe",
        "canonicalize");

    REQUIRE(resolvedPath == std::filesystem::weakly_canonical(sourcePath));
}

TEST_CASE("Managed path safety returns empty for a missing managed file under library root", "[managed-paths]")
{
    CTestWorkspace sandbox(L"librova-managed-paths-missing");
    const auto libraryRoot = sandbox.GetPath() / "Library";

    std::filesystem::create_directories(libraryRoot / "Objects/34/65");

    const auto resolvedPath = Librova::ManagedPaths::TryResolvePathWithinRoot(
        libraryRoot,
        "Objects/34/65/0000000003.book.epub",
        "unsafe",
        "canonicalize");

    REQUIRE_FALSE(resolvedPath.has_value());
}

TEST_CASE("Managed path safety rejects symlinked path escaping library root", "[managed-paths]")
{
    CTestWorkspace sandbox(L"librova-managed-paths-symlink");
    const auto libraryRoot = sandbox.GetPath() / "Library";
    const auto outsideRoot = sandbox.GetPath() / "Outside";
    const auto linkPath = libraryRoot / "Objects/c7/66";

    std::filesystem::create_directories(outsideRoot);
    std::filesystem::create_directories(linkPath.parent_path());
    std::ofstream(outsideRoot / "book.epub", std::ios::binary) << "epub";

    if (!TryCreateDirectorySymlink(outsideRoot, linkPath))
    {
        SKIP("Directory symlinks are not available in this environment.");
    }

    REQUIRE_THROWS_WITH(
        Librova::ManagedPaths::ResolveExistingPathWithinRoot(
            libraryRoot,
            "Objects/c7/66/0000000002.book.epub",
            "missing",
            "unsafe",
            "canonicalize"),
        Catch::Matchers::ContainsSubstring("unsafe"));
}

TEST_CASE("Managed path safety removes a symlink under the library root without touching its target", "[managed-paths]")
{
    CTestWorkspace sandbox(L"librova-managed-paths-remove-symlink");
    const auto libraryRoot = sandbox.GetPath() / "Library";
    const auto outsideRoot = sandbox.GetPath() / "Outside";
    const auto linkPath = libraryRoot / "Temp" / "linked";

    std::filesystem::create_directories(outsideRoot);
    std::filesystem::create_directories(linkPath.parent_path());
    std::ofstream(outsideRoot / "outside.tmp", std::ios::binary) << "outside";

    if (!TryCreateDirectorySymlink(outsideRoot, linkPath))
    {
        SKIP("Directory symlinks are not available in this environment.");
    }

    Librova::ManagedPaths::RemovePathRecursivelyWithinRoot(
        libraryRoot,
        linkPath,
        "missing",
        "unsafe",
        "canonicalize");

    REQUIRE_FALSE(std::filesystem::exists(linkPath));
    REQUIRE(std::filesystem::exists(outsideRoot / "outside.tmp"));
}

