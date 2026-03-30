#pragma once

#include <chrono>

#include "ConverterCommand/ConverterCommandBuilder.hpp"
#include "Domain/ServiceContracts.hpp"

namespace Librova::ConverterRuntime {

struct SExternalConverterSettings
{
    Librova::ConverterCommand::SConverterCommandProfile CommandProfile;
    std::chrono::milliseconds PollInterval{100};

    [[nodiscard]] bool IsValid() const noexcept
    {
        return CommandProfile.IsValid() && PollInterval.count() > 0;
    }
};

class CExternalBookConverter final : public Librova::Domain::IBookConverter
{
public:
    explicit CExternalBookConverter(SExternalConverterSettings settings);

    [[nodiscard]] bool CanConvert(
        Librova::Domain::EBookFormat sourceFormat,
        Librova::Domain::EBookFormat destinationFormat) const override;

    [[nodiscard]] Librova::Domain::SConversionResult Convert(
        const Librova::Domain::SConversionRequest& request,
        Librova::Domain::IProgressSink& progressSink,
        std::stop_token stopToken) const override;

private:
    SExternalConverterSettings m_settings;
};

} // namespace Librova::ConverterRuntime
