#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include "ConverterCommand/ConverterCommandBuilder.hpp"

namespace Librova::ConverterConfiguration {

enum class EConverterConfigurationMode
{
    Disabled,
    BuiltInFb2Cng,
    CustomCommand
};

struct SFb2CngConverterSettings
{
    std::filesystem::path ExecutablePath;
    std::optional<std::filesystem::path> ConfigPath;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return !ExecutablePath.empty();
    }
};

struct SCustomConverterSettings
{
    std::filesystem::path ExecutablePath;
    std::vector<std::string> ArgumentTemplate;
    Librova::ConverterCommand::EConverterOutputMode OutputMode =
        Librova::ConverterCommand::EConverterOutputMode::ExactDestinationPath;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return !ExecutablePath.empty() && !ArgumentTemplate.empty();
    }
};

struct SConverterConfiguration
{
    EConverterConfigurationMode Mode = EConverterConfigurationMode::Disabled;
    SFb2CngConverterSettings Fb2Cng;
    SCustomConverterSettings Custom;

    [[nodiscard]] bool IsEnabled() const noexcept
    {
        return Mode != EConverterConfigurationMode::Disabled;
    }

    [[nodiscard]] bool IsValid() const noexcept;
};

[[nodiscard]] std::optional<Librova::ConverterCommand::SConverterCommandProfile> TryBuildCommandProfile(
    const SConverterConfiguration& configuration);

} // namespace Librova::ConverterConfiguration
