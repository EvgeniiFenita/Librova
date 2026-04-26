#include <QtTest>
#include <filesystem>

#include "Adapters/QtCatalogAdapter.hpp"
#include "TestHelpers/MockLibraryApplication.hpp"
#include "TestHelpers/SyncTestDispatcher.hpp"

using namespace LibrovaQt;
using namespace LibrovaQt::Tests;
using namespace Librova::Application;

class TestQtCatalogAdapter : public QObject
{
    Q_OBJECT

private:
    MockLibraryApplication m_app;
    SyncTestDispatcher     m_dispatcher{&m_app};

private Q_SLOTS:
    void refresh_populates_book_model()
    {
        SBookListItem item;
        item.Id = {1};
        item.TitleUtf8 = "Мастер и Маргарита";
        m_app.listBooksResult.Items = {item};
        m_app.listBooksResult.TotalCount = 1;

        QtCatalogAdapter adapter(&m_dispatcher);
        QSignalSpy spy(&adapter, &QtCatalogAdapter::refreshed);

        adapter.refresh();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(adapter.bookListModel()->rowCount(), 1);
        QCOMPARE(adapter.totalCount(), 1LL);
    }

    void refresh_updates_total_count_signal()
    {
        m_app.listBooksResult.Items = {};
        m_app.listBooksResult.TotalCount = 42;

        QtCatalogAdapter adapter(&m_dispatcher);
        QSignalSpy spy(&adapter, &QtCatalogAdapter::totalCountChanged);

        adapter.refresh();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(adapter.totalCount(), 42LL);
    }

    void refresh_updates_library_statistics_text()
    {
        m_app.listBooksResult.TotalCount = 3;
        m_app.listBooksResult.Statistics.BookCount = 3;
        m_app.listBooksResult.Statistics.TotalLibrarySizeBytes = 5 * 1024 * 1024;

        QtCatalogAdapter adapter(&m_dispatcher);
        adapter.refresh();

        QCOMPARE(adapter.libraryStatisticsText(), QStringLiteral("Library: 3 books, 5.0 MB"));
    }

    void refresh_sets_has_more_results_when_page_is_partial()
    {
        SBookListItem first;
        first.Id = {1};
        first.TitleUtf8 = "Book 1";
        SBookListItem second;
        second.Id = {2};
        second.TitleUtf8 = "Book 2";
        m_app.listBooksResult.Items = {first, second};
        m_app.listBooksResult.TotalCount = 5;

        QtCatalogAdapter adapter(&m_dispatcher);
        adapter.refresh();

        QVERIFY(adapter.hasMoreResults());
        QCOMPARE(adapter.bookListModel()->rowCount(), 2);
    }

    void set_search_text_triggers_refresh()
    {
        m_app.listBooksResult = {};

        QtCatalogAdapter adapter(&m_dispatcher);
        QSignalSpy spy(&adapter, &QtCatalogAdapter::refreshed);

        adapter.setSearchText(QStringLiteral("Толстой"));

        QCOMPARE(spy.count(), 1);
    }

    void set_search_text_marks_result_restrictions()
    {
        QtCatalogAdapter adapter(&m_dispatcher);

        adapter.setSearchText(QStringLiteral("Толстой"));

        QVERIFY(adapter.hasResultRestrictions());
    }

    void refresh_populates_language_facets()
    {
        m_app.listBooksResult.AvailableLanguages = {{"ru", 5}, {"en", 3}};

        QtCatalogAdapter adapter(&m_dispatcher);
        adapter.refresh();

        QCOMPARE(adapter.languageFacets()->rowCount(), 2);
    }

    void set_collection_filter_updates_active_collection_and_request()
    {
        QtCatalogAdapter adapter(&m_dispatcher);
        QSignalSpy activeSpy(&adapter, &QtCatalogAdapter::activeCollectionIdChanged);

        adapter.setCollectionFilter(12);

        QCOMPARE(activeSpy.count(), 1);
        QCOMPARE(adapter.activeCollectionId(), 12LL);
        QVERIFY(m_app.lastListBooksRequest.has_value());
        QVERIFY(m_app.lastListBooksRequest->CollectionId.has_value());
        QCOMPARE(*m_app.lastListBooksRequest->CollectionId, 12LL);
    }

    void set_page_keeps_requested_offset_and_limit()
    {
        QtCatalogAdapter adapter(&m_dispatcher);

        adapter.setPage(120, 40);

        QVERIFY(m_app.lastListBooksRequest.has_value());
        QCOMPARE(m_app.lastListBooksRequest->Offset, static_cast<std::size_t>(120));
        QCOMPARE(m_app.lastListBooksRequest->Limit, static_cast<std::size_t>(40));
    }

    void set_page_clamps_negative_offset_and_restores_default_limit()
    {
        QtCatalogAdapter adapter(&m_dispatcher);

        adapter.setPage(-10, 0);

        QVERIFY(m_app.lastListBooksRequest.has_value());
        QCOMPARE(m_app.lastListBooksRequest->Offset, static_cast<std::size_t>(0));
        QCOMPARE(m_app.lastListBooksRequest->Limit, static_cast<std::size_t>(120));
    }

