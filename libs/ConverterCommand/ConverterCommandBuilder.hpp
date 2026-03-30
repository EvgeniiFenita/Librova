#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "Domain/ServiceContracts.hpp"

namespace LibriFlow::ConverterCommand {

enum class EConverterOutputMode
{
    ExactDestinationPath,
    SingleFileInDestinationDirectory
};

struct SConverterCommandProfile
{
    std::filesystem::path ExecutablePath;
    std::vector<std::string> ArgumentTemplate;
    EConverterOutputMode OutputMode = EConverterOutputMode::ExactDestinationPath;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return !ExecutablePath.empty() && !ArgumentTemplate.empty();
    }
};

struct SResolvedConverterCommand
{
    std::filesystem::path ExecutablePath;
    std::vector<std::string> Arguments;
    EConverterOutputMode OutputMode = EConverterOutputMode::ExactDestinationPath;
    std::filesystem::path ExpectedOutputPath;
    std::filesystem::path ExpectedOutputDirectory;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return !ExecutablePath.empty() && !Arguments.empty();
    }
};

class CConverterCommandBuilder final
{
public:
    [[nodiscard]] static SConverterCommandProfile CreateFb2CngProfile(
        const std::filesystem::path& executablePath,
        const std::optional<std::filesystem::path>& configPath = std::nullopt);

    [[nodiscard]] static SResolvedConverterCommand Build(
        const SConverterCommandProfile& profile,
        const LibriFlow::Domain::SConversionRequest& request);
};

} // namespace LibriFlow::ConverterCommand
