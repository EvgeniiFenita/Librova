#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include "Importing/ImportPerfTracker.hpp"
#include "Logging/Logging.hpp"

namespace {

std::string ReadTextFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

} // namespace

TEST_CASE("Import perf tracker periodic log sorts bottlenecks and includes writer queue depth", "[importing][perf]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-import-perf-tracker";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Logs");
    const auto logPath = sandbox / "Logs" / "host.log";

    Librova::Logging::CLogging::InitializeHostLogger(logPath);

    {
        Librova::Importing::CImportPerfTracker tracker;
        {
            auto timer = tracker.MeasureStage(Librova::Importing::CImportPerfTracker::EStage::Parse);
            (void)timer;
        }
        {
            auto timer = tracker.MeasureStage(Librova::Importing::CImportPerfTracker::EStage::ZipExtract);
            std::this_thread::sleep_for(std::chrono::milliseconds(60));
            (void)timer;
        }
        {
            auto timer = tracker.MeasureStage(Librova::Importing::CImportPerfTracker::EStage::ZipScan);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            (void)timer;
        }

        tracker.SetWriterQueueDepth(123);
        for (std::uint64_t i = 0; i < Librova::Importing::CImportPerfTracker::kLogEveryN; ++i)
        {
            tracker.OnBookProcessed(1, 0, 0);
        }
        tracker.LogSummary(std::chrono::seconds(3));
    }

    Librova::Logging::CLogging::Shutdown();

    const auto logText = ReadTextFile(logPath);
    REQUIRE(logText.find("writer_queue=123") != std::string::npos);
    REQUIRE(logText.find("zip_scan=") != std::string::npos);
    REQUIRE(logText.find("zip_extract=") != std::string::npos);
    REQUIRE(logText.find("[import-perf] SUMMARY") != std::string::npos);

    const auto bottleneckPos = logText.find("bottleneck:");
    REQUIRE(bottleneckPos != std::string::npos);
    const auto zipExtractPos = logText.find("zip_extract=", bottleneckPos);
    const auto zipScanPos = logText.find("zip_scan=", bottleneckPos);
    const auto parsePos = logText.find("parse=", bottleneckPos);
    REQUIRE(zipExtractPos != std::string::npos);
    REQUIRE(zipScanPos != std::string::npos);
    REQUIRE(parsePos != std::string::npos);
    REQUIRE(zipExtractPos < zipScanPos);
    REQUIRE(zipExtractPos < parsePos);

    std::filesystem::remove_all(sandbox);
}

TEST_CASE("Import perf tracker does not emit periodic log before thresholds are reached", "[importing][perf]")
{
    const auto sandbox = std::filesystem::temp_directory_path() / "librova-import-perf-tracker-threshold";
    std::filesystem::remove_all(sandbox);
    std::filesystem::create_directories(sandbox / "Logs");
    const auto logPath = sandbox / "Logs" / "host.log";

    Librova::Logging::CLogging::InitializeHostLogger(logPath);

    {
        Librova::Importing::CImportPerfTracker tracker;
        tracker.OnBookProcessed(1, 0, 0);
        tracker.LogSummary(std::chrono::seconds(1));
    }

    Librova::Logging::CLogging::Shutdown();

    const auto logText = ReadTextFile(logPath);
    REQUIRE(logText.find("[import-perf] books=") == std::string::npos);
    REQUIRE(logText.find("[import-perf] SUMMARY") != std::string::npos);

    std::filesystem::remove_all(sandbox);
}
