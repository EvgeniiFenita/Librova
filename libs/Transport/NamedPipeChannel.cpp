#include "Transport/NamedPipeChannel.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>

#include <array>
#include <bit>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

namespace Librova::PipeTransport {
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

class CEventHandle final
{
public:
    explicit CEventHandle(HANDLE handle = nullptr) noexcept
        : m_handle(handle)
    {
    }

    ~CEventHandle()
    {
        if (m_handle != nullptr)
        {
            CloseHandle(m_handle);
        }
    }

    CEventHandle(const CEventHandle&) = delete;
    CEventHandle& operator=(const CEventHandle&) = delete;

    [[nodiscard]] HANDLE Get() const noexcept
    {
        return m_handle;
    }

private:
    HANDLE m_handle = nullptr;
};

[[nodiscard]] std::optional<std::chrono::milliseconds> GetRemainingTimeout(
    const std::chrono::steady_clock::time_point startTime,
    const std::optional<std::chrono::milliseconds> timeout)
{
    if (!timeout.has_value())
    {
        return std::nullopt;
    }

    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - startTime);

    if (elapsed >= timeout.value())
    {
        return std::chrono::milliseconds::zero();
    }

    return timeout.value() - elapsed;
}

template <typename TOperation>
void RunExactOverlapped(
    const HANDLE handle,
    std::byte* buffer,
    const std::size_t byteCount,
    const std::optional<std::chrono::milliseconds> timeout,
    const bool zeroTransferClosesPipe,
    const std::string_view timeoutMessage,
    const std::string_view zeroTransferMessage,
    const std::string_view failurePrefix,
    TOperation&& operation)
{
    const HANDLE eventHandle = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (eventHandle == nullptr)
    {
        throw std::runtime_error(BuildWindowsErrorMessage("Failed to create named pipe overlapped event"));
    }

    CEventHandle event(eventHandle);
    const auto startTime = std::chrono::steady_clock::now();
    std::size_t totalTransferred = 0;

    while (totalTransferred < byteCount)
    {
        OVERLAPPED overlapped{};
        overlapped.hEvent = event.Get();
        ResetEvent(event.Get());

        DWORD transferredBytes = 0;
        const BOOL completedImmediately = operation(
            buffer + totalTransferred,
            static_cast<DWORD>(byteCount - totalTransferred),
            &transferredBytes,
            &overlapped);
        if (completedImmediately != FALSE)
        {
            if (transferredBytes == 0 && zeroTransferClosesPipe)
            {
                throw std::runtime_error(std::string{zeroTransferMessage});
            }

            totalTransferred += transferredBytes;
            continue;
        }

        const DWORD errorCode = GetLastError();
        if (errorCode != ERROR_IO_PENDING)
        {
            throw std::runtime_error(BuildWindowsErrorMessage(failurePrefix));
        }

        const auto remainingTimeout = GetRemainingTimeout(startTime, timeout);
        if (remainingTimeout.has_value() && remainingTimeout.value() == std::chrono::milliseconds::zero())
        {
            CancelIoEx(handle, &overlapped);
            WaitForSingleObject(event.Get(), INFINITE);
            throw std::runtime_error(std::string{timeoutMessage});
        }

        const DWORD waitResult = WaitForSingleObject(
            event.Get(),
            remainingTimeout.has_value()
                ? static_cast<DWORD>(remainingTimeout.value().count())
                : INFINITE);
        if (waitResult == WAIT_TIMEOUT)
        {
            CancelIoEx(handle, &overlapped);
            WaitForSingleObject(event.Get(), INFINITE);
            throw std::runtime_error(std::string{timeoutMessage});
        }

        if (waitResult != WAIT_OBJECT_0)
        {
            throw std::runtime_error(BuildWindowsErrorMessage("Failed while waiting for named pipe overlapped I/O"));
        }

        if (GetOverlappedResult(handle, &overlapped, &transferredBytes, FALSE) == FALSE)
        {
            throw std::runtime_error(BuildWindowsErrorMessage(failurePrefix));
        }

        if (transferredBytes == 0 && zeroTransferClosesPipe)
        {
            throw std::runtime_error(std::string{zeroTransferMessage});
        }

        totalTransferred += transferredBytes;
    }
}

void WriteFramedMessage(
    const HANDLE handle,
    const bool supportsOverlappedIo,
    const std::vector<std::byte>& bytes,
    const std::optional<std::chrono::milliseconds> timeout)
{
    const std::uint32_t byteCount = static_cast<std::uint32_t>(bytes.size());
    if (supportsOverlappedIo)
    {
        auto rawLength = std::bit_cast<std::array<std::byte, sizeof(byteCount)>>(byteCount);
        RunExactOverlapped(
            handle,
            rawLength.data(),
            rawLength.size(),
            timeout,
            false,
            "Timed out writing named pipe data.",
            "",
            "Failed to write to named pipe",
            [handle](std::byte* chunk, const DWORD chunkSize, DWORD* bytesTransferred, OVERLAPPED* overlapped) {
                return WriteFile(handle, chunk, chunkSize, bytesTransferred, overlapped);
            });

        if (!bytes.empty())
        {
            RunExactOverlapped(
                handle,
                const_cast<std::byte*>(bytes.data()),
                bytes.size(),
                timeout,
                false,
                "Timed out writing named pipe data.",
                "",
                "Failed to write to named pipe",
                [handle](std::byte* chunk, const DWORD chunkSize, DWORD* bytesTransferred, OVERLAPPED* overlapped) {
                    return WriteFile(handle, chunk, chunkSize, bytesTransferred, overlapped);
                });
        }

        return;
    }

    WriteLengthPrefix(handle, byteCount);
    if (!bytes.empty())
    {
        WriteExact(handle, bytes.data(), bytes.size());
    }
}

} // namespace

