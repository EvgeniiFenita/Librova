#include "Hashing/Sha256.hpp"

#include <iomanip>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>

#include "Unicode/UnicodeConversion.hpp"

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

namespace {

class CScopedAlgorithmProvider final
{
public:
    explicit CScopedAlgorithmProvider(LPCWSTR pszAlgId)
    {
        const NTSTATUS status = BCryptOpenAlgorithmProvider(&m_handle, pszAlgId, nullptr, 0);
        if (!NT_SUCCESS(status))
        {
            throw std::runtime_error("BCryptOpenAlgorithmProvider failed");
        }
    }

    ~CScopedAlgorithmProvider()
    {
        if (m_handle != nullptr)
        {
            BCryptCloseAlgorithmProvider(m_handle, 0);
        }
    }

    CScopedAlgorithmProvider(const CScopedAlgorithmProvider&) = delete;
    CScopedAlgorithmProvider& operator=(const CScopedAlgorithmProvider&) = delete;
    CScopedAlgorithmProvider(CScopedAlgorithmProvider&&) = delete;
    CScopedAlgorithmProvider& operator=(CScopedAlgorithmProvider&&) = delete;

    [[nodiscard]] BCRYPT_ALG_HANDLE Get() const noexcept
    {
        return m_handle;
    }

    [[nodiscard]] DWORD GetProperty(LPCWSTR property) const
    {
        DWORD value = 0;
        DWORD dataLen = 0;
        const NTSTATUS status = BCryptGetProperty(
            m_handle,
            property,
            reinterpret_cast<PUCHAR>(&value),
            sizeof(DWORD),
            &dataLen,
            0);
        if (!NT_SUCCESS(status))
        {
            throw std::runtime_error("BCryptGetProperty failed");
        }
        return value;
    }

private:
    BCRYPT_ALG_HANDLE m_handle = nullptr;
};

class CScopedHash final
{
public:
    CScopedHash(BCRYPT_ALG_HANDLE hAlg, std::vector<BYTE>& hashObject)
    {
        const NTSTATUS status = BCryptCreateHash(
            hAlg,
            &m_handle,
            hashObject.data(),
            static_cast<ULONG>(hashObject.size()),
            nullptr,
            0,
            0);
        if (!NT_SUCCESS(status))
        {
            throw std::runtime_error("BCryptCreateHash failed");
        }
    }

    ~CScopedHash()
    {
        if (m_handle != nullptr)
        {
            BCryptDestroyHash(m_handle);
        }
    }

    CScopedHash(const CScopedHash&) = delete;
    CScopedHash& operator=(const CScopedHash&) = delete;
    CScopedHash(CScopedHash&&) = delete;
    CScopedHash& operator=(CScopedHash&&) = delete;

    void HashData(const BYTE* data, const ULONG dataSize)
    {
        const NTSTATUS status = BCryptHashData(
            m_handle,
            const_cast<PUCHAR>(data),
            dataSize,
            0);
        if (!NT_SUCCESS(status))
        {
            throw std::runtime_error("BCryptHashData failed");
        }
    }

    void Finish(std::vector<BYTE>& output)
    {
        const NTSTATUS status = BCryptFinishHash(
            m_handle,
            output.data(),
            static_cast<ULONG>(output.size()),
            0);
        if (!NT_SUCCESS(status))
        {
            throw std::runtime_error("BCryptFinishHash failed");
        }
    }

private:
    BCRYPT_HASH_HANDLE m_handle = nullptr;
};

} // namespace

namespace Librova::Hashing {

std::string ComputeFileSha256Hex(const std::filesystem::path& filePath)
{
    CScopedAlgorithmProvider algProvider(BCRYPT_SHA256_ALGORITHM);
    const DWORD hashObjectSize = algProvider.GetProperty(BCRYPT_OBJECT_LENGTH);
    const DWORD hashSize = algProvider.GetProperty(BCRYPT_HASH_LENGTH);

    std::vector<BYTE> hashObject(hashObjectSize);
    CScopedHash hash(algProvider.Get(), hashObject);

    std::ifstream file(filePath, std::ios::binary);
    if (!file)
    {
        throw std::runtime_error(
            "Cannot open file for hashing: " + Librova::Unicode::PathToUtf8(filePath));
    }

    constexpr std::size_t kChunkSize = 65536;
    std::vector<char> buffer(kChunkSize);
    while (file)
    {
        file.read(buffer.data(), static_cast<std::streamsize>(kChunkSize));
        const auto bytesRead = static_cast<ULONG>(file.gcount());
        if (bytesRead > 0)
        {
            hash.HashData(reinterpret_cast<const BYTE*>(buffer.data()), bytesRead);
        }
    }

    std::vector<BYTE> digest(hashSize);
    hash.Finish(digest);

    std::ostringstream oss;
    for (const BYTE byte : digest)
    {
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned int>(byte);
    }

    return oss.str();
}

} // namespace Librova::Hashing
