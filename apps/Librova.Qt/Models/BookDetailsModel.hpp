#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include "App/LibraryCatalogFacade.hpp"

namespace LibrovaQt {

class BookDetailsModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool hasBook READ hasBook NOTIFY changed)
    Q_PROPERTY(qint64 bookId READ bookId NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString authors READ authors NOTIFY changed)
    Q_PROPERTY(QString language READ language NOTIFY changed)
    Q_PROPERTY(QString seriesName READ seriesName NOTIFY changed)
    Q_PROPERTY(double seriesIndex READ seriesIndex NOTIFY changed)
    Q_PROPERTY(QString publisher READ publisher NOTIFY changed)
    Q_PROPERTY(int year READ year NOTIFY changed)
    Q_PROPERTY(QString isbn READ isbn NOTIFY changed)
    Q_PROPERTY(QStringList tags READ tags NOTIFY changed)
    Q_PROPERTY(QStringList genres READ genres NOTIFY changed)
    Q_PROPERTY(QString description READ description NOTIFY changed)
    Q_PROPERTY(QString identifier READ identifier NOTIFY changed)
    Q_PROPERTY(QString format READ format NOTIFY changed)
    Q_PROPERTY(QString managedPath READ managedPath NOTIFY changed)
    Q_PROPERTY(QString coverPath READ coverPath NOTIFY changed)
    Q_PROPERTY(qint64 sizeBytes READ sizeBytes NOTIFY changed)
    Q_PROPERTY(QString sha256Hex READ sha256Hex NOTIFY changed)
    Q_PROPERTY(QStringList collectionIds READ collectionIds NOTIFY changed)
    Q_PROPERTY(QStringList collectionNames READ collectionNames NOTIFY changed)

public:
    explicit BookDetailsModel(QObject* parent = nullptr);

    void clear();
    void setDetails(const Librova::Application::SBookDetails& details);

    [[nodiscard]] bool hasBook() const { return m_hasBook; }
    [[nodiscard]] qint64 bookId() const { return m_bookId; }
    [[nodiscard]] QString title() const { return m_title; }
    [[nodiscard]] QString authors() const { return m_authors; }
    [[nodiscard]] QString language() const { return m_language; }
    [[nodiscard]] QString seriesName() const { return m_seriesName; }
    [[nodiscard]] double seriesIndex() const { return m_seriesIndex; }
    [[nodiscard]] QString publisher() const { return m_publisher; }
    [[nodiscard]] int year() const { return m_year; }
    [[nodiscard]] QString isbn() const { return m_isbn; }
    [[nodiscard]] QStringList tags() const { return m_tags; }
    [[nodiscard]] QStringList genres() const { return m_genres; }
    [[nodiscard]] QString description() const { return m_description; }
    [[nodiscard]] QString identifier() const { return m_identifier; }
    [[nodiscard]] QString format() const { return m_format; }
    [[nodiscard]] QString managedPath() const { return m_managedPath; }
    [[nodiscard]] QString coverPath() const { return m_coverPath; }
    [[nodiscard]] qint64 sizeBytes() const { return m_sizeBytes; }
    [[nodiscard]] QString sha256Hex() const { return m_sha256Hex; }
    [[nodiscard]] QStringList collectionIds() const { return m_collectionIds; }
    [[nodiscard]] QStringList collectionNames() const { return m_collectionNames; }

Q_SIGNALS:
    void changed();

private:
    bool m_hasBook = false;
    qint64 m_bookId = 0;
    QString m_title;
    QString m_authors;
    QString m_language;
    QString m_seriesName;
    double m_seriesIndex = 0.0;
    QString m_publisher;
    int m_year = 0;
    QString m_isbn;
    QStringList m_tags;
    QStringList m_genres;
    QString m_description;
    QString m_identifier;
    QString m_format;
    QString m_managedPath;
    QString m_coverPath;
    qint64 m_sizeBytes = 0;
    QString m_sha256Hex;
    QStringList m_collectionIds;
    QStringList m_collectionNames;
};

} // namespace LibrovaQt
