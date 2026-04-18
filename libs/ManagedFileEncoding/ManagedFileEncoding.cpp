#include "ManagedFileEncoding/ManagedFileEncoding.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <zlib.h>

#include "Foundation/UnicodeConversion.hpp"

namespace Librova::ManagedFileEncoding {
namespace {

constexpr std::size_t GChunkSize = 64 * 1024;

[[nodiscard]] std::runtime_error BuildIoError(
    const std::string_view action,
    const std::filesystem::path& path)
{
    return std::runtime_error(
        std::string{action}
        + ": "
        + Librova::Unicode::PathToUtf8(path));
}

[[nodiscard]] bool IsSamePath(
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& destinationPath)
{
    if (sourcePath.lexically_normal() == destinationPath.lexically_normal())
    {
        return true;
    }

    std::error_code errorCode;
    if (std::filesystem::exists(sourcePath, errorCode)
        && !errorCode
        && std::filesystem::exists(destinationPath, errorCode)
        && !errorCode)
    {
        return std::filesystem::equivalent(sourcePath, destinationPath, errorCode) && !errorCode;
    }

    return false;
}

void RemovePathNoThrow(const std::filesystem::path& path) noexcept
{
    if (path.empty())
    {
        return;
    }

    std::error_code errorCode;
    std::filesystem::remove(path, errorCode);
}

[[nodiscard]] std::filesystem::path BuildTemporaryOutputPath(const std::filesystem::path& destinationPath)
{
    const auto parentPath = destinationPath.parent_path();
    const auto uniqueSuffix = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    auto temporaryName = destinationPath.filename();
    temporaryName += ".tmp-";
    temporaryName += uniqueSuffix;
    return parentPath / temporaryName;
}

void ReplaceFileFromTemporaryPath(
    const std::filesystem::path& temporaryPath,
    const std::filesystem::path& destinationPath)
{
    std::error_code errorCode;
    std::filesystem::remove(destinationPath, errorCode);
    errorCode.clear();
    std::filesystem::rename(temporaryPath, destinationPath, errorCode);
    if (errorCode)
    {
        throw BuildIoError("Failed to finalize destination file", destinationPath);
    }
}

void CopyFileToPath(
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& destinationPath)
{
    if (IsSamePath(sourcePath, destinationPath))
    {
        return;
    }

    const auto temporaryPath = BuildTemporaryOutputPath(destinationPath);
    std::ifstream input(sourcePath, std::ios::binary);
    if (!input)
    {
        throw BuildIoError("Failed to open source file", sourcePath);
    }

    std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        throw BuildIoError("Failed to open destination file", temporaryPath);
    }

    try
    {
        output << input.rdbuf();

        if (!input.good() && !input.eof())
        {
            throw BuildIoError("Failed to read source file", sourcePath);
        }

        if (!output)
        {
            throw BuildIoError("Failed to write destination file", temporaryPath);
        }

        output.close();
        ReplaceFileFromTemporaryPath(temporaryPath, destinationPath);
    }
    catch (...)
    {
        output.close();
        RemovePathNoThrow(temporaryPath);
        throw;
    }
}

void DeflateFileToPath(
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& destinationPath)
{
    if (IsSamePath(sourcePath, destinationPath))
    {
        throw std::invalid_argument("Source and destination paths must differ for compressed managed-file encoding.");
    }

    const auto temporaryPath = BuildTemporaryOutputPath(destinationPath);
    std::ifstream input(sourcePath, std::ios::binary);
    if (!input)
    {
        throw BuildIoError("Failed to open source file", sourcePath);
    }

    std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        throw BuildIoError("Failed to open destination file", temporaryPath);
    }

    z_stream stream{};
    if (deflateInit2(&stream, Z_BEST_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) != Z_OK)
    {
        throw std::runtime_error("Failed to initialize managed-file compression stream.");
    }

    std::array<unsigned char, GChunkSize> inputBuffer{};
    std::array<unsigned char, GChunkSize> outputBuffer{};