CNamedPipeConnection::CNamedPipeConnection(void* nativeHandle, const bool supportsOverlappedIo) noexcept
    : m_handle(nativeHandle, &ClosePipeHandle)
    , m_supportsOverlappedIo(supportsOverlappedIo)
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

    WriteFramedMessage(static_cast<HANDLE>(m_handle.get()), m_supportsOverlappedIo, bytes, std::nullopt);
}

void CNamedPipeConnection::WriteMessage(
    const std::vector<std::byte>& bytes,
    const std::chrono::milliseconds timeout) const
{
    if (!IsOpen())
    {
        throw std::runtime_error("Named pipe connection is not open.");
    }

    if (bytes.size() > std::numeric_limits<std::uint32_t>::max())
    {
        throw std::runtime_error("Named pipe message exceeds uint32 framing limit.");
    }

    WriteFramedMessage(static_cast<HANDLE>(m_handle.get()), m_supportsOverlappedIo, bytes, timeout);
}

std::vector<std::byte> CNamedPipeConnection::ReadMessage() const
{
    if (!IsOpen())
    {
        throw std::runtime_error("Named pipe connection is not open.");
    }

    const HANDLE handle = static_cast<HANDLE>(m_handle.get());
    if (m_supportsOverlappedIo)
    {
        auto rawLength = std::array<std::byte, sizeof(std::uint32_t)>{};
        RunExactOverlapped(
            handle,
            rawLength.data(),
            rawLength.size(),
            std::nullopt,
            true,
            "Timed out waiting for named pipe data.",
            "Named pipe closed while reading a framed message.",
            "Failed to read from named pipe",
            [handle](std::byte* chunk, const DWORD chunkSize, DWORD* bytesTransferred, OVERLAPPED* overlapped) {
                return ReadFile(handle, chunk, chunkSize, bytesTransferred, overlapped);
            });

        const auto byteCount = static_cast<std::size_t>(std::bit_cast<std::uint32_t>(rawLength));
        if (byteCount > MaxPipeFrameBytes)
        {
            throw std::runtime_error("Named pipe frame exceeds the configured maximum size.");
        }

        std::vector<std::byte> bytes(byteCount);
        if (!bytes.empty())
        {
            RunExactOverlapped(
                handle,
                bytes.data(),
                bytes.size(),
                std::nullopt,
                true,
                "Timed out waiting for named pipe data.",
                "Named pipe closed while reading a framed message.",
                "Failed to read from named pipe",
                [handle](std::byte* chunk, const DWORD chunkSize, DWORD* bytesTransferred, OVERLAPPED* overlapped) {
                    return ReadFile(handle, chunk, chunkSize, bytesTransferred, overlapped);
                });
        }

        return bytes;
    }

    const auto byteCount = static_cast<std::size_t>(ReadLengthPrefix(handle));
    if (byteCount > MaxPipeFrameBytes)
    {
        throw std::runtime_error("Named pipe frame exceeds the configured maximum size.");
    }

    std::vector<std::byte> bytes(byteCount);
    if (byteCount != 0)
    {
        ReadExact(handle, bytes.data(), bytes.size());
    }

    return bytes;
}

std::vector<std::byte> CNamedPipeConnection::ReadMessage(const std::chrono::milliseconds timeout) const
{
    if (!IsOpen())
    {
        throw std::runtime_error("Named pipe connection is not open.");
    }
    if (!m_supportsOverlappedIo)
    {
        throw std::runtime_error("Timed named pipe reads require overlapped I/O support.");
    }

    const HANDLE handle = static_cast<HANDLE>(m_handle.get());
    const auto startTime = std::chrono::steady_clock::now();

    std::array<std::byte, sizeof(std::uint32_t)> rawLength{};
    RunExactOverlapped(
        handle,
        rawLength.data(),
        rawLength.size(),
        timeout,
        true,
        "Timed out waiting for named pipe data.",
        "Named pipe closed while reading a framed message.",
        "Failed to read from named pipe",
        [handle](std::byte* chunk, const DWORD chunkSize, DWORD* bytesTransferred, OVERLAPPED* overlapped) {
            return ReadFile(handle, chunk, chunkSize, bytesTransferred, overlapped);
        });

    const auto byteCount = static_cast<std::size_t>(std::bit_cast<std::uint32_t>(rawLength));
    if (byteCount > MaxPipeFrameBytes)
    {
        throw std::runtime_error("Named pipe frame exceeds the configured maximum size.");
    }

    std::vector<std::byte> bytes(byteCount);
    if (byteCount != 0)
    {
        RunExactOverlapped(
            handle,
            bytes.data(),
            bytes.size(),
            GetRemainingTimeout(startTime, timeout),
            true,
            "Timed out waiting for named pipe data.",
            "Named pipe closed while reading a framed message.",
            "Failed to read from named pipe",
            [handle](std::byte* chunk, const DWORD chunkSize, DWORD* bytesTransferred, OVERLAPPED* overlapped) {
                return ReadFile(handle, chunk, chunkSize, bytesTransferred, overlapped);
            });
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

    return CNamedPipeConnection{m_handle.release(), false};
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
        FILE_FLAG_OVERLAPPED,
        nullptr);

    if (handle == INVALID_HANDLE_VALUE)
    {
        throw std::runtime_error(BuildWindowsErrorMessage("Failed to connect to named pipe server"));
    }

    return CNamedPipeConnection{handle, true};
}

} // namespace Librova::PipeTransport
