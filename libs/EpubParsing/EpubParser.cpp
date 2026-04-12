#include "EpubParsing/EpubParser.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <pugixml.hpp>
#include <zip.h>

#include "Unicode/UnicodeConversion.hpp"

namespace Librova::EpubParsing {
namespace {

class CZipArchive final
{
public:
    explicit CZipArchive(const std::filesystem::path& filePath)
    {
        int errorCode = ZIP_ER_OK;
        const std::string utf8Path = Librova::Unicode::PathToUtf8(filePath);
        m_archive = zip_open(utf8Path.c_str(), ZIP_RDONLY, &errorCode);

        if (m_archive == nullptr)
        {
            zip_error_t errorState;
            zip_error_init_with_code(&errorState, errorCode);
            const std::string message = zip_error_strerror(&errorState);
            zip_error_fini(&errorState);
            throw std::runtime_error("Failed to open EPUB archive: " + message);
        }
    }

    ~CZipArchive()
    {
        if (m_archive != nullptr)
        {
            zip_close(m_archive);
        }
    }

    CZipArchive(const CZipArchive&) = delete;
    CZipArchive& operator=(const CZipArchive&) = delete;
    CZipArchive(CZipArchive&&) = delete;
    CZipArchive& operator=(CZipArchive&&) = delete;

    [[nodiscard]] std::string ReadText(const std::string_view entryPath) const
    {
        const std::vector<std::byte> bytes = ReadBytes(entryPath);
        return std::string{reinterpret_cast<const char*>(bytes.data()), bytes.size()};
    }

