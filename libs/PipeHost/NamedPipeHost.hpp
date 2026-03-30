#pragma once

#include "PipeTransport/NamedPipeChannel.hpp"
#include "PipeTransport/PipeRequestDispatcher.hpp"

namespace LibriFlow::PipeHost {

class CNamedPipeHost final
{
public:
    explicit CNamedPipeHost(LibriFlow::PipeTransport::CPipeRequestDispatcher& dispatcher);

    void RunSingleSession(LibriFlow::PipeTransport::CNamedPipeConnection connection) const;

private:
    LibriFlow::PipeTransport::CPipeRequestDispatcher& m_dispatcher;
};

} // namespace LibriFlow::PipeHost
