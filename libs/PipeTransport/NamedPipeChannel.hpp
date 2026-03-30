#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

namespace LibriFlow::PipeTransport {

using TPipeHandle = std::unique_ptr<void, void(*)(void*)>;

class CNamedPipeConnection
{
public:
    CNamedPipeConnection(CNamedPipeConnection&&) noexcept = default;
    CNamedPipeConnection& operator=(CNamedPipeConnection&&) noexcept = default;

    CNamedPipeConnection(const CNamedPipeConnection&) = delete;
    CNamedPipeConnection& operator=(const CNamedPipeConnection&) = delete;

    [[nodiscard]] bool IsOpen() const noexcept;
    void WriteMessage(const std::vector<std::byte>& bytes) const;
    [[nodiscard]] std::vector<std::byte> ReadMessage() const;

private:
    explicit CNamedPipeConnection(void* nativeHandle) noexcept;

    TPipeHandle m_handle;

    friend class CNamedPipeServer;
    friend CNamedPipeConnection ConnectToNamedPipe(
        const std::filesystem::path& pipePath,
        std::chrono::milliseconds timeout);
};

class CNamedPipeServer
{
public:
    explicit CNamedPipeServer(const std::filesystem::path& pipePath);

    CNamedPipeServer(CNamedPipeServer&&) noexcept = default;
    CNamedPipeServer& operator=(CNamedPipeServer&&) noexcept = default;

    CNamedPipeServer(const CNamedPipeServer&) = delete;
    CNamedPipeServer& operator=(const CNamedPipeServer&) = delete;

    [[nodiscard]] CNamedPipeConnection WaitForClient();

private:
    TPipeHandle m_handle;
};

[[nodiscard]] CNamedPipeConnection ConnectToNamedPipe(
    const std::filesystem::path& pipePath,
    std::chrono::milliseconds timeout);

} // namespace LibriFlow::PipeTransport
