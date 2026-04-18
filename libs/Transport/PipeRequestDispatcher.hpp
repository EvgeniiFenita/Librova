#pragma once

#include "Transport/PipeProtocol.hpp"
#include "ProtoServices/LibraryJobServiceAdapter.hpp"

namespace Librova::PipeTransport {

class CPipeRequestDispatcher final
{
public:
    explicit CPipeRequestDispatcher(Librova::ProtoServices::CLibraryJobServiceAdapter& adapter);

    [[nodiscard]] SPipeResponseEnvelope Dispatch(const SPipeRequestEnvelope& request) const;

private:
    template <typename TRequest, typename TResponse, typename TCallback>
    [[nodiscard]] SPipeResponseEnvelope DispatchTyped(
        const SPipeRequestEnvelope& request,
        TCallback&& callback) const;

    Librova::ProtoServices::CLibraryJobServiceAdapter& m_adapter;
};

} // namespace Librova::PipeTransport
