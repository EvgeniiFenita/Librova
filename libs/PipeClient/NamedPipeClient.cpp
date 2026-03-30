#include "PipeClient/NamedPipeClient.hpp"

#include <atomic>

namespace Librova::PipeClient {
namespace {

std::atomic<std::uint64_t> RequestCounter{1};

} // namespace

CPipeTransportError::CPipeTransportError(const std::string& message)
    : std::runtime_error(message)
{
}

CNamedPipeClient::CNamedPipeClient(std::filesystem::path pipePath)
    : m_pipePath(std::move(pipePath))
{
}

std::uint64_t CNamedPipeClient::NextRequestId() noexcept
{
    return RequestCounter.fetch_add(1, std::memory_order_relaxed);
}

} // namespace Librova::PipeClient
