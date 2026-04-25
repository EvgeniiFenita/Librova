#include <QtTest>

#include "Models/CollectionListModel.hpp"
#include "Domain/BookCollection.hpp"

using namespace LibrovaQt;
using namespace Librova::Domain;

class TestCollectionListModel : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void empty_model_has_zero_rows()
    {
        CollectionListModel model;
        QCOMPARE(model.rowCount(), 0);
    }

    void reset_populates_row_count()
    {
        CollectionListModel model;

        SBookCollection col;
        col.Id = 1;
        col.NameUtf8 = "Favourites";
        col.IconKey = "heart";

        model.reset({col});
        QCOMPARE(model.rowCount(), 1);
    }

    void role_data_returns_name()
    {
        CollectionListModel model;

        SBookCollection col;
        col.Id = 7;
        col.NameUtf8 = "Любимые";

        model.reset({col});

        const QModelIndex idx = model.index(0);
        QCOMPARE(model.data(idx, CollectionListModel::NameRole).toString(),
                 QStringLiteral("Любимые"));
    }

    void role_data_returns_collection_id()
    {
        CollectionListModel model;

        SBookCollection col;
        col.Id = 42;

        model.reset({col});

        const QModelIndex idx = model.index(0);
        QCOMPARE(model.data(idx, CollectionListModel::CollectionIdRole).toLongLong(), 42LL);
    }

    void reset_replaces_contents()
    {
        CollectionListModel model;

        SBookCollection a, b;
        a.Id = 1; b.Id = 2;
        model.reset({a, b});
        QCOMPARE(model.rowCount(), 2);

        model.reset({a});
        QCOMPARE(model.rowCount(), 1);
    }

    void count_changed_signal_emitted()
    {
        CollectionListModel model;
        QSignalSpy spy(&model, &CollectionListModel::countChanged);

        SBookCollection col;
        col.Id = 1;
        model.reset({col});

        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(TestCollectionListModel)
#include "TestCollectionListModel.moc"