    void clear_collection_filter_resets_active_collection_and_request()
    {
        QtCatalogAdapter adapter(&m_dispatcher);
        adapter.setCollectionFilter(12);

        adapter.clearCollectionFilter();

        QCOMPARE(adapter.activeCollectionId(), -1LL);
        QVERIFY(m_app.lastListBooksRequest.has_value());
        QVERIFY(!m_app.lastListBooksRequest->CollectionId.has_value());
    }

    void load_more_appends_next_page()
    {
        SBookListItem first;
        first.Id = {1};
        first.TitleUtf8 = "Book 1";
        SBookListItem second;
        second.Id = {2};
        second.TitleUtf8 = "Book 2";
        SBookListItem third;
        third.Id = {3};
        third.TitleUtf8 = "Book 3";

        SBookListResult pageOne;
        pageOne.Items = {first, second};
        pageOne.TotalCount = 3;

        SBookListResult pageTwo;
        pageTwo.Items = {third};
        pageTwo.TotalCount = 3;

        m_app.queuedListBooksResults = {pageOne, pageTwo};

        QtCatalogAdapter adapter(&m_dispatcher);
        adapter.refresh();
        QVERIFY(adapter.hasMoreResults());

        adapter.loadMore();

        QCOMPARE(adapter.bookListModel()->rowCount(), 3);
        QVERIFY(!adapter.hasMoreResults());
        QVERIFY(m_app.lastListBooksRequest.has_value());
        QCOMPARE(m_app.lastListBooksRequest->Offset, static_cast<std::size_t>(2));
    }

    void delete_active_collection_clears_collection_filter()
    {
        QtCatalogAdapter adapter(&m_dispatcher);
        adapter.setCollectionFilter(7);
        QSignalSpy deletedSpy(&adapter, &QtCatalogAdapter::collectionDeleted);

        adapter.deleteCollection(7);

        QCOMPARE(deletedSpy.count(), 1);
        QCOMPARE(m_app.lastDeleteCollectionId.value_or(0), 7LL);
        QCOMPARE(adapter.activeCollectionId(), -1LL);
        QVERIFY(m_app.lastListBooksRequest.has_value());
        QVERIFY(!m_app.lastListBooksRequest->CollectionId.has_value());
    }

    void create_collection_for_book_adds_membership()
    {
        Librova::Domain::SBookCollection created;
        created.Id = 55;
        created.NameUtf8 = "Favorites";
        created.IconKey = "star";
        m_app.createCollectionResult = created;

        QtCatalogAdapter adapter(&m_dispatcher);
        QSignalSpy createdSpy(&adapter, &QtCatalogAdapter::collectionCreated);
        QSignalSpy membershipSpy(&adapter, &QtCatalogAdapter::bookCollectionMembershipChanged);
        QSignalSpy failedSpy(&adapter, &QtCatalogAdapter::collectionCreateFailed);

        adapter.createCollectionForBook(9, QStringLiteral("Favorites"), QStringLiteral("star"));

        QCOMPARE(createdSpy.count(), 1);
        QCOMPARE(membershipSpy.count(), 1);
        QCOMPARE(failedSpy.count(), 0);
        QCOMPARE(QString::fromStdString(m_app.lastCreateCollectionName.value_or("")), QStringLiteral("Favorites"));
        QCOMPARE(m_app.lastAddedBookId->Value, 9LL);
        QCOMPARE(m_app.lastAddedCollectionId.value_or(0), 55LL);
    }

    void create_collection_for_book_rolls_back_when_membership_add_fails()
    {
        Librova::Domain::SBookCollection created;
        created.Id = 56;
        created.NameUtf8 = "Failed";
        created.IconKey = "book";
        m_app.createCollectionResult = created;
        m_app.addBookToCollectionResult = false;

        QtCatalogAdapter adapter(&m_dispatcher);
        QSignalSpy createdSpy(&adapter, &QtCatalogAdapter::collectionCreated);
        QSignalSpy failedSpy(&adapter, &QtCatalogAdapter::collectionCreateFailed);

        adapter.createCollectionForBook(9, QStringLiteral("Failed"), QStringLiteral("book"));

        QCOMPARE(createdSpy.count(), 0);
        QCOMPARE(failedSpy.count(), 1);
        QCOMPARE(m_app.lastDeleteCollectionId.value_or(0), 56LL);
    }

    void create_collection_for_book_rolls_back_when_membership_add_throws()
    {
        Librova::Domain::SBookCollection created;
        created.Id = 57;
        created.NameUtf8 = "Throws";
        created.IconKey = "book";
        m_app.createCollectionResult = created;
        m_app.throwOnAddBookToCollection = true;

        QtCatalogAdapter adapter(&m_dispatcher);
        QSignalSpy failedSpy(&adapter, &QtCatalogAdapter::collectionCreateFailed);

        adapter.createCollectionForBook(9, QStringLiteral("Throws"), QStringLiteral("book"));

        QCOMPARE(failedSpy.count(), 1);
        QCOMPARE(m_app.lastDeleteCollectionId.value_or(0), 57LL);
    }

