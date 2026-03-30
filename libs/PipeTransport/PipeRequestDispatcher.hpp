#pragma once

#include "PipeTransport/PipeProtocol.hpp"
#include "ProtoServices/LibraryJobServiceAdapter.hpp"

namespace LibriFlow::PipeTransport {

class CPipeRequestDispatcher final
{
public:
    explicit CPipeRequestDispatcher(LibriFlow::ProtoServices::CLibraryJobServiceAdapter& adapter);

    [[nodiscard]] SPipeResponseEnvelope Dispatch(const SPipeRequestEnvelope& request) const;

private:
    template <typename TRequest, typename TResponse, typename TCallback>
    [[nodiscard]] SPipeResponseEnvelope DispatchTyped(
        const SPipeRequestEnvelope& request,
        TCallback&& callback) const;

    LibriFlow::ProtoServices::CLibraryJobServiceAdapter& m_adapter;
};

} // namespace LibriFlow::PipeTransport
