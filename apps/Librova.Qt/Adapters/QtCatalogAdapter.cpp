#include "Adapters/QtCatalogAdapter.hpp"

#include <algorithm>
#include <memory>
#include <exception>
#include <string>
#include <vector>

#include "Adapters/QtStringConversions.hpp"
#include "Domain/SearchQuery.hpp"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcCatalog, "librova.catalog")

namespace LibrovaQt {

namespace {

Librova::Application::SBookListRequest buildRequest(
    const Librova::Application::SBookListRequest& req)
{
    return req; // pass-through; adapter mutates m_request in place
}

QString formatBookCount(std::uint64_t count)
{
    return count == 1 ? QStringLiteral("1 book") : QStringLiteral("%1 books").arg(count);
}

QString formatSizeInMegabytes(std::uint64_t bytes)
{
    const double megabytes = static_cast<double>(bytes) / (1024.0 * 1024.0);
    return QStringLiteral("%1 MB").arg(QString::number(megabytes, 'f', 1));
}

} // namespace

QtCatalogAdapter::QtCatalogAdapter(IBackendDispatcher* backend, QObject* parent)
    : QObject(parent)
    , m_backend(backend)
    , m_bookListModel(new BookListModel(this))
    , m_bookDetailsModel(new BookDetailsModel(this))
    , m_collectionListModel(new CollectionListModel(this))
    , m_languageFacets(new FacetListModel(this))
    , m_genreFacets(new FacetListModel(this))
{
    m_request.Offset = 0;
    m_request.Limit = static_cast<std::size_t>(m_pageSize);
}

void QtCatalogAdapter::refresh()
{
    if (!m_backend->isOpen())
        return;

    m_request.Offset = 0;
    m_request.Limit = static_cast<std::size_t>(m_pageSize);
    setLoading(true);
    const quint64 gen = ++m_generation;
    dispatchRefresh(gen);
    dispatchCollectionRefresh();
}

void QtCatalogAdapter::setSearchText(const QString& text)
{
    const bool hadRestrictions = hasResultRestrictions();
    m_request.TextUtf8 = qtToUtf8(text);
    if (hadRestrictions != hasResultRestrictions())
    {
        Q_EMIT resultRestrictionsChanged();
        Q_EMIT emptyStateChanged();
    }
    refresh();
}

void QtCatalogAdapter::setLanguageFilter(const QStringList& languages)
{
    const bool hadRestrictions = hasResultRestrictions();
    m_request.Languages.clear();
    for (const auto& l : languages)
        m_request.Languages.push_back(qtToUtf8(l));
    if (hadRestrictions != hasResultRestrictions())
    {
        Q_EMIT resultRestrictionsChanged();
        Q_EMIT emptyStateChanged();
    }
    refresh();
}

void QtCatalogAdapter::setGenreFilter(const QStringList& genres)
{
    const bool hadRestrictions = hasResultRestrictions();
    m_request.GenresUtf8.clear();
    for (const auto& g : genres)
        m_request.GenresUtf8.push_back(qtToUtf8(g));
    if (hadRestrictions != hasResultRestrictions())
    {
        Q_EMIT resultRestrictionsChanged();
        Q_EMIT emptyStateChanged();
    }
    refresh();
}

void QtCatalogAdapter::setCollectionFilter(qint64 collectionId)
{
    const bool hadRestrictions = hasResultRestrictions();
    if (collectionId <= 0)
    {
        clearCollectionFilter();
        return;
    }

    m_request.CollectionId = static_cast<std::int64_t>(collectionId);
    if (m_activeCollectionId != collectionId)
    {
        m_activeCollectionId = collectionId;
        Q_EMIT activeCollectionIdChanged();
    }
    if (hadRestrictions != hasResultRestrictions())
    {
        Q_EMIT resultRestrictionsChanged();
        Q_EMIT emptyStateChanged();
    }
    refresh();
}

void QtCatalogAdapter::clearCollectionFilter()
{
    const bool hadRestrictions = hasResultRestrictions();
    m_request.CollectionId.reset();
    if (m_activeCollectionId != -1)
    {
        m_activeCollectionId = -1;
        Q_EMIT activeCollectionIdChanged();
    }
    if (hadRestrictions != hasResultRestrictions())
    {
        Q_EMIT resultRestrictionsChanged();
        Q_EMIT emptyStateChanged();
    }
    refresh();
}

void QtCatalogAdapter::setSortBy(const QString& sortBy, const QString& direction)
{
    if (sortBy.isEmpty())
    {
        m_request.SortBy.reset();
    }
    else if (sortBy == QLatin1String("title"))
    {
        m_request.SortBy = Librova::Domain::EBookSort::Title;
    }
    else if (sortBy == QLatin1String("author"))
    {
        m_request.SortBy = Librova::Domain::EBookSort::Author;
    }
    else if (sortBy == QLatin1String("date_added"))
    {
        m_request.SortBy = Librova::Domain::EBookSort::DateAdded;
    }

    if (direction == QLatin1String("asc"))
        m_request.SortDirection = Librova::Domain::ESortDirection::Ascending;
    else if (direction == QLatin1String("desc"))
        m_request.SortDirection = Librova::Domain::ESortDirection::Descending;

    refresh();
}

void QtCatalogAdapter::setPage(int offset, int limit)
{
    m_request.Offset = static_cast<std::size_t>(std::max(0, offset));
    m_request.Limit  = static_cast<std::size_t>(limit > 0 ? limit : m_pageSize);
    if (!m_backend->isOpen())
    {
        return;
    }

    setLoading(true);
    const quint64 gen = ++m_generation;
    dispatchRefresh(gen);
    dispatchCollectionRefresh();
}

void QtCatalogAdapter::loadMore()
{
    if (!m_backend->isOpen() || m_loading || m_loadingMore || !hasMoreResults())
    {
        return;
    }

    setLoadingMore(true);
    dispatchLoadMore(m_generation);
}

bool QtCatalogAdapter::hasMoreResults() const
{
    return m_loadedCount < m_totalCount;
}

bool QtCatalogAdapter::hasResultRestrictions() const
{
    return !m_request.TextUtf8.empty()
        || !m_request.Languages.empty()
        || !m_request.GenresUtf8.empty()
        || m_request.CollectionId.has_value();
}

bool QtCatalogAdapter::showGoToImportButton() const
{
    return !m_loading && m_totalCount == 0 && !hasResultRestrictions();
}

QString QtCatalogAdapter::libraryStatisticsText() const
{
    return QStringLiteral("Library: %1, %2")
        .arg(formatBookCount(m_statistics.BookCount))
        .arg(formatSizeInMegabytes(m_statistics.TotalLibrarySizeBytes));
}

void QtCatalogAdapter::loadBookDetails(qint64 bookId)
{
    if (!m_backend->isOpen())
    {
        return;
    }

    if (bookId <= 0)
    {
        clearBookDetails();
        return;
    }

    const quint64 gen = ++m_detailsGeneration;
    const Librova::Domain::SBookId id{static_cast<std::int64_t>(bookId)};
    auto result = std::make_shared<std::optional<Librova::Application::SBookDetails>>();

    setDetailsLoading(true);
    m_backend->dispatch(
        [result, id](auto& app) { *result = app.GetBookDetails(id); },
        [this, result, gen, bookId]() {
            if (gen != m_detailsGeneration)
            {
                return;
            }

            setDetailsLoading(false);
            if (result->has_value())
            {
                m_bookDetailsModel->setDetails(**result);
                qCInfo(lcCatalog) << "GetBookDetails completed: bookId=" << bookId;
                Q_EMIT bookDetailsLoaded(bookId);
            }
            else
            {
                m_bookDetailsModel->clear();
                qCWarning(lcCatalog) << "GetBookDetails returned no book: bookId=" << bookId;
                Q_EMIT bookDetailsMissing(bookId);
            }
        },
        [this, gen, bookId](const QString& error) {
            if (gen != m_detailsGeneration)
            {
                return;
            }

            setDetailsLoading(false);
            m_bookDetailsModel->clear();
            qCWarning(lcCatalog) << "GetBookDetails failed:" << error << "bookId=" << bookId;
            Q_EMIT refreshFailed(error);
            Q_EMIT bookDetailsMissing(bookId);
        });
}

void QtCatalogAdapter::clearBookDetails()
{
    ++m_detailsGeneration;
    setDetailsLoading(false);
    m_bookDetailsModel->clear();
}

void QtCatalogAdapter::refreshCollections()
{
    if (!m_backend->isOpen())
        return;
    dispatchCollectionRefresh();
}

void QtCatalogAdapter::createCollection(const QString& name, const QString& iconKey)
{
    if (!m_backend->isOpen())
        return;

    const std::string nameUtf8    = qtToUtf8(name);
    const std::string iconKeyUtf8 = qtToUtf8(iconKey);

    auto result = std::make_shared<Librova::Domain::SBookCollection>();
    m_backend->dispatch(
        [result, nameUtf8, iconKeyUtf8](auto& app) {
            *result = app.CreateCollection(nameUtf8, iconKeyUtf8);
        },
        [this, result]() {
            qCInfo(lcCatalog) << "Collection created:" << result->Id;
            dispatchCollectionRefresh();
            emit collectionCreated(static_cast<qint64>(result->Id));
        },
        [this](const QString& error) {
            qCWarning(lcCatalog) << "CreateCollection failed:" << error;
            emit collectionCreateFailed(error);
        });
}

void QtCatalogAdapter::deleteCollection(qint64 collectionId)
{
    if (!m_backend->isOpen())
        return;

    const auto id = static_cast<std::int64_t>(collectionId);
    auto ok = std::make_shared<bool>(false);
    m_backend->dispatch(
        [ok, id](auto& app) { *ok = app.DeleteCollection(id); },
        [this, ok, collectionId]() {
            if (*ok)
            {
                qCInfo(lcCatalog) << "Collection deleted:" << collectionId;
                if (m_activeCollectionId == collectionId)
                {
                    clearCollectionFilter();
                }
                else
                {
                    dispatchCollectionRefresh();
                }
                emit collectionDeleted(collectionId);
            }
            else
            {
                qCWarning(lcCatalog) << "DeleteCollection returned false for" << collectionId;
            }
        },
        [collectionId](const QString& error) {
            qCWarning(lcCatalog) << "DeleteCollection failed:" << error
                                 << "collectionId=" << collectionId;
        });
}

void QtCatalogAdapter::addBookToCollection(qint64 bookId, qint64 collectionId)
{
    if (!m_backend->isOpen())
        return;

    const Librova::Domain::SBookId bid{static_cast<std::int64_t>(bookId)};
    const auto cid = static_cast<std::int64_t>(collectionId);
    auto ok = std::make_shared<bool>(false);
    m_backend->dispatch(
        [ok, bid, cid](auto& app) { *ok = app.AddBookToCollection(bid, cid); },
        [this, ok, bookId, collectionId]() {
            if (*ok)
            {
                qCInfo(lcCatalog) << "Book" << bookId << "added to collection" << collectionId;
                emit bookCollectionMembershipChanged(bookId);
            }
            else
            {
                qCWarning(lcCatalog) << "AddBookToCollection failed" << bookId << collectionId;
            }
        },
        [bookId, collectionId](const QString& error) {
            qCWarning(lcCatalog) << "AddBookToCollection failed:" << error
                                 << "bookId=" << bookId
                                 << "collectionId=" << collectionId;
        });
}

void QtCatalogAdapter::removeBookFromCollection(qint64 bookId, qint64 collectionId)
{
    if (!m_backend->isOpen())
        return;

    const Librova::Domain::SBookId bid{static_cast<std::int64_t>(bookId)};
    const auto cid = static_cast<std::int64_t>(collectionId);
    auto ok = std::make_shared<bool>(false);
    m_backend->dispatch(
        [ok, bid, cid](auto& app) { *ok = app.RemoveBookFromCollection(bid, cid); },
        [this, ok, bookId, collectionId]() {
            if (*ok)
            {
                qCInfo(lcCatalog) << "Book" << bookId << "removed from collection" << collectionId;
                emit bookCollectionMembershipChanged(bookId);
            }
            else
            {
                qCWarning(lcCatalog) << "RemoveBookFromCollection failed" << bookId << collectionId;
            }
        },
        [bookId, collectionId](const QString& error) {
            qCWarning(lcCatalog) << "RemoveBookFromCollection failed:" << error
                                 << "bookId=" << bookId
                                 << "collectionId=" << collectionId;
        });
}

// ── Private ───────────────────────────────────────────────────────────────────

void QtCatalogAdapter::dispatchRefresh(quint64 gen)
{
    auto result = std::make_shared<Librova::Application::SBookListResult>();
    const auto req = m_request;

    m_backend->dispatch(
        [result, req](auto& app) { *result = app.ListBooks(req); },
        [this, result, gen]() {
            if (gen != m_generation)
                return; // stale response, discard

            m_bookListModel->reset(result->Items);
            m_loadedCount = static_cast<int>(result->Items.size());
            m_languageFacets->reset(result->AvailableLanguages);
            m_genreFacets->reset(result->AvailableGenres);
            m_statistics = result->Statistics;

            if (m_totalCount != static_cast<qint64>(result->TotalCount))
            {
                m_totalCount = static_cast<qint64>(result->TotalCount);
                emit totalCountChanged();
            }

            setLoading(false);
            emit hasMoreResultsChanged();
            emit libraryStatisticsTextChanged();
            emit emptyStateChanged();
            qCInfo(lcCatalog) << "ListBooks completed: count=" << result->TotalCount
                              << "returned=" << result->Items.size();
            emit refreshed();
        },
        [this, gen](const QString& error) {
            if (gen != m_generation)
                return;

            setLoading(false);
            qCWarning(lcCatalog) << "ListBooks failed:" << error;
            emit refreshFailed(error);
        });
}

void QtCatalogAdapter::dispatchLoadMore(quint64 gen)
{
    auto result = std::make_shared<Librova::Application::SBookListResult>();
    auto req = m_request;
    req.Offset = static_cast<std::size_t>(m_loadedCount);
    req.Limit = static_cast<std::size_t>(m_pageSize);

    m_backend->dispatch(
        [result, req](auto& app) { *result = app.ListBooks(req); },
        [this, result, gen]() {
            if (gen != m_generation)
            {
                setLoadingMore(false);
                return;
            }

            m_bookListModel->append(result->Items);
            m_loadedCount += static_cast<int>(result->Items.size());
            if (m_totalCount != static_cast<qint64>(result->TotalCount))
            {
                m_totalCount = static_cast<qint64>(result->TotalCount);
                emit totalCountChanged();
            }

            setLoadingMore(false);
            emit hasMoreResultsChanged();
            qCInfo(lcCatalog) << "ListBooks load more completed: total=" << result->TotalCount
                              << "appended=" << result->Items.size()
                              << "loaded=" << m_loadedCount;
            emit refreshed();
        },
        [this, gen](const QString& error) {
            if (gen != m_generation)
            {
                return;
            }

            setLoadingMore(false);
            qCWarning(lcCatalog) << "ListBooks load more failed:" << error;
            emit refreshFailed(error);
        });
}

void QtCatalogAdapter::dispatchCollectionRefresh()
{
    auto collections = std::make_shared<std::vector<Librova::Domain::SBookCollection>>();
    m_backend->dispatch(
        [collections](auto& app) { *collections = app.ListCollections(); },
        [this, collections]() {
            m_collectionListModel->reset(*collections);
            qCInfo(lcCatalog) << "ListCollections completed: count=" << collections->size();
        },
        [](const QString& error) {
            qCWarning(lcCatalog) << "ListCollections failed:" << error;
        });
}

void QtCatalogAdapter::setLoading(bool v)
{
    if (m_loading == v)
        return;
    m_loading = v;
    emit loadingChanged();
    emit emptyStateChanged();
}

void QtCatalogAdapter::setLoadingMore(bool v)
{
    if (m_loadingMore == v)
    {
        return;
    }

    m_loadingMore = v;
    emit isLoadingMoreChanged();
}

void QtCatalogAdapter::createCollectionForBook(
    qint64 bookId,
    const QString& name,
    const QString& iconKey)
{
    if (!m_backend->isOpen())
    {
        return;
    }

    const Librova::Domain::SBookId bid{static_cast<std::int64_t>(bookId)};
    const std::string nameUtf8 = qtToUtf8(name);
    const std::string iconKeyUtf8 = qtToUtf8(iconKey);

    struct SCreateForBookResult
    {
        Librova::Domain::SBookCollection Collection;
        bool Created = false;
        bool Added = false;
        bool RolledBack = false;
        std::string ErrorUtf8;
    };

    auto result = std::make_shared<SCreateForBookResult>();
    m_backend->dispatch(
        [result, bid, nameUtf8, iconKeyUtf8](auto& app) {
            try
            {
                result->Collection = app.CreateCollection(nameUtf8, iconKeyUtf8);
                result->Created = result->Collection.Id > 0;
                if (!result->Created)
                {
                    return;
                }

                result->Added = app.AddBookToCollection(bid, result->Collection.Id);
            }
            catch (const std::exception& ex)
            {
                result->ErrorUtf8 = ex.what();
            }
            if (!result->Added)
            {
                result->RolledBack = app.DeleteCollection(result->Collection.Id);
            }
        },
        [this, result, bookId]() {
            if (result->Created && result->Added)
            {
                qCInfo(lcCatalog) << "Collection created for book:"
                                  << result->Collection.Id << "bookId=" << bookId;
                dispatchCollectionRefresh();
                emit collectionCreated(static_cast<qint64>(result->Collection.Id));
                emit bookCollectionMembershipChanged(bookId);
            }
            else
            {
                const QString message = result->Created
                    ? QStringLiteral("Created collection but could not add the book.")
                    : QStringLiteral("Could not create collection.");
                qCWarning(lcCatalog) << "CreateCollectionForBook failed:"
                                     << message << "bookId=" << bookId
                                     << "error=" << qtFromUtf8(result->ErrorUtf8)
                                     << "rolledBack=" << result->RolledBack;
                dispatchCollectionRefresh();
                emit collectionCreateFailed(message);
            }
        },
        [this, bookId](const QString& error) {
            qCWarning(lcCatalog) << "CreateCollectionForBook failed:" << error
                                 << "bookId=" << bookId;
            dispatchCollectionRefresh();
            emit collectionCreateFailed(error);
        });
}

void QtCatalogAdapter::setDetailsLoading(bool v)
{
    if (m_detailsLoading == v)
    {
        return;
    }
    m_detailsLoading = v;
    Q_EMIT detailsLoadingChanged();
}

} // namespace LibrovaQt
