// Define stb implementations exactly once in this translation unit.
// All stb headers are included here; other TUs include them without the
// IMPLEMENTATION defines to get only extern declarations resolved from this lib.

#define STBI_NO_STDIO
#define STBI_FAILURE_USERMSG
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define STBI_WRITE_NO_STDIO
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

// Use stb_image_resize2 (v2, sRGB-correct, Mitchell-Netravali default) when
// available; fall back to v1 for older vcpkg baselines.
#if __has_include(<stb_image_resize2.h>)
#    define STB_IMAGE_RESIZE_IMPLEMENTATION
#    include <stb_image_resize2.h>
#    define LIBROVA_STB_RESIZE_V2 1
#else
#    define STB_IMAGE_RESIZE_IMPLEMENTATION
#    include <stb_image_resize.h>
#    define LIBROVA_STB_RESIZE_V2 0
#endif

#include "CoverProcessingStb/StbCoverImageProcessor.hpp"

#include <climits>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int GJpegQuality = 85;

[[nodiscard]] std::pair<std::uint32_t, std::uint32_t> CalculateTargetSize(
    const std::uint32_t srcW, const std::uint32_t srcH,
    const std::uint32_t maxW, const std::uint32_t maxH)
{
    const double wRatio = static_cast<double>(maxW) / static_cast<double>(srcW);
    const double hRatio = static_cast<double>(maxH) / static_cast<double>(srcH);
    const double scale  = std::min(wRatio, hRatio);
    const auto dstW = static_cast<std::uint32_t>(std::max(1.0, std::round(static_cast<double>(srcW) * scale)));
    const auto dstH = static_cast<std::uint32_t>(std::max(1.0, std::round(static_cast<double>(srcH) * scale)));
    return {dstW, dstH};
}

void AppendToVector(void* ctx, void* data, int size)
{
    auto& buf = *static_cast<std::vector<std::byte>*>(ctx);
    const auto* src = static_cast<const std::byte*>(data);
    buf.insert(buf.end(), src, src + size);
}

[[nodiscard]] std::vector<stbi_uc> ResizeRgba(
    const stbi_uc* src, int srcW, int srcH, int dstW, int dstH)
{
    std::vector<stbi_uc> dst(static_cast<std::size_t>(dstW) * static_cast<std::size_t>(dstH) * 4u);

#if LIBROVA_STB_RESIZE_V2
    stbir_resize_uint8_srgb(src, srcW, srcH, 0, dst.data(), dstW, dstH, 0, STBIR_RGBA);
#else
    stbir_resize_uint8(src, srcW, srcH, 0, dst.data(), dstW, dstH, 0, 4);
#endif

    return dst;
}

// Composite RGBA pixels on a white background and pack to RGB (3 channels).
// When alpha == 255 (opaque), the result equals the original RGB values exactly.
[[nodiscard]] std::vector<stbi_uc> CompositeOnWhiteAndPackRgb(
    const stbi_uc* rgba, std::size_t pixelCount)
{
    std::vector<stbi_uc> rgb(pixelCount * 3u);
    for (std::size_t i = 0; i < pixelCount; ++i)
    {
        const std::uint32_t r = rgba[i * 4 + 0];
        const std::uint32_t g = rgba[i * 4 + 1];
        const std::uint32_t b = rgba[i * 4 + 2];
        const std::uint32_t a = rgba[i * 4 + 3];
        // Result = src * (a/255) + white * (1 - a/255)
        // Using integer arithmetic: (channel * a + 255 * (255 - a)) / 255
        rgb[i * 3 + 0] = static_cast<stbi_uc>((r * a + 255u * (255u - a)) / 255u);
        rgb[i * 3 + 1] = static_cast<stbi_uc>((g * a + 255u * (255u - a)) / 255u);
        rgb[i * 3 + 2] = static_cast<stbi_uc>((b * a + 255u * (255u - a)) / 255u);
    }
    return rgb;
}

} // namespace

