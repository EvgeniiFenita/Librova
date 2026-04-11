#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <vector>

#include "Domain/BookRepository.hpp"

namespace Librova::BookDatabase {

class CSqliteBookQueryRepository final : public Librova::Domain::IBookQueryRepository
{
public:
    explicit CSqliteBookQueryRepository(std::filesystem::path databasePath);

    [[nodiscard]] std::vector<Librova::Domain::SBook> Search(const Librova::Domain::SSearchQuery& query) const override;
    [[nodiscard]] std::uint64_t CountSearchResults(const Librova::Domain::SSearchQuery& query) const override;
    [[nodiscard]] std::vector<std::string> ListAvailableLanguages(const Librova::Domain::SSearchQuery& query) const override;
    [[nodiscard]] std::vector<std::string> ListAvailableTags(const Librova::Domain::SSearchQuery& query) const override;
    [[nodiscard]] std::vector<Librova::Domain::SDuplicateMatch> FindDuplicates(const Librova::Domain::SCandidateBook& candidate) const override;
    [[nodiscard]] Librova::Domain::IBookQueryRepository::SLibraryStatistics GetLibraryStatistics() const override;

private:
    struct SCoverDirectorySnapshot
    {
        std::uint64_t FileCount = 0;
        std::uint64_t TotalSizeBytes = 0;
        std::filesystem::file_time_type LatestWriteTime = std::filesystem::file_time_type::min();
    };

    struct SStatisticsCache
    {
        std::filesystem::file_time_type DatabaseLastWriteTime;
        SCoverDirectorySnapshot CoverDirectorySnapshot;
        Librova::Domain::IBookQueryRepository::SLibraryStatistics Statistics;
    };

    std::filesystem::path m_databasePath;
    mutable std::mutex m_statisticsCacheMutex;
    mutable std::optional<SStatisticsCache> m_statisticsCache;
};

} // namespace Librova::BookDatabase
