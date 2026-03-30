#include "ConverterConfiguration/ConverterConfiguration.hpp"

namespace LibriFlow::ConverterConfiguration {

bool SConverterConfiguration::IsValid() const noexcept
{
    switch (Mode)
    {
    case EConverterConfigurationMode::Disabled:
        return true;
    case EConverterConfigurationMode::BuiltInFb2Cng:
        return Fb2Cng.IsValid();
    case EConverterConfigurationMode::CustomCommand:
        return Custom.IsValid();
    }

    return false;
}

std::optional<LibriFlow::ConverterCommand::SConverterCommandProfile> TryBuildCommandProfile(
    const SConverterConfiguration& configuration)
{
    if (!configuration.IsValid())
    {
        return std::nullopt;
    }

    switch (configuration.Mode)
    {
    case EConverterConfigurationMode::Disabled:
        return std::nullopt;
    case EConverterConfigurationMode::BuiltInFb2Cng:
        return LibriFlow::ConverterCommand::CConverterCommandBuilder::CreateFb2CngProfile(
            configuration.Fb2Cng.ExecutablePath,
            configuration.Fb2Cng.ConfigPath);
    case EConverterConfigurationMode::CustomCommand:
        return LibriFlow::ConverterCommand::SConverterCommandProfile{
            .ExecutablePath = configuration.Custom.ExecutablePath,
            .ArgumentTemplate = configuration.Custom.ArgumentTemplate,
            .OutputMode = configuration.Custom.OutputMode
        };
    }

    return std::nullopt;
}

} // namespace LibriFlow::ConverterConfiguration
