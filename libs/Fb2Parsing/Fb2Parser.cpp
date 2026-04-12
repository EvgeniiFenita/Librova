#include "Fb2Parsing/Fb2Parser.hpp"
#include "Fb2Parsing/Fb2GenreMapper.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <pugixml.hpp>

#include "Logging/Logging.hpp"
#include "Unicode/UnicodeConversion.hpp"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace Librova::Fb2Parsing {
namespace {

std::string ReplaceEncodingDeclaration(std::string text)
{
    const std::size_t declarationEnd = text.find("?>");
    if (declarationEnd == std::string::npos)
    {
        return text;
    }

    std::string declaration = text.substr(0, declarationEnd);
    std::string lowerDeclaration = declaration;
    std::transform(lowerDeclaration.begin(), lowerDeclaration.end(), lowerDeclaration.begin(), [](const unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });

    const auto replaceIfPresent = [&declaration, &lowerDeclaration](const std::string_view from, const std::string_view to) {
        const std::size_t index = lowerDeclaration.find(from);
        if (index != std::string::npos)
        {
            declaration.replace(index, from.size(), to);
            lowerDeclaration.replace(index, from.size(), to);
        }
    };

    replaceIfPresent(R"(encoding="windows-1251")", R"(encoding="utf-8")");
    replaceIfPresent(R"(encoding='windows-1251')", R"(encoding='utf-8')");
    replaceIfPresent(R"(encoding="cp1251")", R"(encoding="utf-8")");
    replaceIfPresent(R"(encoding='cp1251')", R"(encoding='utf-8')");

    text.replace(0, declarationEnd, declaration);
    return text;
}

[[nodiscard]] std::uintmax_t TryGetFileSize(const std::filesystem::path& filePath) noexcept
{
    std::error_code errorCode;
    const auto fileSize = std::filesystem::file_size(filePath, errorCode);
    return errorCode ? 0 : fileSize;
}

[[nodiscard]] std::string Trim(const std::string_view value);

[[nodiscard]] std::string TruncateUtf8(std::string_view value, const std::size_t maxBytes)
{
    if (value.size() <= maxBytes)
    {
        return std::string{value};
    }

    std::size_t index = 0;
    std::size_t lastValidEnd = 0;

    while (index < value.size() && index < maxBytes)
    {
        const unsigned char lead = static_cast<unsigned char>(value[index]);
        std::size_t sequenceLength = 1;

        if ((lead & 0x80u) == 0)
        {
            sequenceLength = 1;
        }
        else if ((lead & 0xE0u) == 0xC0u)
        {
            sequenceLength = 2;
        }
        else if ((lead & 0xF0u) == 0xE0u)
        {
            sequenceLength = 3;
        }
        else if ((lead & 0xF8u) == 0xF0u)
        {
            sequenceLength = 4;
        }
        else
        {
            break;
        }

        if (index + sequenceLength > value.size() || index + sequenceLength > maxBytes)
        {
            break;
        }

        bool hasValidContinuationBytes = true;
        for (std::size_t offset = 1; offset < sequenceLength; ++offset)
        {
            const unsigned char continuation = static_cast<unsigned char>(value[index + offset]);
            if ((continuation & 0xC0u) != 0x80u)
            {
                hasValidContinuationBytes = false;
                break;
            }
        }

        if (!hasValidContinuationBytes)
        {
            break;
        }

        lastValidEnd = index + sequenceLength;
        index += sequenceLength;
    }

    if (lastValidEnd == 0)
    {
        return {};
    }

    return std::string{value.substr(0, lastValidEnd)};
}

[[nodiscard]] std::string CompactPreview(std::string text, const std::size_t maxBytes = 320)
{
    for (char& value : text)
    {
        if (value == '\r' || value == '\n' || value == '\t')
        {
            value = ' ';
        }
    }

    bool previousWasWhitespace = false;
    text.erase(
        std::remove_if(text.begin(), text.end(), [&previousWasWhitespace](const char value) {
            const bool isWhitespace = std::isspace(static_cast<unsigned char>(value)) != 0;
            if (!isWhitespace)
            {
                previousWasWhitespace = false;
                return false;
            }

            if (previousWasWhitespace)
            {
                return true;
            }

            previousWasWhitespace = true;
            return false;
        }),
        text.end());

    text = Trim(text);
    if (text.empty())
    {
        return "<empty>";
    }

    if (text.size() > maxBytes)
    {
        text = TruncateUtf8(text, maxBytes);
        text.append("...");
    }

    return text;
}

[[nodiscard]] std::string ReadTextFile(const std::filesystem::path& filePath)
{
    std::ifstream input(filePath, std::ios::binary);

    if (!input)
    {
        throw std::runtime_error("Failed to open FB2 file: " + Librova::Unicode::PathToUtf8(filePath));
    }

    std::string text{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    const std::string pathUtf8 = Librova::Unicode::PathToUtf8(filePath);

    // Step 1 — strip UTF-8 BOM (EF BB BF) so pugixml does not choke on the
    // XML declaration ("Error parsing document declaration" on BOM files).
    if (text.size() >= 3
        && static_cast<unsigned char>(text[0]) == 0xEFu
        && static_cast<unsigned char>(text[1]) == 0xBBu
        && static_cast<unsigned char>(text[2]) == 0xBFu)
    {
        text.erase(0, 3);
        if (Librova::Logging::CLogging::IsInitialized())
        {
            Librova::Logging::Debug("FB2 UTF-8 BOM stripped: {}", pathUtf8);
        }
    }

#if defined(_WIN32)
    const auto lowerPrefix = [](std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return value;
    };

    const auto prefixLength = std::min<std::size_t>(text.size(), 512);
    const std::string prefix = lowerPrefix(text.substr(0, prefixLength));

    // Step 2 — explicit CP1251 declaration: file says encoding="windows-1251" or "cp1251".
    if (prefix.find("encoding=\"windows-1251\"") != std::string::npos
        || prefix.find("encoding='windows-1251'") != std::string::npos
        || prefix.find("encoding=\"cp1251\"") != std::string::npos
        || prefix.find("encoding='cp1251'") != std::string::npos)
    {
        if (Librova::Logging::CLogging::IsInitialized())
        {
            Librova::Logging::Debug("FB2 explicit CP1251 declaration — converting to UTF-8: {}", pathUtf8);
        }
        const std::string utf8Text = Librova::Unicode::CodePageToUtf8(
            text,
            1251,
            "Failed to decode FB2 file from windows-1251.");
        return ReplaceEncodingDeclaration(std::move(utf8Text));
    }

    // Step 3 — CP1251 fallback: no explicit declaration but the byte stream is
    // not valid UTF-8 (misdeclared or encoding-less files from older tools).
    if (!Librova::Unicode::IsValidUtf8(text))
    {
        if (Librova::Logging::CLogging::IsInitialized())
        {
            Librova::Logging::Warn(
                "FB2 file is not valid UTF-8 and has no CP1251 declaration — "
                "applying CP1251 fallback: {}",
                pathUtf8);
        }
        const std::string utf8Text = Librova::Unicode::CodePageToUtf8(
            text,
            1251,
            "Failed to decode misdeclared FB2 file from windows-1251 (fallback).");
        return ReplaceEncodingDeclaration(std::move(utf8Text));
    }
#endif

    return text;
}

// Tries to extract the <description> block from a FB2 file that failed strict XML parsing.
// lib.rus.ec files frequently contain malformed body content (unescaped '<...>' ellipsis,
// '<:>' separators, embedded binary with bare '<') while the <description> section is valid.
// Since <description> always precedes <body>, we can salvage metadata by parsing it in isolation.
[[nodiscard]] std::optional<pugi::xml_document> TryParseDescriptionOnly(const std::string& text)
{
    const std::string_view view{text};

    // Find the opening <description tag (may have attributes or namespace prefixes)
    const std::size_t descOpen = view.find("<description");
    if (descOpen == std::string_view::npos)
    {
        return std::nullopt;
    }

    const std::size_t descClose = view.find("</description>", descOpen);
    if (descClose == std::string_view::npos)
    {
        return std::nullopt;
    }

    const std::size_t descEnd = descClose + std::string_view{"</description>"}.size();
    const std::string descXml =
        "<FictionBook xmlns=\"http://www.gribuser.ru/xml/fictionbook/2.0\">"
        + std::string{view.substr(descOpen, descEnd - descOpen)}
        + "</FictionBook>";

    pugi::xml_document doc;
    const pugi::xml_parse_result parseResult = doc.load_buffer(descXml.data(), descXml.size());

    if (!parseResult && !doc.first_child())
    {
        return std::nullopt;
    }

    return doc;
}

[[nodiscard]] pugi::xml_document ParseXml(const std::string& text, const std::filesystem::path& filePath)
{
    pugi::xml_document document;
    const pugi::xml_parse_result result = document.load_buffer(text.data(), text.size());

    if (result)
    {
        return document;
    }

    // Strict parse failed. Try extracting just <description>...</description> — malformed body
    // content (unescaped '<...>' ellipsis, '<:>' separators, etc.) causes 100% of real-world
    // lib.rus.ec parse failures while the description section itself is valid.
    if (std::optional<pugi::xml_document> recovered = TryParseDescriptionOnly(text))
    {
        if (Librova::Logging::CLogging::IsInitialized())
        {
            Librova::Logging::Warn(
                "FB2 XML strict parse failed, recovered metadata from <description> block: "
                "file='{}' error='{}' [size_bytes={}]",
                Librova::Unicode::PathToUtf8(filePath),
                result.description(),
                TryGetFileSize(filePath));
        }
        return std::move(*recovered);
    }

    throw std::runtime_error(
        "Failed to parse FB2 XML from " + Librova::Unicode::PathToUtf8(filePath)
        + ": " + result.description()
        + " [size_bytes=" + std::to_string(TryGetFileSize(filePath))
        + ", xml_preview=\"" + CompactPreview(text) + "\"]");
}

[[nodiscard]] std::string Trim(const std::string_view value)
{
    std::size_t start = 0;
    std::size_t end = value.size();

    while (start < end && std::isspace(static_cast<unsigned char>(value[start])))
    {
        ++start;
    }

    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])))
    {
        --end;
    }

    return std::string{value.substr(start, end - start)};
}

