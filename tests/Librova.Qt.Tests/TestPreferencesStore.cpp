#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "Controllers/PreferencesStore.hpp"
#include "TestHelpers/TestWorkspace.hpp"

using LibrovaQt::PreferencesStore;

class TestPreferencesStore : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void freshStore_hasNoLibraryRoot()
    {
        PreferencesStore store;
        QVERIFY(!store.HasLibraryRoot());
        QVERIFY(store.libraryRoot().isEmpty());
    }

    void loadMissingFile_returnsFalseKeepsDefaults()
    {
        PreferencesStore store;
        QCOMPARE(store.Load(QStringLiteral("C:/nonexistent/prefs.json")), false);
        QVERIFY(!store.HasLibraryRoot());
    }

    void roundTrip_persitsAllFields()
    {
        QTemporaryDir tmp(LibrovaQt::Tests::TestWorkspaceTemplate(QStringLiteral("prefs")));
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("prefs.json"));

        PreferencesStore writer;
        writer.setLibraryRoot(QStringLiteral("C:/MyBooks"));
        writer.setPortableLibraryRoot(QStringLiteral("Books"));
        writer.setConverterExePath(QStringLiteral("C:/tools/fb2cng.exe"));
        writer.setForceEpubConversionOnImport(true);
        writer.setPreferredSortKey(QStringLiteral("date_added"));
        writer.setPreferredSortDescending(true);
        QVERIFY(writer.Save(path));

        PreferencesStore reader;
        QVERIFY(reader.Load(path));
        QCOMPARE(reader.libraryRoot(), QStringLiteral("C:/MyBooks"));
        QCOMPARE(reader.portableLibraryRoot(), QStringLiteral("Books"));
        QCOMPARE(reader.converterExePath(), QStringLiteral("C:/tools/fb2cng.exe"));
        QCOMPARE(reader.forceEpubConversionOnImport(), true);
        QCOMPARE(reader.preferredSortKey(), QStringLiteral("date_added"));
        QCOMPARE(reader.preferredSortDescending(), true);
    }

    void setLibraryRoot_emitsSignal()
    {
        PreferencesStore store;
        QSignalSpy spy(&store, &PreferencesStore::libraryRootChanged);
        store.setLibraryRoot(QStringLiteral("C:/Books"));
        QCOMPARE(spy.count(), 1);
    }

    void setLibraryRoot_sameValue_doesNotEmitSignal()
    {
        PreferencesStore store;
        store.setLibraryRoot(QStringLiteral("C:/Books"));
        QSignalSpy spy(&store, &PreferencesStore::libraryRootChanged);
        store.setLibraryRoot(QStringLiteral("C:/Books"));
        QCOMPARE(spy.count(), 0);
    }

    void hasLibraryRoot_trueAfterSetting()
    {
        PreferencesStore store;
        store.setLibraryRoot(QStringLiteral("C:/Books"));
        QVERIFY(store.HasLibraryRoot());
    }

    void invalidPreferredSortKey_fallsBackToTitle()
    {
        PreferencesStore store;
        QSignalSpy spy(&store, &PreferencesStore::preferredSortKeyChanged);

        store.setPreferredSortKey(QStringLiteral("invalid"));

        QCOMPARE(store.preferredSortKey(), QStringLiteral("title"));
        QCOMPARE(spy.count(), 0);
    }

    void setPreferredSortKey_emitsSignal()
    {
        PreferencesStore store;
        QSignalSpy spy(&store, &PreferencesStore::preferredSortKeyChanged);

        store.setPreferredSortKey(QStringLiteral("author"));

        QCOMPARE(store.preferredSortKey(), QStringLiteral("author"));
        QCOMPARE(spy.count(), 1);
    }

    void loadInvalidJson_returnsFalse()
    {
        QTemporaryDir tmp(LibrovaQt::Tests::TestWorkspaceTemplate(QStringLiteral("prefs")));
        QVERIFY(tmp.isValid());
        const QString path = tmp.filePath(QStringLiteral("bad.json"));
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("not json");
        f.close();

        PreferencesStore store;
        QCOMPARE(store.Load(path), false);
    }
};

QTEST_APPLESS_MAIN(TestPreferencesStore)
#include "TestPreferencesStore.moc"
