#pragma once

#include "PipeTransport/NamedPipeChannel.hpp"
#include "PipeTransport/PipeRequestDispatcher.hpp"

namespace Librova::PipeHost {

class CNamedPipeHost final
{
public:
    explicit CNamedPipeHost(Librova::PipeTransport::CPipeRequestDispatcher& dispatcher);

    void RunSingleSession(Librova::PipeTransport::CNamedPipeConnection connection) const;

private:
    Librova::PipeTransport::CPipeRequestDispatcher& m_dispatcher;
};

} // namespace Librova::PipeHost
