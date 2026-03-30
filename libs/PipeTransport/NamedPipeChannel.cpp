#include "PipeTransport/NamedPipeChannel.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <array>
#include <bit>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace LibriFlow::PipeTransport {
namespace {

[[nodiscard]] std::wstring ToWidePath(const std::filesystem::path& path)
{
    return path.native();
}

[[nodiscard]] std::string BuildWindowsErrorMessage(const std::string_view prefix)
{
    const DWORD errorCode = GetLastError();
    std::string message{prefix};
    message.append(" (win32=");
    message.append(std::to_string(static_cast<unsigned long>(errorCode)));
    message.push_back(')');
    return message;
}

void ReadExact(const HANDLE handle, std::byte* buffer, const std::size_t byteCount)
{
    std::size_t totalRead = 0;
    while (totalRead < byteCount)
    {
        DWORD bytesRead = 0;
        const BOOL readResult = ReadFile(
            handle,
            buffer + totalRead,
            static_cast<DWORD>(byteCount - totalRead),
            &bytesRead,
            nullptr);

        if (readResult == FALSE)
        {
            throw std::runtime_error(BuildWindowsErrorMessage("Failed to read from named pipe"));
        }

        if (bytesRead == 0)
        {
            throw std::runtime_error("Named pipe closed while reading a framed message.");
        }

        totalRead += bytesRead;
    }
}

void WriteExact(const HANDLE handle, const std::byte* buffer, const std::size_t byteCount)
{
    std::size_t totalWritten = 0;
    while (totalWritten < byteCount)
    {
        DWORD bytesWritten = 0;
        const BOOL writeResult = WriteFile(
            handle,
            buffer + totalWritten,
            static_cast<DWORD>(byteCount - totalWritten),
            &bytesWritten,
            nullptr);

        if (writeResult == FALSE)
        {
            throw std::runtime_error(BuildWindowsErrorMessage("Failed to write to named pipe"));
        }

        totalWritten += bytesWritten;
    }
}

[[nodiscard]] std::uint32_t ReadLengthPrefix(const HANDLE handle)
{
    std::array<std::byte, sizeof(std::uint32_t)> raw{};
    ReadExact(handle, raw.data(), raw.size());
    return std::bit_cast<std::uint32_t>(raw);
}

void WriteLengthPrefix(const HANDLE handle, const std::uint32_t value)
{
    const auto raw = std::bit_cast<std::array<std::byte, sizeof(value)>>(value);
    WriteExact(handle, raw.data(), raw.size());
}

void ClosePipeHandle(void* nativeHandle) noexcept
{
    if (nativeHandle != nullptr && nativeHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(static_cast<HANDLE>(nativeHandle));
    }
}

} // namespace

CNamedPipeConnection::CNamedPipeConnection(void* nativeHandle) noexcept
    : m_handle(nativeHandle, &ClosePipeHandle)
{
}

bool CNamedPipeConnection::IsOpen() const noexcept
{
    return m_handle != nullptr && m_handle.get() != INVALID_HANDLE_VALUE;
}

void CNamedPipeConnection::WriteMessage(const std::vector<std::byte>& bytes) const
{
    if (!IsOpen())
    {
        throw std::runtime_error("Named pipe connection is not open.");
    }

    if (bytes.size() > std::numeric_limits<std::uint32_t>::max())
    {
        throw std::runtime_error("Named pipe message exceeds uint32 framing limit.");
    }

    const HANDLE handle = static_cast<HANDLE>(m_handle.get());
    WriteLengthPrefix(handle, static_cast<std::uint32_t>(bytes.size()));
    if (!bytes.empty())
    {
        WriteExact(handle, bytes.data(), bytes.size());
    }
}

std::vector<std::byte> CNamedPipeConnection::ReadMessage() const
{
    if (!IsOpen())
    {
        throw std::runtime_error("Named pipe connection is not open.");
    }

    const HANDLE handle = static_cast<HANDLE>(m_handle.get());
    const auto byteCount = static_cast<std::size_t>(ReadLengthPrefix(handle));

    std::vector<std::byte> bytes(byteCount);
    if (byteCount != 0)
    {
        ReadExact(handle, bytes.data(), bytes.size());
    }

    return bytes;
}

CNamedPipeServer::CNamedPipeServer(const std::filesystem::path& pipePath)
    : m_handle(nullptr, &ClosePipeHandle)
{
    const HANDLE pipeHandle = CreateNamedPipeW(
        ToWidePath(pipePath).c_str(),
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1,
        64 * 1024,
        64 * 1024,
        0,
        nullptr);

    if (pipeHandle == INVALID_HANDLE_VALUE)
    {
        throw std::runtime_error(BuildWindowsErrorMessage("Failed to create named pipe server"));
    }

    m_handle.reset(static_cast<void*>(pipeHandle));
}

CNamedPipeConnection CNamedPipeServer::WaitForClient()
{
    if (m_handle == nullptr || m_handle.get() == INVALID_HANDLE_VALUE)
    {
        throw std::runtime_error("Named pipe server is not open.");
    }

    const HANDLE handle = static_cast<HANDLE>(m_handle.get());
    const BOOL connectResult = ConnectNamedPipe(handle, nullptr);
    if (connectResult == FALSE)
    {
        const DWORD errorCode = GetLastError();
        if (errorCode != ERROR_PIPE_CONNECTED)
        {
            throw std::runtime_error(BuildWindowsErrorMessage("Failed to accept named pipe client"));
        }
    }

    return CNamedPipeConnection{m_handle.release()};
}

CNamedPipeConnection ConnectToNamedPipe(
    const std::filesystem::path& pipePath,
    const std::chrono::milliseconds timeout)
{
    if (WaitNamedPipeW(ToWidePath(pipePath).c_str(), static_cast<DWORD>(timeout.count())) == FALSE)
    {
        throw std::runtime_error(BuildWindowsErrorMessage("Timed out waiting for named pipe server"));
    }

    const HANDLE handle = CreateFileW(
        ToWidePath(pipePath).c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        nullptr,
        OPEN_EXISTING,
        0,
        nullptr);

    if (handle == INVALID_HANDLE_VALUE)
    {
        throw std::runtime_error(BuildWindowsErrorMessage("Failed to connect to named pipe server"));
    }

    return CNamedPipeConnection{handle};
}

} // namespace LibriFlow::PipeTransport
