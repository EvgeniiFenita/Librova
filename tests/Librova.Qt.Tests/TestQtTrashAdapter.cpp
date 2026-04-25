#include <QtTest>

#include "Adapters/QtTrashAdapter.hpp"
#include "App/LibraryTrashFacade.hpp"
#include "TestHelpers/MockLibraryApplication.hpp"
#include "TestHelpers/SyncTestDispatcher.hpp"

using namespace LibrovaQt;
using namespace LibrovaQt::Tests;
using namespace Librova::Application;

class TestQtTrashAdapter : public QObject
{
    Q_OBJECT

private:
    MockLibraryApplication m_app;
    SyncTestDispatcher     m_dispatcher{&m_app};

private Q_SLOTS:
    void trash_success_recycle_bin()
    {
        STrashedBookResult result;
        result.BookId = {5};
        result.Destination = ETrashDestination::RecycleBin;
        result.HasOrphanedFiles = false;
        m_app.trashResult = result;

        QtTrashAdapter adapter(&m_dispatcher);
        QSignalSpy successSpy(&adapter, &QtTrashAdapter::trashed);
        QSignalSpy failSpy(&adapter, &QtTrashAdapter::trashFailed);

        adapter.moveToTrash(5LL);

        QCOMPARE(failSpy.count(), 0);
        QCOMPARE(successSpy.count(), 1);
        QCOMPARE(successSpy.at(0).at(0).toLongLong(), 5LL);
        QCOMPARE(successSpy.at(0).at(1).toString(), QStringLiteral("recycle_bin"));
        QCOMPARE(successSpy.at(0).at(2).toBool(), false);
    }

    void trash_success_managed_trash()
    {
        STrashedBookResult result;
        result.BookId = {6};
        result.Destination = ETrashDestination::ManagedTrash;
        result.HasOrphanedFiles = true;
        m_app.trashResult = result;

        QtTrashAdapter adapter(&m_dispatcher);
        QSignalSpy successSpy(&adapter, &QtTrashAdapter::trashed);

        adapter.moveToTrash(6LL);

        QCOMPARE(successSpy.count(), 1);
        QCOMPARE(successSpy.at(0).at(1).toString(), QStringLiteral("managed_trash"));
        QCOMPARE(successSpy.at(0).at(2).toBool(), true);
    }

    void trash_failure_emits_failed()
    {
        m_app.trashResult = std::nullopt;

        QtTrashAdapter adapter(&m_dispatcher);
        QSignalSpy successSpy(&adapter, &QtTrashAdapter::trashed);
        QSignalSpy failSpy(&adapter, &QtTrashAdapter::trashFailed);

        adapter.moveToTrash(7LL);

        QCOMPARE(successSpy.count(), 0);
        QCOMPARE(failSpy.count(), 1);
        QCOMPARE(failSpy.at(0).at(0).toLongLong(), 7LL);
    }

    void closed_dispatcher_emits_failed()
    {
        SyncTestDispatcher closed(nullptr);
        QtTrashAdapter adapter(&closed);
        QSignalSpy failSpy(&adapter, &QtTrashAdapter::trashFailed);

        adapter.moveToTrash(8LL);

        QCOMPARE(failSpy.count(), 1);
        QCOMPARE(failSpy.at(0).at(0).toLongLong(), 8LL);
    }
};

QTEST_MAIN(TestQtTrashAdapter)
#include "TestQtTrashAdapter.moc"
