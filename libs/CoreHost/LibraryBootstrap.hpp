#pragma once

#include <filesystem>

#include "CoreHost/HostOptions.hpp"

namespace Librova::CoreHost {

class CLibraryBootstrap final
{
public:
    static void PrepareLibraryRoot(
        const std::filesystem::path& libraryRoot,
        ELibraryOpenMode libraryOpenMode);
};

} // namespace Librova::CoreHost
