#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "Controllers/ShellStateStore.hpp"
#include "TestHelpers/TestWorkspace.hpp"

using LibrovaQt::ShellStateStore;

class TestShellStateStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void freshStore_hasDefaults()
    {
        ShellStateStore store;
        QCOMPARE(store.windowWidth(), 1280);
        QCOMPARE(store.windowHeight(), 800);
        QCOMPARE(store.maximized(), false);
        QCOMPARE(store.lastSection(), QStringLiteral("library"));
        QCOMPARE(store.allowProbableDuplicates(), false);
        QCOMPARE(store.sourcePaths(), QStringList{});
    }

    void loadMissingFile_returnsFalseKeepsDefaults()
    {
        ShellStateStore store;
        QCOMPARE(store.Load(QStringLiteral("C:/nonexistent/state.json")), false);
        QCOMPARE(store.windowWidth(), 1280);
    }

    void roundTrip_persistsAllFields()
    {
        QTemporaryDir tmp(LibrovaQt::Tests::TestWorkspaceTemplate(QStringLiteral("state")));
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("state.json"));

        ShellStateStore writer;
        writer.setWindowX(50);
        writer.setWindowY(60);
        writer.setWindowWidth(1600);
        writer.setWindowHeight(900);
        writer.setMaximized(true);
        writer.setLastSection(QStringLiteral("import"));
        writer.setAllowProbableDuplicates(true);
        writer.setSourcePaths({
            QStringLiteral("C:/Incoming/book.fb2"),
            QStringLiteral("C:/Incoming/batch.zip"),
        });
        QVERIFY(writer.Save(path));

        ShellStateStore reader;
        QVERIFY(reader.Load(path));
        QCOMPARE(reader.windowX(), 50);
        QCOMPARE(reader.windowY(), 60);
        QCOMPARE(reader.windowWidth(), 1600);
        QCOMPARE(reader.windowHeight(), 900);
        QCOMPARE(reader.maximized(), true);
        QCOMPARE(reader.lastSection(), QStringLiteral("import"));
        QCOMPARE(reader.allowProbableDuplicates(), true);
        QCOMPARE(reader.sourcePaths(), QStringList({
            QStringLiteral("C:/Incoming/book.fb2"),
            QStringLiteral("C:/Incoming/batch.zip"),
        }));
    }

    void setWindowWidth_emitsSignal()
    {
        ShellStateStore store;
        QSignalSpy spy(&store, &ShellStateStore::windowWidthChanged);
        store.setWindowWidth(1920);
        QCOMPARE(spy.count(), 1);
    }

    void setWindowWidth_sameValue_doesNotEmitSignal()
    {
        ShellStateStore store;
        QSignalSpy spy(&store, &ShellStateStore::windowWidthChanged);
        store.setWindowWidth(store.windowWidth());
        QCOMPARE(spy.count(), 0);
    }

    void setSourcePaths_emitsSignal()
    {
        ShellStateStore store;
        QSignalSpy spy(&store, &ShellStateStore::sourcePathsChanged);

        store.setSourcePaths({QStringLiteral("C:/Incoming/book.fb2")});

        QCOMPARE(spy.count(), 1);
    }
};

QTEST_APPLESS_MAIN(TestShellStateStore)
#include "TestShellStateStore.moc"
