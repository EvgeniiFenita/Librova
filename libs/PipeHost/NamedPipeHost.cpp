#include "PipeHost/NamedPipeHost.hpp"

namespace LibriFlow::PipeHost {

CNamedPipeHost::CNamedPipeHost(LibriFlow::PipeTransport::CPipeRequestDispatcher& dispatcher)
    : m_dispatcher(dispatcher)
{
}

void CNamedPipeHost::RunSingleSession(LibriFlow::PipeTransport::CNamedPipeConnection connection) const
{
    const auto requestBytes = connection.ReadMessage();
    const auto parsedRequest = LibriFlow::PipeTransport::DeserializeRequestEnvelope(requestBytes);

    LibriFlow::PipeTransport::SPipeResponseEnvelope response;

    if (!parsedRequest.HasValue())
    {
        response = {
            .Status = LibriFlow::PipeTransport::EPipeResponseStatus::InvalidRequest,
            .ErrorMessage = parsedRequest.Error
        };
    }
    else
    {
        response = m_dispatcher.Dispatch(*parsedRequest.Value);
    }

    connection.WriteMessage(LibriFlow::PipeTransport::SerializeResponseEnvelope(response));
}

} // namespace LibriFlow::PipeHost
