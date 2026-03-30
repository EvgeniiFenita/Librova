#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "ConverterConfiguration/ConverterConfiguration.hpp"

namespace Librova::CoreHost {

struct SHostOptions
{
    std::filesystem::path PipePath;
    std::filesystem::path LibraryRoot;
    std::size_t MaxSessions = 0;
    Librova::ConverterConfiguration::SConverterConfiguration ConverterConfiguration;
};

class CHostOptions final
{
public:
    [[nodiscard]] static SHostOptions Parse(const std::vector<std::string>& arguments);
};

} // namespace Librova::CoreHost
