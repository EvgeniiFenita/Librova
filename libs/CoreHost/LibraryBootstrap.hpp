#pragma once

#include <filesystem>

namespace Librova::CoreHost {

class CLibraryBootstrap final
{
public:
    static void PrepareLibraryRoot(const std::filesystem::path& libraryRoot);
};

} // namespace Librova::CoreHost