    [[nodiscard]] std::vector<std::byte> ReadBytes(const std::string_view entryPath) const
    {
        zip_stat_t stat{};

        if (zip_stat(m_archive, std::string{entryPath}.c_str(), ZIP_FL_ENC_GUESS, &stat) != 0)
        {
            throw std::runtime_error("Failed to locate EPUB entry: " + std::string{entryPath});
        }

        zip_file_t* file = zip_fopen(m_archive, std::string{entryPath}.c_str(), ZIP_FL_ENC_GUESS);

        if (file == nullptr)
        {
            throw std::runtime_error("Failed to open EPUB entry: " + std::string{entryPath});
        }

        std::vector<std::byte> bytes(static_cast<std::size_t>(stat.size));
        const zip_int64_t readCount = zip_fread(file, bytes.data(), stat.size);
        zip_fclose(file);

        if (readCount < 0 || static_cast<zip_uint64_t>(readCount) != stat.size)
        {
            throw std::runtime_error("Failed to read EPUB entry: " + std::string{entryPath});
        }

        return bytes;
    }

private:
    zip_t* m_archive = nullptr;
};

static_assert(!std::is_copy_constructible_v<CZipArchive>);
static_assert(!std::is_move_constructible_v<CZipArchive>);

[[nodiscard]] pugi::xml_document ParseXml(const std::string& text, const std::string_view label)
{
    pugi::xml_document document;
    const pugi::xml_parse_result result = document.load_buffer(text.data(), text.size());

    if (!result)
    {
        throw std::runtime_error("Failed to parse XML from " + std::string{label} + ": " + result.description());
    }

    return document;
}

[[nodiscard]] std::filesystem::path NormalizeArchivePath(const std::string_view value)
{
    return std::filesystem::path{value}.lexically_normal();
}

[[nodiscard]] std::string PathToArchiveString(const std::filesystem::path& path)
{
    return path.generic_string();
}

[[nodiscard]] std::string RequireTextChild(const pugi::xml_node& parent, const char* childName, const char* errorLabel)
{
    const pugi::xml_node node = parent.child(childName);

    if (!node)
    {
        throw std::runtime_error(std::string{"Missing required EPUB node: "} + errorLabel);
    }

    const std::string value = node.text().as_string();

    if (value.empty())
    {
        throw std::runtime_error(std::string{"Missing required EPUB text value: "} + errorLabel);
    }

    return value;
}

[[nodiscard]] std::string Trim(const std::string_view value)
{
    std::size_t start = 0;
    std::size_t end = value.size();

    while (start < end && std::isspace(static_cast<unsigned char>(value[start])) != 0)
    {
        ++start;
    }

    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
    {
        --end;
    }

    return std::string{value.substr(start, end - start)};
}

[[nodiscard]] bool MatchesLocalName(const std::string_view qualifiedName, const std::string_view localName)
{
    const std::size_t separatorIndex = qualifiedName.find(':');
    const std::string_view currentLocalName = separatorIndex == std::string_view::npos
        ? qualifiedName
        : qualifiedName.substr(separatorIndex + 1);
    return currentLocalName == localName;
}

[[nodiscard]] bool MatchesLocalName(const pugi::xml_node& node, const std::string_view localName)
{
    return MatchesLocalName(node.name(), localName);
}

[[nodiscard]] bool MatchesLocalName(const pugi::xml_attribute& attribute, const std::string_view localName)
{
    return MatchesLocalName(attribute.name(), localName);
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

[[nodiscard]] std::optional<std::string> TryReadNodeText(const pugi::xml_node& node)
{
    const std::string value = Trim(node.text().as_string());
    if (value.empty())
    {
        return std::nullopt;
    }

    return value;
}

[[nodiscard]] std::string RequireTextChildByLocalName(const pugi::xml_node& parent, const char* childName, const char* errorLabel)
{
    const std::optional<std::string> value = TryReadNodeText(FindFirstChildByLocalName(parent, childName));
    if (!value.has_value())
    {
        throw std::runtime_error(std::string{"Missing required EPUB node: "} + errorLabel);
    }

    return *value;
}

[[nodiscard]] std::optional<std::string> TryReadAttributeByLocalName(const pugi::xml_node& node, const std::string_view localName)
{
    for (const pugi::xml_attribute attribute : node.attributes())
    {
        if (MatchesLocalName(attribute, localName))
        {
            const std::string value = Trim(attribute.as_string());
            if (!value.empty())
            {
                return value;
            }
        }
    }

    return std::nullopt;
}

[[nodiscard]] std::optional<std::string> FindRefinedMetaValue(
    const pugi::xml_node& metadataNode,
    const std::string_view targetId,
    const std::string_view propertyName)
{
    if (targetId.empty())
    {
        return std::nullopt;
    }

    const std::string expectedRef = "#" + std::string{targetId};
    for (const pugi::xml_node childNode : metadataNode.children())
    {
        if (!MatchesLocalName(childNode, "meta"))
        {
            continue;
        }

        const std::optional<std::string> refines = TryReadAttributeByLocalName(childNode, "refines");
        const std::optional<std::string> property = TryReadAttributeByLocalName(childNode, "property");
        if (!refines.has_value() || !property.has_value())
        {
            continue;
        }

        if (*refines == expectedRef && *property == propertyName)
        {
            return TryReadNodeText(childNode);
        }
    }

    return std::nullopt;
}

template <typename TValue>
TValue ParseExactNumber(const std::string_view value, const char* errorMessage)
{
    TValue parsedValue{};
    const char* begin = value.data();
    const char* end = value.data() + value.size();
    const auto [parseEnd, errorCode] = std::from_chars(begin, end, parsedValue);

    if (errorCode != std::errc{} || parseEnd != end)
    {
        throw std::runtime_error(errorMessage);
    }

    return parsedValue;
}

void AppendUniqueText(std::vector<std::string>& values, const std::string& value)
{
    if (std::find(values.begin(), values.end(), value) == values.end())
    {
        values.push_back(value);
    }
}

void ParseSubjects(const pugi::xml_node& metadataNode, Librova::Domain::SParsedBook& parsedBook)
{
    for (const pugi::xml_node childNode : metadataNode.children())
    {
        if (!MatchesLocalName(childNode, "subject"))
        {
            continue;
        }

        const std::optional<std::string> subject = TryReadNodeText(childNode);
        if (subject.has_value())
        {
            AppendUniqueText(parsedBook.Metadata.GenresUtf8, *subject);
        }
    }
}

void ParseSeriesMetadata(const pugi::xml_node& metadataNode, Librova::Domain::SParsedBook& parsedBook)
{
    for (const pugi::xml_node childNode : metadataNode.children())
    {
        if (!MatchesLocalName(childNode, "meta"))
        {
            continue;
        }

        const std::optional<std::string> property = TryReadAttributeByLocalName(childNode, "property");
        if (!property.has_value() || *property != "belongs-to-collection")
        {
            continue;
        }

        const std::optional<std::string> collectionName = TryReadNodeText(childNode);
        if (!collectionName.has_value())
        {
            continue;
        }

        const std::optional<std::string> itemId = TryReadAttributeByLocalName(childNode, "id");
        const std::optional<std::string> collectionType = itemId.has_value()
            ? FindRefinedMetaValue(metadataNode, *itemId, "collection-type")
            : std::nullopt;

        if (collectionType.has_value() && *collectionType != "series")
        {
            continue;
        }

        parsedBook.Metadata.SeriesUtf8 = *collectionName;

        if (itemId.has_value())
        {
            if (const std::optional<std::string> groupPosition = FindRefinedMetaValue(metadataNode, *itemId, "group-position"))
            {
                parsedBook.Metadata.SeriesIndex = ParseExactNumber<double>(
                    *groupPosition,
                    "Failed to parse EPUB series index.");
            }
        }

        return;
    }
}

[[nodiscard]] std::optional<std::string> TryFindCoverPath(const pugi::xml_node& packageNode)
{
    const pugi::xml_node manifestNode = FindFirstChildByLocalName(packageNode, "manifest");

    if (!manifestNode)
    {
        return std::nullopt;
    }

    std::optional<std::string> coverItemId;

    for (const pugi::xml_node metaNode : FindFirstChildByLocalName(packageNode, "metadata").children())
    {
        if (!MatchesLocalName(metaNode, "meta"))
        {
            continue;
        }

        if (TryReadAttributeByLocalName(metaNode, "name") == std::optional<std::string>{"cover"})
        {
            const std::string contentValue = metaNode.attribute("content").as_string();

            if (!contentValue.empty())
            {
                coverItemId = contentValue;
                break;
            }
        }
    }

    if (coverItemId.has_value())
    {
        for (const pugi::xml_node itemNode : manifestNode.children())
        {
            if (!MatchesLocalName(itemNode, "item"))
            {
                continue;
            }

            if (std::string_view{itemNode.attribute("id").as_string()} == *coverItemId)
            {
                const std::string href = itemNode.attribute("href").as_string();

                if (!href.empty())
                {
                    return href;
                }
            }
        }
    }

    for (const pugi::xml_node itemNode : manifestNode.children())
    {
        if (!MatchesLocalName(itemNode, "item"))
        {
            continue;
        }

        const std::string_view properties = itemNode.attribute("properties").as_string();

        if (properties.find("cover-image") != std::string_view::npos)
        {
            const std::string href = itemNode.attribute("href").as_string();

            if (!href.empty())
            {
                return href;
            }
        }
    }

    return std::nullopt;
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

} // namespace

bool CEpubParser::CanParse(const Librova::Domain::EBookFormat format) const
{
    return format == Librova::Domain::EBookFormat::Epub;
}

Librova::Domain::SParsedBook CEpubParser::Parse(const std::filesystem::path& filePath) const
{
    CZipArchive archive(filePath);

    const pugi::xml_document containerDocument = ParseXml(archive.ReadText("META-INF/container.xml"), "META-INF/container.xml");
    const pugi::xml_node rootfileNode =
        FindFirstChildByLocalName(
            FindFirstChildByLocalName(
                FindFirstChildByLocalName(containerDocument, "container"),
                "rootfiles"),
            "rootfile");

    if (!rootfileNode)
    {
        throw std::runtime_error("EPUB container.xml does not contain a rootfile entry.");
    }

    const std::filesystem::path packagePath = NormalizeArchivePath(rootfileNode.attribute("full-path").as_string());

    if (packagePath.empty())
    {
        throw std::runtime_error("EPUB rootfile path is empty.");
    }

    const pugi::xml_document packageDocument = ParseXml(archive.ReadText(PathToArchiveString(packagePath)), "package.opf");
    const pugi::xml_node packageNode = FindFirstChildByLocalName(packageDocument, "package");
    const pugi::xml_node metadataNode = FindFirstChildByLocalName(packageNode, "metadata");

    if (!packageNode || !metadataNode)
    {
        throw std::runtime_error("EPUB package document is missing required metadata.");
    }

    Librova::Domain::SParsedBook parsedBook;
    parsedBook.SourceFormat = Librova::Domain::EBookFormat::Epub;
    parsedBook.Metadata.TitleUtf8 = RequireTextChildByLocalName(metadataNode, "title", "dc:title");
    parsedBook.Metadata.Language = RequireTextChildByLocalName(metadataNode, "language", "dc:language");

    for (const pugi::xml_node creatorNode : metadataNode.children())
    {
        if (!MatchesLocalName(creatorNode, "creator"))
        {
            continue;
        }

        const std::string author = Trim(creatorNode.text().as_string());

        if (!author.empty())
        {
            parsedBook.Metadata.AuthorsUtf8.push_back(author);
        }
    }

    if (parsedBook.Metadata.AuthorsUtf8.empty())
    {
        throw std::runtime_error("EPUB metadata must contain at least one dc:creator.");
    }

    ParseSubjects(metadataNode, parsedBook);
    ParseSeriesMetadata(metadataNode, parsedBook);

    if (const pugi::xml_node identifierNode = FindFirstChildByLocalName(metadataNode, "identifier"))
    {
        const std::string identifier = Trim(identifierNode.text().as_string());

        if (!identifier.empty())
        {
            parsedBook.Metadata.Identifier = identifier;
        }
    }

    if (const pugi::xml_node descriptionNode = FindFirstChildByLocalName(metadataNode, "description"))
    {
        const std::string description = Trim(descriptionNode.text().as_string());

        if (!description.empty())
        {
            parsedBook.Metadata.DescriptionUtf8 = description;
        }
    }

    const std::optional<std::string> coverRelativePath = TryFindCoverPath(packageNode);

    if (coverRelativePath.has_value())
    {
        const std::filesystem::path coverArchivePath = (packagePath.parent_path() / NormalizeArchivePath(*coverRelativePath)).lexically_normal();
        parsedBook.CoverBytes = archive.ReadBytes(PathToArchiveString(coverArchivePath));
        parsedBook.CoverExtension = NormalizeExtension(coverArchivePath.extension().string());
    }

    return parsedBook;
}

} // namespace Librova::EpubParsing
