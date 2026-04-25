#include <QtTest>

#include "Models/BookListModel.hpp"
#include "App/LibraryCatalogFacade.hpp"

using namespace LibrovaQt;
using namespace Librova::Application;

class TestBookListModel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void empty_model_has_zero_rows()
    {
        BookListModel model;
        QCOMPARE(model.rowCount(), 0);
    }

    void reset_with_items_updates_row_count()
    {
        BookListModel model;

        SBookListItem item;
        item.Id = {1};
        item.TitleUtf8 = "Test Book";
        item.Language = "ru";

        model.reset({item});
        QCOMPARE(model.rowCount(), 1);
    }

    void role_data_returns_title()
    {
        BookListModel model;

        SBookListItem item;
        item.Id = {42};
        item.TitleUtf8 = "Война и мир";

        model.reset({item});

        const QModelIndex idx = model.index(0);
        QCOMPARE(model.data(idx, BookListModel::TitleRole).toString(),
                 QStringLiteral("Война и мир"));
    }

    void role_data_returns_book_id()
    {
        BookListModel model;

        SBookListItem item;
        item.Id = {99};
        item.TitleUtf8 = "Book";

        model.reset({item});

        const QModelIndex idx = model.index(0);
        QCOMPARE(model.data(idx, BookListModel::BookIdRole).toLongLong(), 99LL);
    }

    void role_data_returns_authors_joined()
    {
        BookListModel model;

        SBookListItem item;
        item.Id = {1};
        item.AuthorsUtf8 = {"Толстой", "Другой"};

        model.reset({item});

        const QModelIndex idx = model.index(0);
        const QString authors = model.data(idx, BookListModel::AuthorsRole).toString();
        QVERIFY(authors.contains(QStringLiteral("Толстой")));
    }

    void role_data_returns_collection_memberships()
    {
        BookListModel model;

        SBookListItem item;
        item.Id = {1};
        item.Collections.push_back({
            .Id = 7,
            .NameUtf8 = "Избранное",
            .IconKey = "star",
            .Kind = Librova::Domain::EBookCollectionKind::User,
            .IsDeletable = true,
        });

        model.reset({item});

        const QModelIndex idx = model.index(0);
        const QVariantList ids = model.data(idx, BookListModel::CollectionIdsRole).toList();
        const QStringList names = model.data(idx, BookListModel::CollectionNamesRole).toStringList();

        QCOMPARE(ids.size(), 1);
        QCOMPARE(ids[0].toLongLong(), 7LL);
        QCOMPARE(names, QStringList{QStringLiteral("Избранное")});
    }

    void reset_clears_previous_items()
    {
        BookListModel model;

        SBookListItem item;
        item.Id = {1};
        model.reset({item, item});
        QCOMPARE(model.rowCount(), 2);

        model.reset({});
        QCOMPARE(model.rowCount(), 0);
    }

    void count_changed_signal_emitted_on_reset()
    {
        BookListModel model;
        QSignalSpy spy(&model, &BookListModel::countChanged);

        SBookListItem item;
        item.Id = {1};
        model.reset({item});

        QCOMPARE(spy.count(), 1);
    }

    void invalid_index_returns_null_variant()
    {
        BookListModel model;
        const QModelIndex idx = model.index(0); // out of range
        QVERIFY(!model.data(idx, BookListModel::TitleRole).isValid());
    }
};

QTEST_MAIN(TestBookListModel)
#include "TestBookListModel.moc"
