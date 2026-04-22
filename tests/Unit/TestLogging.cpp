#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include "Foundation/Logging.hpp"

#include "TestWorkspace.hpp"

namespace {


[[nodiscard]] std::string ReadAllText(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

} // namespace

TEST_CASE("Logging initializes host logger and writes records into file", "[logging]")
{
    CTestWorkspace sandbox(L"librova-logging-кириллица");
    const auto logFilePath = sandbox.GetPath() / "Logs" / "host.log";

    Librova::Logging::CLogging::InitializeHostLogger(logFilePath);
    Librova::Logging::Info("Hello {}.", "logger");
    Librova::Logging::Error("Something {}.", "happened");
    Librova::Logging::CLogging::Shutdown();

    REQUIRE(std::filesystem::exists(logFilePath));
    REQUIRE(std::filesystem::is_regular_file(logFilePath));

    const auto fileContents = ReadAllText(logFilePath);
    REQUIRE(fileContents.find("Hello logger.") != std::string::npos);
    REQUIRE(fileContents.find("Something happened.") != std::string::npos);
}

