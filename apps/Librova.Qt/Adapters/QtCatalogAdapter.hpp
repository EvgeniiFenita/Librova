#pragma once

#include <memory>

#include <QObject>
#include <QString>
#include <QStringList>

#include "App/IBackendDispatcher.hpp"
#include "App/LibraryCatalogFacade.hpp"
#include "Models/BookDetailsModel.hpp"
#include "Models/BookListModel.hpp"
#include "Models/CollectionListModel.hpp"
#include "Models/FacetListModel.hpp"

namespace LibrovaQt {

// Exposes catalog read/filter/sort operations and collection mutations to QML.
//
// Owns BookListModel, CollectionListModel, languageFacets, genreFacets.
// All backend calls go through IBackendDispatcher (→ worker thread).
// A generation counter discards stale completions from in-flight refreshes.
class QtCatalogAdapter : public QObject
{
    Q_OBJECT

    Q_PROPERTY(LibrovaQt::BookListModel*       bookListModel       READ bookListModel       CONSTANT)
    Q_PROPERTY(LibrovaQt::BookDetailsModel*    bookDetailsModel    READ bookDetailsModel    CONSTANT)
    Q_PROPERTY(LibrovaQt::CollectionListModel* collectionListModel READ collectionListModel CONSTANT)
    Q_PROPERTY(LibrovaQt::FacetListModel*      languageFacets      READ languageFacets      CONSTANT)
    Q_PROPERTY(LibrovaQt::FacetListModel*      genreFacets         READ genreFacets         CONSTANT)
    Q_PROPERTY(bool    loading    READ isLoading    NOTIFY loadingChanged)
    Q_PROPERTY(bool    detailsLoading READ isDetailsLoading NOTIFY detailsLoadingChanged)
    Q_PROPERTY(bool    isLoadingMore READ isLoadingMore NOTIFY isLoadingMoreChanged)
    Q_PROPERTY(qint64  activeCollectionId READ activeCollectionId NOTIFY activeCollectionIdChanged)
    Q_PROPERTY(qint64  totalCount READ totalCount   NOTIFY totalCountChanged)
    Q_PROPERTY(bool hasMoreResults READ hasMoreResults NOTIFY hasMoreResultsChanged)
    Q_PROPERTY(bool hasResultRestrictions READ hasResultRestrictions NOTIFY resultRestrictionsChanged)
    Q_PROPERTY(bool showGoToImportButton READ showGoToImportButton NOTIFY emptyStateChanged)
    Q_PROPERTY(bool showClearFiltersButton READ showClearFiltersButton NOTIFY emptyStateChanged)
    Q_PROPERTY(QString libraryStatisticsText READ libraryStatisticsText NOTIFY libraryStatisticsTextChanged)

public:
    explicit QtCatalogAdapter(IBackendDispatcher* backend, QObject* parent = nullptr);

    [[nodiscard]] BookListModel*       bookListModel()       const { return m_bookListModel;       }
    [[nodiscard]] BookDetailsModel*    bookDetailsModel()    const { return m_bookDetailsModel;    }
    [[nodiscard]] CollectionListModel* collectionListModel() const { return m_collectionListModel; }
    [[nodiscard]] FacetListModel*      languageFacets()      const { return m_languageFacets;      }
    [[nodiscard]] FacetListModel*      genreFacets()         const { return m_genreFacets;         }
    [[nodiscard]] bool                 isLoading()           const { return m_loading;             }
    [[nodiscard]] bool                 isDetailsLoading()    const { return m_detailsLoading;      }
    [[nodiscard]] bool                 isLoadingMore()       const { return m_loadingMore;         }
    [[nodiscard]] qint64               activeCollectionId()  const { return m_activeCollectionId; }
    [[nodiscard]] qint64               totalCount()          const { return m_totalCount;          }
    [[nodiscard]] bool                 hasMoreResults() const;
    [[nodiscard]] bool                 hasResultRestrictions() const;
    [[nodiscard]] bool                 showGoToImportButton() const;
    [[nodiscard]] bool                 showClearFiltersButton() const;
    [[nodiscard]] QString              libraryStatisticsText() const;

    // ── Catalog operations ────────────────────────────────────────────────────

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setSearchText(const QString& text);
    Q_INVOKABLE void setLanguageFilter(const QStringList& languages);
    Q_INVOKABLE void setGenreFilter(const QStringList& genres);
    Q_INVOKABLE void setCollectionFilter(qint64 collectionId);
    Q_INVOKABLE void clearCollectionFilter();
    Q_INVOKABLE void clearAllFilters();
    Q_INVOKABLE void setSortBy(const QString& sortBy, const QString& direction);
    Q_INVOKABLE void setPage(int offset, int limit);
    Q_INVOKABLE void loadMore();
    Q_INVOKABLE void loadBookDetails(qint64 bookId);
    Q_INVOKABLE void clearBookDetails();

    // ── Collection mutations ──────────────────────────────────────────────────

    Q_INVOKABLE void refreshCollections();
    Q_INVOKABLE void createCollection(const QString& name, const QString& iconKey);
    Q_INVOKABLE void createCollectionForBook(
        qint64 bookId,
        const QString& name,
        const QString& iconKey);
    Q_INVOKABLE void deleteCollection(qint64 collectionId);
    Q_INVOKABLE void addBookToCollection(qint64 bookId, qint64 collectionId);
    Q_INVOKABLE void removeBookFromCollection(qint64 bookId, qint64 collectionId);

Q_SIGNALS:
    void refreshed();
    void refreshFailed(const QString& error);
    void loadingChanged();
    void detailsLoadingChanged();
    void isLoadingMoreChanged();
    void activeCollectionIdChanged();
    void totalCountChanged();
    void hasMoreResultsChanged();
    void resultRestrictionsChanged();
    void emptyStateChanged();
    void libraryStatisticsTextChanged();
    void bookDetailsLoaded(qint64 bookId);
    void bookDetailsMissing(qint64 bookId);
    void collectionCreated(qint64 collectionId);
    void collectionCreateFailed(const QString& error);
    void collectionDeleted(qint64 collectionId);
    void bookCollectionMembershipChanged(qint64 bookId);

private:
    void dispatchRefresh(quint64 gen);
    void dispatchLoadMore(quint64 gen);
    void dispatchCollectionRefresh();
    void setLoading(bool v);
    void setLoadingMore(bool v);
    void setDetailsLoading(bool v);

    IBackendDispatcher*  m_backend;
    BookListModel*       m_bookListModel;
    BookDetailsModel*    m_bookDetailsModel;
    CollectionListModel* m_collectionListModel;
    FacetListModel*      m_languageFacets;
    FacetListModel*      m_genreFacets;

    Librova::Application::SBookListRequest m_request;
    bool    m_loading     = false;
    bool    m_detailsLoading = false;
    bool    m_loadingMore = false;
    qint64  m_activeCollectionId = -1;
    qint64  m_totalCount  = 0;
    int     m_pageSize = 120;
    int     m_loadedCount = 0;
    Librova::Application::SLibraryStatistics m_statistics;
    quint64 m_generation  = 0;
    quint64 m_detailsGeneration = 0;
};

} // namespace LibrovaQt
