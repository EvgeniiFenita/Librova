#pragma once

#include "Transport/NamedPipeChannel.hpp"
#include "Transport/PipeRequestDispatcher.hpp"

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
