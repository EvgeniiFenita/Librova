#include "CoverProcessingWic/WicCoverImageProcessor.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <objbase.h>
#include <propidl.h>
#include <wincodec.h>
#endif

namespace {

[[nodiscard]] std::string NormalizeExtension(std::string extension)
{
    if (!extension.empty() && extension.front() == '.')
    {
        extension.erase(extension.begin());
    }

    std::ranges::transform(extension, extension.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    return extension;
}

[[nodiscard]] std::pair<std::uint32_t, std::uint32_t> CalculateTargetSize(
    const std::uint32_t sourceWidth,
    const std::uint32_t sourceHeight,
    const std::uint32_t maxWidth,
    const std::uint32_t maxHeight)
{
    if (sourceWidth == 0 || sourceHeight == 0)
    {
        throw std::invalid_argument("Cover image dimensions must be non-zero.");
    }

    const double widthRatio = static_cast<double>(maxWidth) / static_cast<double>(sourceWidth);
    const double heightRatio = static_cast<double>(maxHeight) / static_cast<double>(sourceHeight);
    const double scale = std::min(widthRatio, heightRatio);

    const auto targetWidth = static_cast<std::uint32_t>(std::max(1.0, std::round(static_cast<double>(sourceWidth) * scale)));
    const auto targetHeight = static_cast<std::uint32_t>(std::max(1.0, std::round(static_cast<double>(sourceHeight) * scale)));
    return {targetWidth, targetHeight};
}

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

        if (result == RPC_E_CHANGED_MODE)
        {
            return;
        }

        throw std::runtime_error("Failed to initialize COM for cover processing.");
    }

    ~CScopedCoInitialize()
    {
        if (m_ShouldUninitialize)
        {
            ::CoUninitialize();
        }
    }

    CScopedCoInitialize(const CScopedCoInitialize&) = delete;
    CScopedCoInitialize& operator=(const CScopedCoInitialize&) = delete;

private:
    bool m_ShouldUninitialize = false;
};

[[nodiscard]] CLSID ResolveEncoderClsid(const std::string_view normalizedExtension)
{
    if (normalizedExtension == "jpg" || normalizedExtension == "jpeg")
    {
        return GUID_ContainerFormatJpeg;
    }

    if (normalizedExtension == "png")
    {
        return GUID_ContainerFormatPng;
    }

    throw std::invalid_argument("Unsupported cover image format for WIC processing.");
}

void SetEncoderOptions(
    IPropertyBag2* propertyBag,
    const std::string_view normalizedExtension)
{
    if (propertyBag == nullptr)
    {
        return;
    }

    if (normalizedExtension != "jpg" && normalizedExtension != "jpeg")
    {
        return;
    }

    PROPBAG2 option{};
    option.pstrName = const_cast<LPOLESTR>(L"ImageQuality");

    VARIANT value;
    ::VariantInit(&value);
    value.vt = VT_R4;
    value.fltVal = 0.85f;

    const HRESULT result = propertyBag->Write(1, &option, &value);
    ::VariantClear(&value);

    if (FAILED(result))
    {
        throw std::runtime_error("Failed to configure JPEG cover quality.");
    }
}

[[nodiscard]] std::vector<std::byte> ReadStreamBytes(IStream* stream)
{
    HGLOBAL globalHandle = nullptr;
    const HRESULT handleResult = ::GetHGlobalFromStream(stream, &globalHandle);
    if (FAILED(handleResult) || globalHandle == nullptr)
    {
        throw std::runtime_error("Failed to access encoded cover image buffer.");
    }

    const SIZE_T size = ::GlobalSize(globalHandle);
    if (size == 0)
    {
        return {};
    }

    void* lockedData = ::GlobalLock(globalHandle);
    if (lockedData == nullptr)
    {
        throw std::runtime_error("Failed to lock encoded cover image buffer.");
    }

    std::vector<std::byte> bytes(size);
    std::memcpy(bytes.data(), lockedData, size);
    ::GlobalUnlock(globalHandle);
    return bytes;
}

