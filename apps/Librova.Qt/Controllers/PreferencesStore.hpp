#pragma once

#include <QObject>
#include <QString>

namespace LibrovaQt {

// Persists user preferences to a JSON file.
// All properties survive application restarts.
class PreferencesStore : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString libraryRoot READ libraryRoot WRITE setLibraryRoot NOTIFY libraryRootChanged)
    Q_PROPERTY(
        QString portableLibraryRoot READ portableLibraryRoot WRITE setPortableLibraryRoot
            NOTIFY portableLibraryRootChanged)
    Q_PROPERTY(
        QString converterExePath READ converterExePath WRITE setConverterExePath
            NOTIFY converterExePathChanged)
    Q_PROPERTY(
        bool forceEpubConversionOnImport READ forceEpubConversionOnImport
            WRITE setForceEpubConversionOnImport NOTIFY forceEpubConversionOnImportChanged)
    Q_PROPERTY(
        QString preferredSortKey READ preferredSortKey WRITE setPreferredSortKey
            NOTIFY preferredSortKeyChanged)
    Q_PROPERTY(
        bool preferredSortDescending READ preferredSortDescending WRITE setPreferredSortDescending
            NOTIFY preferredSortDescendingChanged)

public:
    explicit PreferencesStore(QObject* parent = nullptr);

    // Returns false if the file does not exist or cannot be parsed; keeps defaults in that case.
    [[nodiscard]] bool Load(const QString& filePath);
    [[nodiscard]] bool Save(const QString& filePath) const;

    [[nodiscard]] bool HasLibraryRoot() const;

    [[nodiscard]] QString libraryRoot() const;
    void setLibraryRoot(const QString& value);

    [[nodiscard]] QString portableLibraryRoot() const;
    void setPortableLibraryRoot(const QString& value);

    [[nodiscard]] QString converterExePath() const;
    void setConverterExePath(const QString& value);

    [[nodiscard]] bool forceEpubConversionOnImport() const;
    void setForceEpubConversionOnImport(bool value);

    [[nodiscard]] QString preferredSortKey() const;
    void setPreferredSortKey(const QString& value);

    [[nodiscard]] bool preferredSortDescending() const;
    void setPreferredSortDescending(bool value);

Q_SIGNALS:
    void libraryRootChanged();
    void portableLibraryRootChanged();
    void converterExePathChanged();
    void forceEpubConversionOnImportChanged();
    void preferredSortKeyChanged();
    void preferredSortDescendingChanged();

private:
    QString m_libraryRoot;
    QString m_portableLibraryRoot;
    QString m_converterExePath;
    bool m_forceEpubConversionOnImport = false;
    QString m_preferredSortKey = QStringLiteral("title");
    bool m_preferredSortDescending = false;
};

} // namespace LibrovaQt
