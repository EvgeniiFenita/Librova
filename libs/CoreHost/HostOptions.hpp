#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "ConverterConfiguration/ConverterConfiguration.hpp"

namespace Librova::CoreHost {

enum class ELibraryOpenMode
{
    OpenExisting,
    CreateNew
};

struct SHostOptions
{
    std::filesystem::path PipePath;
    std::filesystem::path LibraryRoot;
    std::optional<std::filesystem::path> LogFilePath;
    std::optional<std::filesystem::path> ConverterWorkingDirectory;
    std::optional<std::filesystem::path> ManagedStorageStagingRoot;
    std::optional<std::string> ShutdownEventName;
    ELibraryOpenMode LibraryOpenMode = ELibraryOpenMode::OpenExisting;
    std::optional<std::uint32_t> ParentProcessId;
    std::size_t MaxSessions = 0;
    bool ShowHelp = false;
    bool ShowVersion = false;
    Librova::ConverterConfiguration::SConverterConfiguration ConverterConfiguration;
};

class CHostOptions final
{
public:
    [[nodiscard]] static SHostOptions Parse(const std::vector<std::string>& arguments);
};

} // namespace Librova::CoreHost
