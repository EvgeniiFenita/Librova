#include "Logging/Logging.hpp"

#include <mutex>
#include <stdexcept>
#include <vector>

#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "Unicode/UnicodeConversion.hpp"

namespace Librova::Logging {
namespace {

std::mutex LoggerMutex;
std::shared_ptr<spdlog::logger> LoggerInstance;
constexpr std::string_view HostLoggerName = "Librova.Core.Host";

[[nodiscard]] spdlog::filename_t ToSpdlogFilename(const std::filesystem::path& path)
{
#ifdef _WIN32
    return Librova::Unicode::Utf8ToWide(Librova::Unicode::PathToUtf8(path));
#else
    return Librova::Unicode::PathToUtf8(path);
#endif
}

} // namespace

void CLogging::InitializeHostLogger(const std::filesystem::path& logFilePath)
{
    std::scoped_lock lock(LoggerMutex);

    std::filesystem::create_directories(logFilePath.parent_path());

    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(ToSpdlogFilename(logFilePath), true);
    auto stderrSink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();

    fileSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
    stderrSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

    std::vector<spdlog::sink_ptr> sinks{
        std::move(fileSink),
        std::move(stderrSink)
    };

    LoggerInstance = std::make_shared<spdlog::logger>(
        std::string{HostLoggerName},
        sinks.begin(),
        sinks.end());

    LoggerInstance->set_level(spdlog::level::info);
    LoggerInstance->flush_on(spdlog::level::info);

    spdlog::set_default_logger(LoggerInstance);
}

void CLogging::Shutdown() noexcept
{
    std::scoped_lock lock(LoggerMutex);
    LoggerInstance.reset();
    spdlog::shutdown();
}

bool CLogging::IsInitialized() noexcept
{
    std::scoped_lock lock(LoggerMutex);
    return static_cast<bool>(LoggerInstance);
}

std::shared_ptr<spdlog::logger> CLogging::GetLogger()
{
    std::scoped_lock lock(LoggerMutex);
    if (!LoggerInstance)
    {
        throw std::runtime_error("Librova logger is not initialized.");
    }

    return LoggerInstance;
}

} // namespace Librova::Logging
