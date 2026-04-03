#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "CoverProcessingWic/WicCoverImageProcessor.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <objbase.h>
#include <wincodec.h>
#endif

namespace {

#ifdef _WIN32

template <typename T>
void SafeRelease(T*& value) noexcept
{
    if (value != nullptr)
    {
        value->Release();
        value = nullptr;
    }
}

class CScopedCoInitialize final
{
public:
    CScopedCoInitialize()
    {
        const HRESULT result = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (SUCCEEDED(result))
        {
            m_ShouldUninitialize = true;
            return;
        }

        if (result != RPC_E_CHANGED_MODE)
        {
            throw std::runtime_error("Failed to initialize COM for test cover generation.");
        }
    }

    ~CScopedCoInitialize()
    {
        if (m_ShouldUninitialize)
        {
            ::CoUninitialize();
        }
    }

private:
    bool m_ShouldUninitialize = false;
};

[[nodiscard]] std::vector<std::byte> ReadStreamBytes(IStream* stream)
{
    HGLOBAL globalHandle = nullptr;
    if (FAILED(::GetHGlobalFromStream(stream, &globalHandle)) || globalHandle == nullptr)
    {
        throw std::runtime_error("Failed to access generated image stream.");
    }

    const auto size = static_cast<std::size_t>(::GlobalSize(globalHandle));
    void* lockedData = ::GlobalLock(globalHandle);
    if (lockedData == nullptr)
    {
        throw std::runtime_error("Failed to lock generated image stream.");
    }

    std::vector<std::byte> bytes(size);
    std::memcpy(bytes.data(), lockedData, size);
    ::GlobalUnlock(globalHandle);
    return bytes;
}

[[nodiscard]] std::vector<std::byte> CreateEncodedImage(
    const GUID& containerFormat,
    const std::uint32_t width,
    const std::uint32_t height)
{
    CScopedCoInitialize coInitialize;

    IWICImagingFactory* factory = nullptr;
    IStream* outputStream = nullptr;
    IWICBitmapEncoder* encoder = nullptr;
    IWICBitmapFrameEncode* frame = nullptr;
    IPropertyBag2* propertyBag = nullptr;

    try
    {
        if (FAILED(::CoCreateInstance(
                CLSID_WICImagingFactory,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&factory))))
        {
            throw std::runtime_error("Failed to create WIC imaging factory.");
        }

        if (FAILED(::CreateStreamOnHGlobal(nullptr, TRUE, &outputStream)))
        {
            throw std::runtime_error("Failed to create generated image stream.");
        }

        if (FAILED(factory->CreateEncoder(containerFormat, nullptr, &encoder)))
        {
            throw std::runtime_error("Failed to create generated image encoder.");
        }

        if (FAILED(encoder->Initialize(outputStream, WICBitmapEncoderNoCache)))
        {
            throw std::runtime_error("Failed to initialize generated image encoder.");
        }

        if (FAILED(encoder->CreateNewFrame(&frame, &propertyBag)))
        {
            throw std::runtime_error("Failed to create generated image frame.");
        }

        if (FAILED(frame->Initialize(propertyBag)))
        {
            throw std::runtime_error("Failed to initialize generated image frame.");
        }

        if (FAILED(frame->SetSize(width, height)))
        {
            throw std::runtime_error("Failed to set generated image size.");
        }

        WICPixelFormatGUID pixelFormat = GUID_WICPixelFormat32bppBGRA;
        if (FAILED(frame->SetPixelFormat(&pixelFormat)))
        {
            throw std::runtime_error("Failed to set generated image pixel format.");
        }

        std::vector<std::byte> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U);
        for (std::size_t index = 0; index < pixels.size(); index += 4)
        {
            pixels[index + 0] = std::byte{0x30};
            pixels[index + 1] = std::byte{0x60};
            pixels[index + 2] = std::byte{0x90};
            pixels[index + 3] = std::byte{0xFF};
        }

        if (FAILED(frame->WritePixels(
                height,
                width * 4U,
                static_cast<UINT>(pixels.size()),
                reinterpret_cast<BYTE*>(pixels.data()))))
        {
            throw std::runtime_error("Failed to write generated image pixels.");
        }

        if (FAILED(frame->Commit()) || FAILED(encoder->Commit()))
        {
            throw std::runtime_error("Failed to finalize generated image.");
        }

        auto bytes = ReadStreamBytes(outputStream);
        SafeRelease(propertyBag);
        SafeRelease(frame);
        SafeRelease(encoder);
        SafeRelease(outputStream);
        SafeRelease(factory);
        return bytes;
    }
    catch (...)
    {
        SafeRelease(propertyBag);
        SafeRelease(frame);
        SafeRelease(encoder);
        SafeRelease(outputStream);
        SafeRelease(factory);
        throw;
    }
}

#endif

} // namespace

TEST_CASE("WIC cover processor returns unsupported for non-JPEG and non-PNG covers", "[cover-processing]")
{
    const Librova::CoverProcessingWic::CWicCoverImageProcessor processor;

    const auto result = processor.ProcessForManagedStorage({
        .Cover = {
            .Extension = "gif",
            .Bytes = {std::byte{0x01}, std::byte{0x02}}
        },
        .MaxWidth = 512,
        .MaxHeight = 768
    });

    REQUIRE(result.Status == Librova::Domain::ECoverProcessingStatus::Unsupported);
}

#ifdef _WIN32

TEST_CASE("WIC cover processor leaves smaller PNG covers unchanged", "[cover-processing]")
{
    const Librova::CoverProcessingWic::CWicCoverImageProcessor processor;
    const auto pngBytes = CreateEncodedImage(GUID_ContainerFormatPng, 32, 48);

    const auto result = processor.ProcessForManagedStorage({
        .Cover = {
            .Extension = "png",
            .Bytes = pngBytes
        },
        .MaxWidth = 512,
        .MaxHeight = 768
    });

    REQUIRE(result.Status == Librova::Domain::ECoverProcessingStatus::Unchanged);
    REQUIRE(result.PixelWidth == 32);
    REQUIRE(result.PixelHeight == 48);
    REQUIRE_FALSE(result.WasResized);
    REQUIRE(result.Cover.Extension == "png");
    REQUIRE(result.Cover.Bytes == pngBytes);
}

TEST_CASE("WIC cover processor downscales oversized JPEG covers within bounds", "[cover-processing]")
{
    const Librova::CoverProcessingWic::CWicCoverImageProcessor processor;
    const auto jpegBytes = CreateEncodedImage(GUID_ContainerFormatJpeg, 1024, 1536);

    const auto result = processor.ProcessForManagedStorage({
        .Cover = {
            .Extension = "jpg",
            .Bytes = jpegBytes
        },
        .MaxWidth = 512,
        .MaxHeight = 768
    });

    REQUIRE(result.Status == Librova::Domain::ECoverProcessingStatus::Processed);
    REQUIRE(result.WasResized);
    REQUIRE(result.PixelWidth == 512);
    REQUIRE(result.PixelHeight == 768);
    REQUIRE(result.Cover.Extension == "jpg");
    REQUIRE_FALSE(result.Cover.Bytes.empty());
}

#endif
