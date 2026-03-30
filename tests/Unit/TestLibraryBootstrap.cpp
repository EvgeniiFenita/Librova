#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "CoreHost/LibraryBootstrap.hpp"

namespace {

class CScopedDirectory final
{
public:
    explicit CScopedDirectory(std::filesystem::path path)
        : m_path(std::move(path))
    {
        std::filesystem::remove_all(m_path);
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

void WriteTextFile(const std::filesystem::path& path, const std::string& text)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << text;
}

} // namespace

TEST_CASE("Library bootstrap creates library layout and clears stale temp state", "[core-host]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "libriflow-library-bootstrap");
    const auto libraryRoot = sandbox.GetPath() / "Library";
    WriteTextFile(libraryRoot / "Temp" / "stale" / "file.tmp", "stale");

    LibriFlow::CoreHost::CLibraryBootstrap::PrepareLibraryRoot(libraryRoot);

    REQUIRE(std::filesystem::exists(libraryRoot / "Database"));
    REQUIRE(std::filesystem::exists(libraryRoot / "Books"));
    REQUIRE(std::filesystem::exists(libraryRoot / "Covers"));
    REQUIRE(std::filesystem::exists(libraryRoot / "Temp"));
    REQUIRE(std::filesystem::exists(libraryRoot / "Logs"));
    REQUIRE_FALSE(std::filesystem::exists(libraryRoot / "Temp" / "stale" / "file.tmp"));
}
