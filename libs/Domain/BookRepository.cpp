#include "Domain/BookRepository.hpp"

namespace Librova::Domain {

std::vector<SBookId> IBookRepository::ReserveIds(const std::size_t count)
{
    std::vector<SBookId> reservedIds;
    reservedIds.reserve(count);

    for (std::size_t i = 0; i < count; ++i)
    {
        reservedIds.push_back(ReserveId());
    }

    return reservedIds;
}

std::vector<IBookRepository::SBatchBookResult>
IBookRepository::AddBatch(std::span<const SBatchBookEntry> entries)
{
    std::vector<SBatchBookResult> results;
    results.reserve(entries.size());

    for (const auto& entry : entries)
    {
        try
        {
            const SBookId bookId = entry.ForceAdd
                ? ForceAdd(entry.Book)
                : Add(entry.Book);

            results.push_back({
                .Status = EBatchAddStatus::Imported,
                .BookId = bookId
            });
        }
        catch (const CDuplicateHashException& ex)
        {
            results.push_back({
                .Status            = EBatchAddStatus::RejectedDuplicate,
                .ConflictingBookId = ex.ExistingBookId()
            });
        }
        catch (const std::exception& ex)
        {
            results.push_back({
                .Status = EBatchAddStatus::Failed,
                .Error  = ex.what()
            });
        }
    }

    return results;
}

void IBookRepository::RemoveBatch(std::span<const SBookId> ids)
{
    for (const auto& id : ids)
    {
        Remove(id);
    }
}

} // namespace Librova::Domain
