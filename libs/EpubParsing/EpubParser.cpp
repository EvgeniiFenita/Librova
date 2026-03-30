#include "EpubParsing/EpubParser.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <pugixml.hpp>
#include <zip.h>

namespace Librova::EpubParsing {
namespace {

class CZipArchive final
{
public:
    explicit CZipArchive(const std::filesystem::path& filePath)
    {
        int errorCode = ZIP_ER_OK;
        m_archive = zip_open(filePath.string().c_str(), ZIP_RDONLY, &errorCode);

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

[[nodiscard]] std::optional<std::string> TryFindCoverPath(const pugi::xml_node& packageNode)
{
    const pugi::xml_node manifestNode = packageNode.child("manifest");

    if (!manifestNode)
    {
        return std::nullopt;
    }

    std::optional<std::string> coverItemId;

    for (const pugi::xml_node metaNode : packageNode.child("metadata").children("meta"))
    {
        if (std::string_view{metaNode.attribute("name").as_string()} == "cover")
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
        for (const pugi::xml_node itemNode : manifestNode.children("item"))
        {
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

    for (const pugi::xml_node itemNode : manifestNode.children("item"))
    {
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
    const pugi::xml_node rootfileNode = containerDocument.child("container").child("rootfiles").child("rootfile");

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
    const pugi::xml_node packageNode = packageDocument.child("package");
    const pugi::xml_node metadataNode = packageNode.child("metadata");

    if (!packageNode || !metadataNode)
    {
        throw std::runtime_error("EPUB package document is missing required metadata.");
    }

    Librova::Domain::SParsedBook parsedBook;
    parsedBook.SourceFormat = Librova::Domain::EBookFormat::Epub;
    parsedBook.Metadata.TitleUtf8 = RequireTextChild(metadataNode, "dc:title", "dc:title");
    parsedBook.Metadata.Language = RequireTextChild(metadataNode, "dc:language", "dc:language");

    for (const pugi::xml_node creatorNode : metadataNode.children("dc:creator"))
    {
        const std::string author = creatorNode.text().as_string();

        if (!author.empty())
        {
            parsedBook.Metadata.AuthorsUtf8.push_back(author);
        }
    }

    if (parsedBook.Metadata.AuthorsUtf8.empty())
    {
        throw std::runtime_error("EPUB metadata must contain at least one dc:creator.");
    }

    if (const pugi::xml_node identifierNode = metadataNode.child("dc:identifier"))
    {
        const std::string identifier = identifierNode.text().as_string();

        if (!identifier.empty())
        {
            parsedBook.Metadata.Identifier = identifier;
        }
    }

    if (const pugi::xml_node descriptionNode = metadataNode.child("dc:description"))
    {
        const std::string description = descriptionNode.text().as_string();

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
