#pragma once

#include <filesystem>

namespace LibriFlow::CoreHost {

class CLibraryBootstrap final
{
public:
    static void PrepareLibraryRoot(const std::filesystem::path& libraryRoot);
};

} // namespace LibriFlow::CoreHost