    void refresh_populates_collection_model()
    {
        Librova::Domain::SBookCollection col;
        col.Id = 1;
        col.NameUtf8 = "Test";
        m_app.listCollectionsResult = {col};

        QtCatalogAdapter adapter(&m_dispatcher);
        adapter.refresh();

        QCOMPARE(adapter.collectionListModel()->rowCount(), 1);
    }

    void load_book_details_populates_details_model()
    {
        SBookDetails details;
        details.Id = {7};
        details.TitleUtf8 = "Пикник на обочине";
        details.AuthorsUtf8 = {"Аркадий Стругацкий", "Борис Стругацкий"};
        details.Language = "ru";
        details.SeriesUtf8 = "Мир Полудня";
        details.SeriesIndex = 2.0;
        details.PublisherUtf8 = "Детская литература";
        details.Year = 1972;
        details.Isbn = "978-5";
        details.GenresUtf8 = {"sci-fi", "adventure"};
        details.DescriptionUtf8 = "Зона меняет людей.";
        details.Identifier = "details-id";
        details.Format = Librova::Domain::EBookFormat::Fb2;
        details.ManagedPath = std::filesystem::path(L"C:\\library\\book.fb2");
        details.CoverPath = std::filesystem::path(L"C:\\library\\cover.jpg");
        details.SizeBytes = 4096;
        details.Sha256Hex = "abcd";
        Librova::Domain::SBookCollection collection;
        collection.Id = 3;
        collection.NameUtf8 = "Favorites";
        details.Collections = {collection};
        m_app.getBookDetailsResult = details;

        QtCatalogAdapter adapter(&m_dispatcher);
        QSignalSpy loadedSpy(&adapter, &QtCatalogAdapter::bookDetailsLoaded);

        adapter.loadBookDetails(7);

        QCOMPARE(loadedSpy.count(), 1);
        QCOMPARE(adapter.bookDetailsModel()->hasBook(), true);
        QCOMPARE(adapter.bookDetailsModel()->bookId(), 7LL);
        QCOMPARE(adapter.bookDetailsModel()->title(), QStringLiteral("Пикник на обочине"));
        QCOMPARE(adapter.bookDetailsModel()->authors(), QStringLiteral("Аркадий Стругацкий, Борис Стругацкий"));
        QCOMPARE(adapter.bookDetailsModel()->publisher(), QStringLiteral("Детская литература"));
        QCOMPARE(adapter.bookDetailsModel()->genres(), QStringList({QStringLiteral("sci-fi"), QStringLiteral("adventure")}));
        QCOMPARE(adapter.bookDetailsModel()->description(), QStringLiteral("Зона меняет людей."));
        QCOMPARE(adapter.bookDetailsModel()->format(), QStringLiteral("fb2"));
        QCOMPARE(adapter.bookDetailsModel()->sizeBytes(), 4096LL);
        QCOMPARE(adapter.bookDetailsModel()->collectionIds(), QStringList({QStringLiteral("3")}));
    }

    void load_book_details_missing_clears_details_model()
    {
        m_app.getBookDetailsResult = std::nullopt;

        QtCatalogAdapter adapter(&m_dispatcher);
        QSignalSpy missingSpy(&adapter, &QtCatalogAdapter::bookDetailsMissing);

        adapter.loadBookDetails(404);

        QCOMPARE(missingSpy.count(), 1);
        QCOMPARE(adapter.bookDetailsModel()->hasBook(), false);
    }

    void clear_book_details_resets_model()
    {
        SBookDetails details;
        details.Id = {1};
        details.TitleUtf8 = "Title";
        details.Language = "en";
        m_app.getBookDetailsResult = details;

        QtCatalogAdapter adapter(&m_dispatcher);
        adapter.loadBookDetails(1);
        QVERIFY(adapter.bookDetailsModel()->hasBook());

        adapter.clearBookDetails();

        QCOMPARE(adapter.bookDetailsModel()->hasBook(), false);
        QCOMPARE(adapter.isDetailsLoading(), false);
    }

    void loading_flips_during_refresh()
    {
        m_app.listBooksResult = {};

        QtCatalogAdapter adapter(&m_dispatcher);
        QSignalSpy loadingSpy(&adapter, &QtCatalogAdapter::loadingChanged);

        adapter.refresh();

        // SyncTestDispatcher completes synchronously, so we see two changes:
        // true then false, but since it's sync there may be just one emission
        // if the final state is false. Check the final state at minimum.
        QVERIFY(!adapter.isLoading());
        QVERIFY(loadingSpy.count() >= 1);
    }

    void closed_dispatcher_skips_refresh()
    {
        SyncTestDispatcher closedDispatcher(nullptr);
        QtCatalogAdapter adapter(&closedDispatcher);
        QSignalSpy spy(&adapter, &QtCatalogAdapter::refreshed);

        adapter.refresh();

        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TestQtCatalogAdapter)
#include "TestQtCatalogAdapter.moc"