[[nodiscard]] bool MatchesLocalName(const pugi::xml_node& node, const std::string_view localName)
{
    const std::string_view qualifiedName = node.name();
    const std::size_t separatorIndex = qualifiedName.find(':');
    const std::string_view currentLocalName = separatorIndex == std::string_view::npos
        ? qualifiedName
        : qualifiedName.substr(separatorIndex + 1);
    return currentLocalName == localName;
}

[[nodiscard]] pugi::xml_node FindFirstChildByLocalName(const pugi::xml_node& parent, const std::string_view localName)
{
    for (const pugi::xml_node childNode : parent.children())
    {
        if (MatchesLocalName(childNode, localName))
        {
            return childNode;
        }
    }

    return {};
}

[[nodiscard]] std::string JoinAuthorName(const pugi::xml_node& authorNode)
{
    std::vector<std::string> parts;

    for (const char* partName : {"first-name", "middle-name", "last-name", "nickname"})
    {
        const std::string value = Trim(FindFirstChildByLocalName(authorNode, partName).text().as_string());

        if (!value.empty())
        {
            parts.push_back(value);
        }
    }

    std::string author;

    for (std::size_t index = 0; index < parts.size(); ++index)
    {
        if (index > 0)
        {
            author.push_back(' ');
        }

        author.append(parts[index]);
    }

    return author;
}

