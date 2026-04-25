#include "Controllers/PreferencesStore.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace LibrovaQt {

PreferencesStore::PreferencesStore(QObject* parent)
    : QObject(parent)
{
}

bool PreferencesStore::Load(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull() || !doc.isObject())
    {
        return false;
    }

    const QJsonObject root = doc.object();
    if (root.contains(QLatin1String("libraryRoot")))
    {
        setLibraryRoot(root[QLatin1String("libraryRoot")].toString());
    }
    if (root.contains(QLatin1String("portableLibraryRoot")))
    {
        setPortableLibraryRoot(root[QLatin1String("portableLibraryRoot")].toString());
    }
    if (root.contains(QLatin1String("converterExePath")))
    {
        setConverterExePath(root[QLatin1String("converterExePath")].toString());
    }
    if (root.contains(QLatin1String("forceEpubConversionOnImport")))
    {
        setForceEpubConversionOnImport(
            root[QLatin1String("forceEpubConversionOnImport")].toBool(false));
    }
    if (root.contains(QLatin1String("preferredSortKey")))
    {
        setPreferredSortKey(root[QLatin1String("preferredSortKey")].toString(QStringLiteral("title")));
    }
    if (root.contains(QLatin1String("preferredSortDescending")))
    {
        setPreferredSortDescending(root[QLatin1String("preferredSortDescending")].toBool(false));
    }
    return true;
}

bool PreferencesStore::Save(const QString& filePath) const
{
    QJsonObject root;
    root[QLatin1String("libraryRoot")] = m_libraryRoot;
    root[QLatin1String("portableLibraryRoot")] = m_portableLibraryRoot;
    root[QLatin1String("converterExePath")] = m_converterExePath;
    root[QLatin1String("forceEpubConversionOnImport")] = m_forceEpubConversionOnImport;
    root[QLatin1String("preferredSortKey")] = m_preferredSortKey;
    root[QLatin1String("preferredSortDescending")] = m_preferredSortDescending;

    QDir().mkpath(QFileInfo(filePath).absolutePath());
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        return false;
    }

    if (file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0)
    {
        file.cancelWriting();
        return false;
    }

    return file.commit();
}

bool PreferencesStore::HasLibraryRoot() const
{
    return !m_libraryRoot.isEmpty();
}

QString PreferencesStore::libraryRoot() const { return m_libraryRoot; }
void PreferencesStore::setLibraryRoot(const QString& value)
{
    if (m_libraryRoot != value)
    {
        m_libraryRoot = value;
        Q_EMIT libraryRootChanged();
    }
}

QString PreferencesStore::portableLibraryRoot() const { return m_portableLibraryRoot; }
void PreferencesStore::setPortableLibraryRoot(const QString& value)
{
    if (m_portableLibraryRoot != value)
    {
        m_portableLibraryRoot = value;
        Q_EMIT portableLibraryRootChanged();
    }
}

QString PreferencesStore::converterExePath() const { return m_converterExePath; }
void PreferencesStore::setConverterExePath(const QString& value)
{
    if (m_converterExePath != value)
    {
        m_converterExePath = value;
        Q_EMIT converterExePathChanged();
    }
}

bool PreferencesStore::forceEpubConversionOnImport() const
{
    return m_forceEpubConversionOnImport;
}

void PreferencesStore::setForceEpubConversionOnImport(bool value)
{
    if (m_forceEpubConversionOnImport != value)
    {
        m_forceEpubConversionOnImport = value;
        Q_EMIT forceEpubConversionOnImportChanged();
    }
}

QString PreferencesStore::preferredSortKey() const
{
    return m_preferredSortKey;
}

void PreferencesStore::setPreferredSortKey(const QString& value)
{
    const QString normalized = value == QLatin1String("author")
            || value == QLatin1String("date_added")
        ? value
        : QStringLiteral("title");
    if (m_preferredSortKey != normalized)
    {
        m_preferredSortKey = normalized;
        Q_EMIT preferredSortKeyChanged();
    }
}

bool PreferencesStore::preferredSortDescending() const
{
    return m_preferredSortDescending;
}

void PreferencesStore::setPreferredSortDescending(bool value)
{
    if (m_preferredSortDescending != value)
    {
        m_preferredSortDescending = value;
        Q_EMIT preferredSortDescendingChanged();
    }
}

} // namespace LibrovaQt
