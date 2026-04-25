#pragma once

#include <filesystem>

namespace LibrovaQt {

// Bridges the spdlog-based Librova logger with Qt's message system.
// Initialize once at startup (before any Qt logging calls).
// Shutdown once at clean exit.
class QtLogging
{
public:
    static void Initialize(const std::filesystem::path& logFilePath);
    static void Reinitialize(const std::filesystem::path& logFilePath);
    [[nodiscard]] static std::filesystem::path CurrentLogFilePath();
    static void Shutdown() noexcept;
};

} // namespace LibrovaQt