[[nodiscard]] std::size_t CountAuthorNodes(const pugi::xml_node& parent)
{
    std::size_t count = 0;

    for (const pugi::xml_node childNode : parent.children())
    {
        if (MatchesLocalName(childNode, "author"))
        {
            ++count;
        }
    }

    return count;
}

[[nodiscard]] std::string BuildNodePreview(const pugi::xml_node& node)
{
    if (!node)
    {
        return "<missing>";
    }

    std::ostringstream stream;
    node.print(stream, "", pugi::format_raw);
    return CompactPreview(stream.str());
}

[[nodiscard]] std::optional<std::string> TryReadTextChild(const pugi::xml_node& parent, const char* childName)
{
    const pugi::xml_node node = FindFirstChildByLocalName(parent, childName);
    const std::string value = Trim(node.text().as_string());

    if (value.empty())
    {
        return std::nullopt;
    }

    return value;
}

[[nodiscard]] std::optional<std::string> TryReadNodeText(const pugi::xml_node& node)
{
    const std::string value = Trim(node.text().as_string());

    if (value.empty())
    {
        return std::nullopt;
    }

    return value;
}

void AppendNonEmptyNodeText(std::vector<std::string>& values, const pugi::xml_node& node)
{
    const std::optional<std::string> value = TryReadNodeText(node);
    if (value.has_value())
    {
        values.push_back(*value);
    }
}