[[nodiscard]] Librova::Domain::SCoverProcessingResult ProcessWithWic(
    const Librova::Domain::SCoverProcessingRequest& request)
{
    CScopedCoInitialize coInitialize;

    IWICImagingFactory* factory = nullptr;
    IStream* inputStream = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICBitmapScaler* scaler = nullptr;
    IWICFormatConverter* converter = nullptr;
    IStream* outputStream = nullptr;
    IWICBitmapEncoder* encoder = nullptr;
    IWICBitmapFrameEncode* outputFrame = nullptr;
    IPropertyBag2* propertyBag = nullptr;

    try
    {
        const HRESULT factoryResult = ::CoCreateInstance(
            CLSID_WICImagingFactory,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&factory));
        if (FAILED(factoryResult))
        {
            throw std::runtime_error("Failed to create WIC imaging factory.");
        }

        const HRESULT inputStreamResult = ::CreateStreamOnHGlobal(nullptr, TRUE, &inputStream);
        if (FAILED(inputStreamResult))
        {
            throw std::runtime_error("Failed to create input stream for cover processing.");
        }

        ULONG writtenBytes = 0;
        const auto* rawInput = reinterpret_cast<const void*>(request.Cover.Bytes.data());
        const auto byteCount = static_cast<ULONG>(request.Cover.Bytes.size());
        const HRESULT writeResult = inputStream->Write(rawInput, byteCount, &writtenBytes);
        if (FAILED(writeResult) || writtenBytes != byteCount)
        {
            throw std::runtime_error("Failed to load cover image bytes into the WIC stream.");
        }

        LARGE_INTEGER rewind{};
        const HRESULT seekResult = inputStream->Seek(rewind, STREAM_SEEK_SET, nullptr);
        if (FAILED(seekResult))
        {
            throw std::runtime_error("Failed to rewind the cover input stream.");
        }

        const HRESULT decoderResult = factory->CreateDecoderFromStream(
            inputStream,
            nullptr,
            WICDecodeMetadataCacheOnLoad,
            &decoder);
        if (FAILED(decoderResult))
        {
            throw std::runtime_error("Failed to decode the cover image.");
        }

        const HRESULT frameResult = decoder->GetFrame(0, &frame);
        if (FAILED(frameResult))
        {
            throw std::runtime_error("Failed to open the decoded cover frame.");
        }

        UINT sourceWidth = 0;
        UINT sourceHeight = 0;
        const HRESULT sizeResult = frame->GetSize(&sourceWidth, &sourceHeight);
        if (FAILED(sizeResult) || sourceWidth == 0 || sourceHeight == 0)
        {
            throw std::runtime_error("Failed to read decoded cover image dimensions.");
        }

        Librova::Domain::SCoverProcessingResult result;
        result.PixelWidth = sourceWidth;
        result.PixelHeight = sourceHeight;

        if (request.PreserveSmallerImages
            && sourceWidth <= request.MaxWidth
            && sourceHeight <= request.MaxHeight)
        {
            result.Status = Librova::Domain::ECoverProcessingStatus::Unchanged;
            result.Cover = request.Cover;
            return result;
        }

        const auto [targetWidth, targetHeight] = CalculateTargetSize(
            sourceWidth,
            sourceHeight,
            request.MaxWidth,
            request.MaxHeight);

        const HRESULT scalerResult = factory->CreateBitmapScaler(&scaler);
        if (FAILED(scalerResult))
        {
            throw std::runtime_error("Failed to create WIC bitmap scaler.");
        }

        const HRESULT scalerInitResult = scaler->Initialize(frame, targetWidth, targetHeight, WICBitmapInterpolationModeFant);
        if (FAILED(scalerInitResult))
        {
            throw std::runtime_error("Failed to initialize the WIC bitmap scaler.");
        }

        const HRESULT converterResult = factory->CreateFormatConverter(&converter);
        if (FAILED(converterResult))
        {
            throw std::runtime_error("Failed to create WIC format converter.");
        }

        const HRESULT converterInitResult = converter->Initialize(
            scaler,
            GUID_WICPixelFormat32bppBGRA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom);
        if (FAILED(converterInitResult))
        {
            throw std::runtime_error("Failed to initialize the WIC format converter.");
        }

        const HRESULT outputStreamResult = ::CreateStreamOnHGlobal(nullptr, TRUE, &outputStream);
        if (FAILED(outputStreamResult))
        {
            throw std::runtime_error("Failed to create output stream for cover processing.");
        }

        const std::string normalizedExtension = NormalizeExtension(request.Cover.Extension);
        const CLSID encoderClsid = ResolveEncoderClsid(normalizedExtension);
        const HRESULT encoderResult = factory->CreateEncoder(encoderClsid, nullptr, &encoder);
        if (FAILED(encoderResult))
        {
            throw std::runtime_error("Failed to create the WIC cover encoder.");
        }

        const HRESULT encoderInitResult = encoder->Initialize(outputStream, WICBitmapEncoderNoCache);
        if (FAILED(encoderInitResult))
        {
            throw std::runtime_error("Failed to initialize the WIC cover encoder.");
        }

        const HRESULT outputFrameResult = encoder->CreateNewFrame(&outputFrame, &propertyBag);
        if (FAILED(outputFrameResult))
        {
            throw std::runtime_error("Failed to create the encoded cover frame.");
        }

        SetEncoderOptions(propertyBag, normalizedExtension);

        const HRESULT outputFrameInitResult = outputFrame->Initialize(propertyBag);
        if (FAILED(outputFrameInitResult))
        {
            throw std::runtime_error("Failed to initialize the encoded cover frame.");
        }

        const HRESULT outputFrameSizeResult = outputFrame->SetSize(targetWidth, targetHeight);
        if (FAILED(outputFrameSizeResult))
        {
            throw std::runtime_error("Failed to set the encoded cover size.");
        }

        WICPixelFormatGUID pixelFormat = GUID_WICPixelFormat32bppBGRA;
        const HRESULT pixelFormatResult = outputFrame->SetPixelFormat(&pixelFormat);
        if (FAILED(pixelFormatResult))
        {
            throw std::runtime_error("Failed to configure the encoded cover pixel format.");
        }

        const HRESULT writeSourceResult = outputFrame->WriteSource(converter, nullptr);
        if (FAILED(writeSourceResult))
        {
            throw std::runtime_error("Failed to write the resized cover image.");
        }

        const HRESULT frameCommitResult = outputFrame->Commit();
        if (FAILED(frameCommitResult))
        {
            throw std::runtime_error("Failed to finalize the resized cover frame.");
        }

        const HRESULT encoderCommitResult = encoder->Commit();
        if (FAILED(encoderCommitResult))
        {
            throw std::runtime_error("Failed to finalize the resized cover image.");
        }

        result.Status = Librova::Domain::ECoverProcessingStatus::Processed;
        result.WasResized = true;
        result.PixelWidth = targetWidth;
        result.PixelHeight = targetHeight;
        result.Cover = {
            .Extension = normalizedExtension,
            .Bytes = ReadStreamBytes(outputStream)
        };

        if (result.Cover.IsEmpty())
        {
            throw std::runtime_error("The resized cover image encoder returned no bytes.");
        }

        SafeRelease(propertyBag);
        SafeRelease(outputFrame);
        SafeRelease(encoder);
        SafeRelease(outputStream);
        SafeRelease(converter);
        SafeRelease(scaler);
        SafeRelease(frame);
        SafeRelease(decoder);
        SafeRelease(inputStream);
        SafeRelease(factory);
        return result;
    }
    catch (...)
    {
        SafeRelease(propertyBag);
        SafeRelease(outputFrame);
        SafeRelease(encoder);
        SafeRelease(outputStream);
        SafeRelease(converter);
        SafeRelease(scaler);
        SafeRelease(frame);
        SafeRelease(decoder);
        SafeRelease(inputStream);
        SafeRelease(factory);
        throw;
    }
}

