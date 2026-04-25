#pragma once

#include <filesystem>

#include "App/ILibraryApplication.hpp"

namespace Librova::Application {

class CLibraryBootstrap final
{
public:
    static void PrepareLibraryRoot(
        const std::filesystem::path& libraryRoot,
        ELibraryOpenMode libraryOpenMode);
};

} // namespace Librova::Application
