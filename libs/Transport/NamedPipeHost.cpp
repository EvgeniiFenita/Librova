#include "Transport/NamedPipeHost.hpp"

namespace Librova::PipeHost {

CNamedPipeHost::CNamedPipeHost(Librova::PipeTransport::CPipeRequestDispatcher& dispatcher)
    : m_dispatcher(dispatcher)
{
}

void CNamedPipeHost::RunSingleSession(Librova::PipeTransport::CNamedPipeConnection connection) const
{
    const auto requestBytes = connection.ReadMessage();
    const auto parsedRequest = Librova::PipeTransport::DeserializeRequestEnvelope(requestBytes);

    Librova::PipeTransport::SPipeResponseEnvelope response;

    if (!parsedRequest.HasValue())
    {
        response = {
            .Status = Librova::PipeTransport::EPipeResponseStatus::InvalidRequest,
            .ErrorMessage = parsedRequest.Error
        };
    }
    else
    {
        response = m_dispatcher.Dispatch(*parsedRequest.Value);
    }

    connection.WriteMessage(Librova::PipeTransport::SerializeResponseEnvelope(response));
}

} // namespace Librova::PipeHost