#endif

} // namespace

namespace Librova::CoverProcessingWic {

Librova::Domain::SCoverProcessingResult CWicCoverImageProcessor::ProcessForManagedStorage(
    const Librova::Domain::SCoverProcessingRequest& request) const
{
    if (!request.IsValid())
    {
        return {
            .Status = Librova::Domain::ECoverProcessingStatus::Failed,
            .DiagnosticMessage = "Cover processing request must contain non-empty bytes and positive target bounds."
        };
    }

    const std::string normalizedExtension = NormalizeExtension(request.Cover.Extension);
    if (normalizedExtension != "jpg"
        && normalizedExtension != "jpeg"
        && normalizedExtension != "png")
    {
        return {
            .Status = Librova::Domain::ECoverProcessingStatus::Unsupported,
            .DiagnosticMessage = "Cover processing supports only JPEG and PNG input."
        };
    }

#ifdef _WIN32
    try
    {
        auto result = ProcessWithWic(request);
        if (result.Cover.Extension.empty() && result.HasOutputCover())
        {
            result.Cover.Extension = normalizedExtension;
        }

        return result;
    }
    catch (const std::exception& error)
    {
        return {
            .Status = Librova::Domain::ECoverProcessingStatus::Failed,
            .DiagnosticMessage = error.what()
        };
    }
#else
    (void)request;
    return {
        .Status = Librova::Domain::ECoverProcessingStatus::Unsupported,
        .DiagnosticMessage = "WIC cover processing is available only on Windows."
    };
#endif
}

} // namespace Librova::CoverProcessingWic
