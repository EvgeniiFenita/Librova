#include "Fb2Parsing/Fb2Parser.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <pugixml.hpp>

namespace LibriFlow::Fb2Parsing {
namespace {

[[nodiscard]] std::string ReadTextFile(const std::filesystem::path& filePath)
{
    std::ifstream input(filePath, std::ios::binary);

    if (!input)
    {
        throw std::runtime_error("Failed to open FB2 file: " + filePath.string());
    }

    return std::string{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

[[nodiscard]] pugi::xml_document ParseXml(const std::string& text, const std::filesystem::path& filePath)
{
    pugi::xml_document document;
    const pugi::xml_parse_result result = document.load_buffer(text.data(), text.size());

    if (!result)
    {
        throw std::runtime_error("Failed to parse FB2 XML from " + filePath.string() + ": " + result.description());
    }

    return document;
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

[[nodiscard]] std::string RequireTextChild(const pugi::xml_node& parent, const char* childName, const char* errorLabel)
{
    const std::optional<std::string> value = TryReadTextChild(parent, childName);

    if (!value.has_value())
    {
        throw std::runtime_error(std::string{"Missing required FB2 node: "} + errorLabel);
    }

    return *value;
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

bool CFb2Parser::CanParse(const LibriFlow::Domain::EBookFormat format) const
{
    return format == LibriFlow::Domain::EBookFormat::Fb2;
}

LibriFlow::Domain::SParsedBook CFb2Parser::Parse(const std::filesystem::path& filePath) const
{
    const pugi::xml_document document = ParseXml(ReadTextFile(filePath), filePath);
    const pugi::xml_node rootNode = FindFirstChildByLocalName(document, "FictionBook");
    const pugi::xml_node descriptionNode = FindFirstChildByLocalName(rootNode, "description");
    const pugi::xml_node titleInfoNode = FindFirstChildByLocalName(descriptionNode, "title-info");

    if (!rootNode || !titleInfoNode)
    {
        throw std::runtime_error("FB2 document is missing required description/title-info nodes.");
    }

    LibriFlow::Domain::SParsedBook parsedBook;
    parsedBook.SourceFormat = LibriFlow::Domain::EBookFormat::Fb2;
    const std::optional<std::string> title = TryReadNodeText(FindFirstChildByLocalName(titleInfoNode, "book-title"));

    if (!title.has_value())
    {
        throw std::runtime_error("Missing required FB2 node: book-title");
    }

    parsedBook.Metadata.TitleUtf8 = *title;
    parsedBook.Metadata.Language = RequireTextChild(titleInfoNode, "lang", "lang");

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
        throw std::runtime_error("FB2 metadata must contain at least one author.");
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
        try
        {
            parsedBook.Metadata.SeriesIndex = std::stod(sequenceNumber);
        }
        catch (const std::exception&)
        {
            throw std::runtime_error("Failed to parse FB2 sequence number.");
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
            try
            {
                parsedBook.Metadata.Year = std::stoi(*year);
            }
            catch (const std::exception&)
            {
                throw std::runtime_error("Failed to parse FB2 publish year.");
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

} // namespace LibriFlow::Fb2Parsing
