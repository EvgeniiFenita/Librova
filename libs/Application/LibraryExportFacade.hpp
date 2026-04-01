#pragma once

#include <filesystem>
#include <optional>

#include "Domain/BookRepository.hpp"
#include "Domain/ServiceContracts.hpp"

namespace Librova::Application {

struct SExportBookRequest
{
    Librova::Domain::SBookId BookId;
    std::filesystem::path DestinationPath;
    std::optional<Librova::Domain::EBookFormat> ExportFormat;
};

class CLibraryExportFacade final
{
public:
    CLibraryExportFacade(
        const Librova::Domain::IBookRepository& bookRepository,
        std::filesystem::path libraryRoot,
        const Librova::Domain::IBookConverter* converter = nullptr);

    [[nodiscard]] std::optional<std::filesystem::path> ExportBook(
        const SExportBookRequest& request) const;

private:
    [[nodiscard]] std::filesystem::path ExportManagedFile(
        const std::filesystem::path& sourcePath,
        const std::filesystem::path& destinationPath) const;

    [[nodiscard]] std::filesystem::path ExportConvertedFile(
        const Librova::Domain::SBook& book,
        const std::filesystem::path& sourcePath,
        const std::filesystem::path& destinationPath,
        Librova::Domain::EBookFormat exportFormat) const;

    [[nodiscard]] std::filesystem::path ResolveManagedSourcePath(const std::filesystem::path& managedPath) const;

    const Librova::Domain::IBookRepository& m_bookRepository;
    std::filesystem::path m_libraryRoot;
    const Librova::Domain::IBookConverter* m_converter;
};

} // namespace Librova::Application