namespace Librova::CoverProcessingStb {

Librova::Domain::SCoverProcessingResult CStbCoverImageProcessor::ProcessForManagedStorage(
    const Librova::Domain::SCoverProcessingRequest& request) const
{
    if (!request.IsValid())
    {
        return {
            .Status = Librova::Domain::ECoverProcessingStatus::Failed,
            .DiagnosticMessage = "Cover processing request must contain non-empty bytes and positive target bounds."
        };
    }

    // Guard against stb_image's int-sized length API.
    if (request.Cover.Bytes.size() > static_cast<std::size_t>(INT_MAX))
    {
        return {
            .Status = Librova::Domain::ECoverProcessingStatus::Failed,
            .DiagnosticMessage = "Cover image data too large: "
                + std::to_string(request.Cover.Bytes.size()) + " bytes"
        };
    }

    // Decode image to RGBA pixels regardless of source format.
    // stb_image handles JPEG (baseline + progressive), PNG, BMP, GIF (first
    // frame), TGA, PSD, PNM, HDR, PIC, and JPEG with unusual subsampling or
    // CMYK colour space — all cases that caused WIC to fail.
    int srcW     = 0;
    int srcH     = 0;
    int srcChans = 0; // original channel count; actual buffer is always RGBA
    stbi_uc* rawPixels = stbi_load_from_memory(
        reinterpret_cast<const stbi_uc*>(request.Cover.Bytes.data()),
        static_cast<int>(request.Cover.Bytes.size()),
        &srcW, &srcH, &srcChans,
        STBI_rgb_alpha); // always decode to RGBA

    if (rawPixels == nullptr)
    {
        const char* const stbReason = stbi_failure_reason(); // thread-local on MSVC
        return {
            .Status = Librova::Domain::ECoverProcessingStatus::Failed,
            .DiagnosticMessage = std::string("Failed to decode cover image: ")
                + (stbReason != nullptr ? stbReason : "unknown error")
        };
    }

    // RAII: free stb-allocated pixel buffer on scope exit.
    struct StbGuard
    {
        stbi_uc* data;
        ~StbGuard() { stbi_image_free(data); }
    } guard{rawPixels};

    // stb_image can theoretically return 0 dimensions for malformed images
    // even without returning nullptr — guard before any division.
    if (srcW <= 0 || srcH <= 0)
    {
        return {
            .Status = Librova::Domain::ECoverProcessingStatus::Failed,
            .DiagnosticMessage = "Decoded image has invalid dimensions: "
                + std::to_string(srcW) + "x" + std::to_string(srcH)
        };
    }

    const auto w = static_cast<std::uint32_t>(srcW);
    const auto h = static_cast<std::uint32_t>(srcH);

    const bool fitsInBounds = (w <= request.MaxWidth && h <= request.MaxHeight);

    // Fast path: source bytes are genuinely JPEG (magic bytes FF D8 FF) and
    // already within bounds — return them as-is to avoid a lossy re-encode.
    // We check the actual bytes rather than the extension field because the
    // extension may be wrong (e.g. a PNG embedded with content-type image/jpeg).
    const auto& coverBytes = request.Cover.Bytes;
    const bool isActuallyJpeg = coverBytes.size() >= 3
        && coverBytes[0] == std::byte{0xFF}
        && coverBytes[1] == std::byte{0xD8}
        && coverBytes[2] == std::byte{0xFF};

    if (isActuallyJpeg && request.PreserveSmallerImages && fitsInBounds)
    {
        return {
            .Status      = Librova::Domain::ECoverProcessingStatus::Unchanged,
            .Cover       = request.Cover,
            .PixelWidth  = w,
            .PixelHeight = h,
            .WasResized  = false
        };
    }

    // Determine output dimensions.
    auto dstW = w;
    auto dstH = h;
    bool wasResized = false;
    if (!fitsInBounds)
    {
        std::tie(dstW, dstH) = CalculateTargetSize(w, h, request.MaxWidth, request.MaxHeight);
        wasResized = true;
    }

    // Resize RGBA → RGBA if needed.
    std::vector<stbi_uc> resizedBuf;
    const stbi_uc* encodeInput = rawPixels;
    if (wasResized)
    {
        resizedBuf  = ResizeRgba(rawPixels, srcW, srcH, static_cast<int>(dstW), static_cast<int>(dstH));
        encodeInput = resizedBuf.data();
    }

    // Composite alpha on white and pack to RGB for JPEG encoding.
    const std::size_t pixelCount = static_cast<std::size_t>(dstW) * static_cast<std::size_t>(dstH);
    const auto rgbBuf = CompositeOnWhiteAndPackRgb(encodeInput, pixelCount);

    // Encode to JPEG into an in-memory buffer.
    std::vector<std::byte> jpegBytes;
    jpegBytes.reserve(pixelCount); // rough initial reservation
    const int ok = stbi_write_jpg_to_func(
        AppendToVector, &jpegBytes,
        static_cast<int>(dstW), static_cast<int>(dstH),
        3, rgbBuf.data(), GJpegQuality);

    if (ok == 0 || jpegBytes.empty())
    {
        return {
            .Status = Librova::Domain::ECoverProcessingStatus::Failed,
            .DiagnosticMessage = "JPEG encoding failed for "
                + std::to_string(dstW) + "x" + std::to_string(dstH)
                + " image (source " + std::to_string(srcW) + "x" + std::to_string(srcH)
                + ", channels=" + std::to_string(srcChans) + ")."
        };
    }

    return {
        .Status      = Librova::Domain::ECoverProcessingStatus::Processed,
        .Cover       = {.Extension = "jpg", .Bytes = std::move(jpegBytes)},
        .PixelWidth  = dstW,
        .PixelHeight = dstH,
        .WasResized  = wasResized
    };
}

} // namespace Librova::CoverProcessingStb