[[nodiscard]] std::string RequireTextChild(const pugi::xml_node& parent, const char* childName, const char* errorLabel)
{
    const std::optional<std::string> value = TryReadTextChild(parent, childName);

    if (!value.has_value())
    {
        throw std::runtime_error(std::string{"Missing required FB2 node: "} + errorLabel);
    }

    return *value;
}

template <typename TValue>
[[nodiscard]] std::optional<TValue> TryParseNumber(const std::string_view value) noexcept
{
    TValue parsedValue{};
    const char* begin = value.data();
    const char* end = value.data() + value.size();
    const auto [parseEnd, errorCode] = std::from_chars(begin, end, parsedValue);

    if (errorCode != std::errc{} || parseEnd != end)
    {
        return std::nullopt;
    }

    return parsedValue;
}

[[nodiscard]] std::string ReadAnnotationText(const pugi::xml_node& annotationNode)
{
    std::string description;

    for (const pugi::xml_node childNode : annotationNode.children())
    {
        if (childNode.type() != pugi::node_element)
        {
            continue;
        }

        const std::string text = Trim(childNode.text().as_string());

        if (text.empty())
        {
            continue;
        }

        if (!description.empty())
        {
            description.push_back('\n');
        }

        description.append(text);
    }

    return description;
}

[[nodiscard]] std::string NormalizeExtension(std::string extension)
{
    if (!extension.empty() && extension.front() == '.')
    {
        extension.erase(extension.begin());
    }

    std::transform(extension.begin(), extension.end(), extension.begin(), [](const unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });

    return extension;
}

[[nodiscard]] std::string MimeTypeToExtension(const std::string_view mimeType)
{
    if (mimeType == "image/jpeg" || mimeType == "image/jpg")
    {
        return "jpg";
    }

    if (mimeType == "image/png")
    {
        return "png";
    }

    if (mimeType == "image/gif")
    {
        return "gif";
    }

    return {};
}

[[nodiscard]] std::vector<std::byte> DecodeBase64(const std::string_view encodedValue)
{
    const auto DecodeCharacter = [](const char value) -> std::uint8_t {
        if (value >= 'A' && value <= 'Z')
        {
            return static_cast<std::uint8_t>(value - 'A');
        }

        if (value >= 'a' && value <= 'z')
        {
            return static_cast<std::uint8_t>(value - 'a' + 26);
        }

        if (value >= '0' && value <= '9')
        {
            return static_cast<std::uint8_t>(value - '0' + 52);
        }

        if (value == '+')
        {
            return 62;
        }

        if (value == '/')
        {
            return 63;
        }

        throw std::runtime_error("Invalid base64 character in FB2 binary data.");
    };

    std::string sanitized;
    sanitized.reserve(encodedValue.size());

    for (const char value : encodedValue)
    {
        if (!std::isspace(static_cast<unsigned char>(value)))
        {
            sanitized.push_back(value);
        }
    }

    if (sanitized.empty())
    {
        return {};
    }

    if (sanitized.size() % 4 != 0)
    {
        throw std::runtime_error("Invalid base64 length in FB2 binary data.");
    }

    std::vector<std::byte> bytes;
    bytes.reserve((sanitized.size() * 3) / 4);

    for (std::size_t index = 0; index < sanitized.size(); index += 4)
    {
        const char a = sanitized[index];
        const char b = sanitized[index + 1];
        const char c = sanitized[index + 2];
        const char d = sanitized[index + 3];

        const std::uint32_t block =
            (static_cast<std::uint32_t>(DecodeCharacter(a)) << 18) |
            (static_cast<std::uint32_t>(DecodeCharacter(b)) << 12) |
            ((c == '=' ? 0u : static_cast<std::uint32_t>(DecodeCharacter(c))) << 6) |
            (d == '=' ? 0u : static_cast<std::uint32_t>(DecodeCharacter(d)));

        bytes.push_back(static_cast<std::byte>((block >> 16) & 0xFF));

        if (c != '=')
        {
            bytes.push_back(static_cast<std::byte>((block >> 8) & 0xFF));
        }

        if (d != '=')
        {
            bytes.push_back(static_cast<std::byte>(block & 0xFF));
        }
    }

    return bytes;
}

