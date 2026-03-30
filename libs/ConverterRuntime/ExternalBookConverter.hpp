#pragma once

#include <chrono>

#include "ConverterCommand/ConverterCommandBuilder.hpp"
#include "Domain/ServiceContracts.hpp"

namespace LibriFlow::ConverterRuntime {

struct SExternalConverterSettings
{
    LibriFlow::ConverterCommand::SConverterCommandProfile CommandProfile;
    std::chrono::milliseconds PollInterval{100};

    [[nodiscard]] bool IsValid() const noexcept
    {
        return CommandProfile.IsValid() && PollInterval.count() > 0;
    }
};

class CExternalBookConverter final : public LibriFlow::Domain::IBookConverter
{
public:
    explicit CExternalBookConverter(SExternalConverterSettings settings);

    [[nodiscard]] bool CanConvert(
        LibriFlow::Domain::EBookFormat sourceFormat,
        LibriFlow::Domain::EBookFormat destinationFormat) const override;

    [[nodiscard]] LibriFlow::Domain::SConversionResult Convert(
        const LibriFlow::Domain::SConversionRequest& request,
        LibriFlow::Domain::IProgressSink& progressSink,
        std::stop_token stopToken) const override;

private:
    SExternalConverterSettings m_settings;
};

} // namespace LibriFlow::ConverterRuntime
