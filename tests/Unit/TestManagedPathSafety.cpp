#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>

#include "ManagedPaths/ManagedPathSafety.hpp"

namespace {

class CScopedDirectory final
{
public:
    explicit CScopedDirectory(std::filesystem::path path)
        : m_path(std::move(path))
    {
        std::error_code errorCode;
        std::filesystem::remove_all(m_path, errorCode);
        std::filesystem::create_directories(m_path);
    }

    ~CScopedDirectory()
    {
        std::error_code errorCode;
        std::filesystem::remove_all(m_path, errorCode);
    }

    [[nodiscard]] const std::filesystem::path& GetPath() const noexcept
    {
        return m_path;
    }

private:
    std::filesystem::path m_path;
};

bool TryCreateDirectorySymlink(const std::filesystem::path& target, const std::filesystem::path& linkPath)
{
    std::error_code errorCode;
    std::filesystem::create_directory_symlink(target, linkPath, errorCode);
    return !errorCode;
}

} // namespace

TEST_CASE("Managed path safety resolves managed file under library root", "[managed-paths]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-managed-paths-resolve");
    const auto libraryRoot = sandbox.GetPath() / "Library";
    const auto sourcePath = libraryRoot / "Books/0000000001/book.epub";

    std::filesystem::create_directories(sourcePath.parent_path());
    std::ofstream(sourcePath, std::ios::binary) << "epub";

    const auto resolvedPath = Librova::ManagedPaths::ResolveExistingPathWithinRoot(
        libraryRoot,
        "Books/0000000001/book.epub",
        "missing",
        "unsafe",
        "canonicalize");

    REQUIRE(resolvedPath == std::filesystem::weakly_canonical(sourcePath));
}

TEST_CASE("Managed path safety rejects symlinked path escaping library root", "[managed-paths]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "librova-managed-paths-symlink");
    const auto libraryRoot = sandbox.GetPath() / "Library";
    const auto outsideRoot = sandbox.GetPath() / "Outside";
    const auto linkPath = libraryRoot / "Books/0000000002";

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
            "Books/0000000002/book.epub",
            "missing",
            "unsafe",
            "canonicalize"),
        Catch::Matchers::ContainsSubstring("unsafe"));
}