[[nodiscard]] std::optional<std::string> TryGetCoverBinaryId(const pugi::xml_node& titleInfoNode)
{
    const pugi::xml_node imageNode = FindFirstChildByLocalName(FindFirstChildByLocalName(titleInfoNode, "coverpage"), "image");

    if (!imageNode)
    {
        return std::nullopt;
    }

    for (const pugi::xml_attribute attribute : imageNode.attributes())
    {
        const std::string_view attributeName = attribute.name();

        if (attributeName == "l:href" || attributeName == "href")
        {
            std::string value = attribute.as_string();

            if (!value.empty() && value.front() == '#')
            {
                value.erase(value.begin());
            }

            if (!value.empty())
            {
                return value;
            }
        }
    }

    return std::nullopt;
}

[[nodiscard]] const pugi::xml_node FindBinaryById(const pugi::xml_node& rootNode, const std::string_view id)
{
    for (const pugi::xml_node binaryNode : rootNode.children())
    {
        if (MatchesLocalName(binaryNode, "binary") && std::string_view{binaryNode.attribute("id").as_string()} == id)
        {
            return binaryNode;
        }
    }

    return {};
}

} // namespace

bool CFb2Parser::CanParse(const Librova::Domain::EBookFormat format) const
{
    return format == Librova::Domain::EBookFormat::Fb2;
}

