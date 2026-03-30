#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include <spdlog/logger.h>
#include <spdlog/spdlog.h>

namespace LibriFlow::Logging {

class CLogging final
{
public:
    static void InitializeHostLogger(const std::filesystem::path& logFilePath);
    static void Shutdown() noexcept;

    [[nodiscard]] static bool IsInitialized() noexcept;
    [[nodiscard]] static std::shared_ptr<spdlog::logger> GetLogger();
};

template <typename... TArgs>
void Info(spdlog::format_string_t<TArgs...> format, TArgs&&... args)
{
    CLogging::GetLogger()->info(format, std::forward<TArgs>(args)...);
}

template <typename... TArgs>
void Warn(spdlog::format_string_t<TArgs...> format, TArgs&&... args)
{
    CLogging::GetLogger()->warn(format, std::forward<TArgs>(args)...);
}

template <typename... TArgs>
void Error(spdlog::format_string_t<TArgs...> format, TArgs&&... args)
{
    CLogging::GetLogger()->error(format, std::forward<TArgs>(args)...);
}

template <typename... TArgs>
void Critical(spdlog::format_string_t<TArgs...> format, TArgs&&... args)
{
    CLogging::GetLogger()->critical(format, std::forward<TArgs>(args)...);
}

} // namespace LibriFlow::Logging