    try
    {
        int flush = Z_NO_FLUSH;

        do
        {
            input.read(reinterpret_cast<char*>(inputBuffer.data()), static_cast<std::streamsize>(inputBuffer.size()));
            const auto bytesRead = input.gcount();

            if (bytesRead < 0)
            {
                throw BuildIoError("Failed to read source file", sourcePath);
            }

            flush = input.eof() ? Z_FINISH : Z_NO_FLUSH;
            stream.next_in = inputBuffer.data();
            stream.avail_in = static_cast<uInt>(bytesRead);

            do
            {
                stream.next_out = outputBuffer.data();
                stream.avail_out = static_cast<uInt>(outputBuffer.size());
                const int result = deflate(&stream, flush);

                if (result == Z_STREAM_ERROR)
                {
                    throw std::runtime_error("Managed-file compression failed.");
                }

                const auto producedBytes = outputBuffer.size() - stream.avail_out;
                output.write(
                    reinterpret_cast<const char*>(outputBuffer.data()),
                    static_cast<std::streamsize>(producedBytes));

                if (!output)
                {
                    throw BuildIoError("Failed to write destination file", temporaryPath);
                }
            }
            while (stream.avail_out == 0);
        }
        while (flush != Z_FINISH);

        output.close();
        ReplaceFileFromTemporaryPath(temporaryPath, destinationPath);
    }
    catch (...)
    {
        output.close();
        RemovePathNoThrow(temporaryPath);
        deflateEnd(&stream);
        throw;
    }

    deflateEnd(&stream);
}

void InflateFileToPath(
    const std::filesystem::path& sourcePath,
    const std::filesystem::path& destinationPath)
{
    if (IsSamePath(sourcePath, destinationPath))
    {
        throw std::invalid_argument("Source and destination paths must differ for compressed managed-file decoding.");
    }

    const auto temporaryPath = BuildTemporaryOutputPath(destinationPath);
    std::ifstream input(sourcePath, std::ios::binary);
    if (!input)
    {
        throw BuildIoError("Failed to open source file", sourcePath);
    }

    std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        throw BuildIoError("Failed to open destination file", temporaryPath);
    }

    z_stream stream{};
    if (inflateInit2(&stream, 15 + 32) != Z_OK)
    {
        throw std::runtime_error("Failed to initialize managed-file decompression stream.");
    }

    std::array<unsigned char, GChunkSize> inputBuffer{};
    std::array<unsigned char, GChunkSize> outputBuffer{};
    int result = Z_OK;

    try
    {
        do
        {
            input.read(reinterpret_cast<char*>(inputBuffer.data()), static_cast<std::streamsize>(inputBuffer.size()));
            const auto bytesRead = input.gcount();

            if (bytesRead < 0)
            {
                throw BuildIoError("Failed to read source file", sourcePath);
            }

            stream.next_in = inputBuffer.data();
            stream.avail_in = static_cast<uInt>(bytesRead);

            if (stream.avail_in == 0)
            {
                break;
            }

            do
            {
                stream.next_out = outputBuffer.data();
                stream.avail_out = static_cast<uInt>(outputBuffer.size());
                result = inflate(&stream, Z_NO_FLUSH);

                if (result != Z_OK && result != Z_STREAM_END)
                {
                    throw std::runtime_error("Managed-file decompression failed.");
                }

                const auto producedBytes = outputBuffer.size() - stream.avail_out;
                output.write(
                    reinterpret_cast<const char*>(outputBuffer.data()),
                    static_cast<std::streamsize>(producedBytes));

                if (!output)
                {
                    throw BuildIoError("Failed to write destination file", temporaryPath);
                }
            }
            while (stream.avail_out == 0);
        }
        while (result != Z_STREAM_END);

        if (result != Z_STREAM_END)
        {
            throw std::runtime_error("Managed-file decompression finished before reaching end of stream.");
        }

        output.close();
        ReplaceFileFromTemporaryPath(temporaryPath, destinationPath);
    }
    catch (...)
    {
        output.close();
        RemovePathNoThrow(temporaryPath);
        inflateEnd(&stream);
        throw;
    }

    inflateEnd(&stream);
}

} // namespace

void EncodeFileToPath(
    const std::filesystem::path& sourcePath,
    const Librova::Domain::EStorageEncoding encoding,
    const std::filesystem::path& destinationPath)
{
    switch (encoding)
    {
    case Librova::Domain::EStorageEncoding::Plain:
        CopyFileToPath(sourcePath, destinationPath);
        return;
    case Librova::Domain::EStorageEncoding::Compressed:
        DeflateFileToPath(sourcePath, destinationPath);
        return;
    }

    throw std::runtime_error("Unsupported managed-file storage encoding.");
}

void DecodeFileToPath(
    const std::filesystem::path& sourcePath,
    const Librova::Domain::EStorageEncoding encoding,
    const std::filesystem::path& destinationPath)
{
    switch (encoding)
    {
    case Librova::Domain::EStorageEncoding::Plain:
        CopyFileToPath(sourcePath, destinationPath);
        return;
    case Librova::Domain::EStorageEncoding::Compressed:
        InflateFileToPath(sourcePath, destinationPath);
        return;
    }

    throw std::runtime_error("Unsupported managed-file storage encoding.");
}

} // namespace Librova::ManagedFileEncoding
