#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include "Logging/Logging.hpp"

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

[[nodiscard]] std::string ReadAllText(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

} // namespace

TEST_CASE("Logging initializes host logger and writes records into file", "[logging]")
{
    CScopedDirectory sandbox(std::filesystem::temp_directory_path() / "libriflow-logging");
    const auto logFilePath = sandbox.GetPath() / "Logs" / "host.log";

    LibriFlow::Logging::CLogging::InitializeHostLogger(logFilePath);
    LibriFlow::Logging::Info("Hello {}.", "logger");
    LibriFlow::Logging::Error("Something {}.", "happened");
    LibriFlow::Logging::CLogging::Shutdown();

    REQUIRE(std::filesystem::exists(logFilePath));

    const auto fileContents = ReadAllText(logFilePath);
    REQUIRE(fileContents.find("Hello logger.") != std::string::npos);
    REQUIRE(fileContents.find("Something happened.") != std::string::npos);
}
