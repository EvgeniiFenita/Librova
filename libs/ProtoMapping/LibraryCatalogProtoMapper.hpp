#pragma once

#include "Application/LibraryCatalogFacade.hpp"
#include "import_jobs.pb.h"

namespace Librova::ProtoMapping {

class CLibraryCatalogProtoMapper final
{
public:
    [[nodiscard]] static Librova::Application::SBookListRequest FromProto(
        const librova::v1::BookListRequest& request);

    [[nodiscard]] static librova::v1::BookListRequest ToProto(
        const Librova::Application::SBookListRequest& request);

    [[nodiscard]] static librova::v1::BookListItem ToProto(
        const Librova::Application::SBookListItem& item);

    [[nodiscard]] static librova::v1::ListBooksResponse ToProtoResponse(
        const Librova::Application::SBookListResult& result);

private:
    [[nodiscard]] static std::string PathToUtf8(const std::filesystem::path& path);
    [[nodiscard]] static std::filesystem::path PathFromUtf8(const std::string& value);
    [[nodiscard]] static Librova::Domain::EBookFormat FromProto(librova::v1::BookFormat format) noexcept;
    [[nodiscard]] static Librova::Domain::EBookSort FromProto(librova::v1::BookSort sort) noexcept;
    [[nodiscard]] static librova::v1::BookFormat ToProto(Librova::Domain::EBookFormat format) noexcept;
    [[nodiscard]] static librova::v1::BookSort ToProto(Librova::Domain::EBookSort sort) noexcept;
};

} // namespace Librova::ProtoMapping
