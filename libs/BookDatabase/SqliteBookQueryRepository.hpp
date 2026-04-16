#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

#include "Domain/BookRepository.hpp"
#include "Sqlite/SqliteConnection.hpp"

namespace Librova::BookDatabase {

class CSqliteBookQueryRepository final : public Librova::Domain::IBookQueryRepository
{
public:
    explicit CSqliteBookQueryRepository(std::filesystem::path databasePath);

    [[nodiscard]] std::vector<Librova::Domain::SBook> Search(const Librova::Domain::SSearchQuery& query) const override;
    [[nodiscard]] std::uint64_t CountSearchResults(const Librova::Domain::SSearchQuery& query) const override;
    [[nodiscard]] std::vector<std::string> ListAvailableLanguages(const Librova::Domain::SSearchQuery& query) const override;
    [[nodiscard]] std::vector<std::string> ListAvailableTags(const Librova::Domain::SSearchQuery& query) const override;
    [[nodiscard]] std::vector<std::string> ListAvailableGenres(const Librova::Domain::SSearchQuery& query) const override;
    [[nodiscard]] std::vector<Librova::Domain::SDuplicateMatch> FindDuplicates(const Librova::Domain::SCandidateBook& candidate) const override;
    [[nodiscard]] Librova::Domain::IBookQueryRepository::SLibraryStatistics GetLibraryStatistics() const override;

    // Closes the cached dedup connection. Call before deleting the database file.
    void CloseSession();

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
    mutable std::mutex m_findDupConnectionMutex;
    mutable std::unique_ptr<Librova::Sqlite::CSqliteConnection> m_findDupConnection;
};

} // namespace Librova::BookDatabase