Librova::Domain::SParsedBook CFb2Parser::Parse(const std::filesystem::path& filePath) const
{
    const std::string text = ReadTextFile(filePath);
    const pugi::xml_document document = ParseXml(text, filePath);
    const pugi::xml_node rootNode = FindFirstChildByLocalName(document, "FictionBook");
    const pugi::xml_node descriptionNode = FindFirstChildByLocalName(rootNode, "description");
    const pugi::xml_node titleInfoNode = FindFirstChildByLocalName(descriptionNode, "title-info");

    if (!rootNode || !titleInfoNode)
    {
        throw std::runtime_error("FB2 document is missing required description/title-info nodes.");
    }

    Librova::Domain::SParsedBook parsedBook;
    parsedBook.SourceFormat = Librova::Domain::EBookFormat::Fb2;
    const std::optional<std::string> title = TryReadNodeText(FindFirstChildByLocalName(titleInfoNode, "book-title"));

    if (!title.has_value())
    {
        throw std::runtime_error("Missing required FB2 node: book-title");
    }

    parsedBook.Metadata.TitleUtf8 = *title;

    if (const std::optional<std::string> lang = TryReadTextChild(titleInfoNode, "lang"))
    {
        parsedBook.Metadata.Language = *lang;
    }
    else
    {
        if (Librova::Logging::CLogging::IsInitialized())
        {
            Librova::Logging::Warn("FB2 missing required <lang> node in '{}'; language will be empty.",
                Librova::Unicode::PathToUtf8(filePath));
        }
        parsedBook.Metadata.Language = "";
    }

    for (const pugi::xml_node authorNode : titleInfoNode.children())
    {
        if (!MatchesLocalName(authorNode, "author"))
        {
            continue;
        }

        const std::string author = JoinAuthorName(authorNode);

        if (!author.empty())
        {
            parsedBook.Metadata.AuthorsUtf8.push_back(author);
        }
    }

    if (parsedBook.Metadata.AuthorsUtf8.empty())
    {
        if (Librova::Logging::CLogging::IsInitialized())
        {
            Librova::Logging::Warn(
                "FB2 title-info has no non-empty author nodes; using 'Аноним' fallback."
                " [title_info_author_nodes={}, title_info_preview=\"{}\", document_info_preview=\"{}\", file=\"{}\"]",
                CountAuthorNodes(titleInfoNode),
                BuildNodePreview(titleInfoNode),
                BuildNodePreview(FindFirstChildByLocalName(descriptionNode, "document-info")),
                Librova::Unicode::PathToUtf8(filePath));
        }
        parsedBook.Metadata.AuthorsUtf8.push_back("Аноним");
    }

    for (const pugi::xml_node childNode : titleInfoNode.children())
    {
        if (MatchesLocalName(childNode, "genre"))
        {
            const std::optional<std::string> rawCode = TryReadNodeText(childNode);
            if (rawCode.has_value())
            {
                const std::string_view resolvedName = CFb2GenreMapper::ResolveGenreName(*rawCode);
                if (resolvedName == std::string_view{*rawCode} && Librova::Logging::CLogging::IsInitialized())
                {
                    Librova::Logging::Warn(
                        "FB2 unknown genre code '{}' (no mapping) in file: {}",
                        *rawCode,
                        Librova::Unicode::PathToUtf8(filePath));
                }
                parsedBook.Metadata.GenresUtf8.push_back(std::string{resolvedName});
            }
        }
    }

    if (const std::optional<std::string> sequenceName = TryReadTextChild(titleInfoNode.child("sequence"), "name"))
    {
        parsedBook.Metadata.SeriesUtf8 = sequenceName;
    }
    else if (const char* sequenceValue = FindFirstChildByLocalName(titleInfoNode, "sequence").attribute("name").as_string(); sequenceValue != nullptr && sequenceValue[0] != '\0')
    {
        parsedBook.Metadata.SeriesUtf8 = std::string{sequenceValue};
    }

    if (const char* sequenceNumber = FindFirstChildByLocalName(titleInfoNode, "sequence").attribute("number").as_string(); sequenceNumber != nullptr && sequenceNumber[0] != '\0')
    {
        const auto parsed = TryParseNumber<double>(sequenceNumber);
        if (parsed.has_value())
        {
            parsedBook.Metadata.SeriesIndex = *parsed;
        }
        else
        {
            if (Librova::Logging::CLogging::IsInitialized())
            {
                Librova::Logging::Warn(
                    "FB2 non-numeric sequence number '{}' skipped in file: {}",
                    sequenceNumber,
                    Librova::Unicode::PathToUtf8(filePath));
            }
        }
    }

    if (const pugi::xml_node annotationNode = FindFirstChildByLocalName(titleInfoNode, "annotation"))
    {
        const std::string description = ReadAnnotationText(annotationNode);

        if (!description.empty())
        {
            parsedBook.Metadata.DescriptionUtf8 = description;
        }
    }

    if (const pugi::xml_node publishInfoNode = FindFirstChildByLocalName(descriptionNode, "publish-info"))
    {
        if (const std::optional<std::string> isbn = TryReadTextChild(publishInfoNode, "isbn"))
        {
            parsedBook.Metadata.Isbn = isbn;
        }

        if (const std::optional<std::string> publisher = TryReadTextChild(publishInfoNode, "publisher"))
        {
            parsedBook.Metadata.PublisherUtf8 = publisher;
        }

        if (const std::optional<std::string> year = TryReadTextChild(publishInfoNode, "year"))
        {
            const auto parsed = TryParseNumber<int>(*year);
            if (parsed.has_value())
            {
                parsedBook.Metadata.Year = *parsed;
            }
            else
            {
                if (Librova::Logging::CLogging::IsInitialized())
                {
                    Librova::Logging::Warn(
                        "FB2 non-integer publish year '{}' skipped in file: {}",
                        *year,
                        Librova::Unicode::PathToUtf8(filePath));
                }
            }
        }
    }

    if (const pugi::xml_node documentInfoNode = FindFirstChildByLocalName(descriptionNode, "document-info"))
    {
        if (const std::optional<std::string> identifier = TryReadTextChild(documentInfoNode, "id"))
        {
            parsedBook.Metadata.Identifier = identifier;
        }
    }

    if (const std::optional<std::string> coverBinaryId = TryGetCoverBinaryId(titleInfoNode))
    {
        const pugi::xml_node binaryNode = FindBinaryById(rootNode, *coverBinaryId);

        if (binaryNode)
        {
            parsedBook.CoverBytes = DecodeBase64(binaryNode.text().as_string());

            if (!parsedBook.CoverBytes.empty())
            {
                std::string extension = MimeTypeToExtension(binaryNode.attribute("content-type").as_string());

                if (extension.empty())
                {
                    extension = NormalizeExtension(std::filesystem::path{binaryNode.attribute("id").as_string()}.extension().string());
                }

                if (!extension.empty())
                {
                    parsedBook.CoverExtension = extension;
                }
            }
        }
    }

    return parsedBook;
}

} // namespace Librova::Fb2Parsing
