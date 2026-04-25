#include "Models/BookListModel.hpp"

#include <QDateTime>

#include "Adapters/QtStringConversions.hpp"
#include "Domain/BookFormat.hpp"

namespace LibrovaQt {

BookListModel::BookListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void BookListModel::reset(const std::vector<Librova::Application::SBookListItem>& items)
{
    beginResetModel();
    m_items = items;
    endResetModel();
    emit countChanged();
}

void BookListModel::append(const std::vector<Librova::Application::SBookListItem>& items)
{
    if (items.empty())
    {
        return;
    }

    const int firstRow = static_cast<int>(m_items.size());
    const int lastRow = firstRow + static_cast<int>(items.size()) - 1;
    beginInsertRows({}, firstRow, lastRow);
    m_items.insert(m_items.end(), items.begin(), items.end());
    endInsertRows();
    emit countChanged();
}

int BookListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_items.size());
}

QVariant BookListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_items.size()))
        return {};

    const auto& item = m_items[static_cast<std::size_t>(index.row())];

    switch (role)
    {
        case BookIdRole:
            return static_cast<qint64>(item.Id.Value);

        case TitleRole:
            return qtFromUtf8(item.TitleUtf8);

        case AuthorsRole:
        {
            QStringList authors;
            authors.reserve(static_cast<qsizetype>(item.AuthorsUtf8.size()));
            for (const auto& a : item.AuthorsUtf8)
                authors.append(qtFromUtf8(a));
            return authors.join(QStringLiteral(", "));
        }

        case LanguageRole:
            return qtFromUtf8(item.Language);

        case SeriesNameRole:
            return item.SeriesUtf8.has_value() ? qtFromUtf8(*item.SeriesUtf8) : QString{};

        case SeriesIndexRole:
            return item.SeriesIndex.value_or(0.0);

        case YearRole:
            return item.Year.value_or(0);

        case GenresRole:
        {
            QStringList genres;
            genres.reserve(static_cast<qsizetype>(item.GenresUtf8.size()));
            for (const auto& g : item.GenresUtf8)
                genres.append(qtFromUtf8(g));
            return genres;
        }

        case FormatRole:
            return qtFromUtf8(std::string(Librova::Domain::ToString(item.Format)));

        case ManagedPathRole:
            return qtFromPath(item.ManagedPath);

        case CoverPathRole:
            return item.CoverPath.has_value() ? qtFromPath(*item.CoverPath) : QString{};

        case SizeBytesRole:
            return static_cast<qint64>(item.SizeBytes);

        case AddedAtRole:
        {
            const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                item.AddedAtUtc.time_since_epoch()).count();
            return QDateTime::fromMSecsSinceEpoch(ms, Qt::UTC).toString(Qt::ISODate);
        }

        case CollectionIdsRole:
        {
            QVariantList ids;
            ids.reserve(static_cast<qsizetype>(item.Collections.size()));
            for (const auto& collection : item.Collections)
                ids.append(static_cast<qint64>(collection.Id));
            return ids;
        }

        case CollectionNamesRole:
        {
            QStringList names;
            names.reserve(static_cast<qsizetype>(item.Collections.size()));
            for (const auto& collection : item.Collections)
                names.append(qtFromUtf8(collection.NameUtf8));
            return names;
        }

        default:
            return {};
    }
}

QHash<int, QByteArray> BookListModel::roleNames() const
{
    return {
        {BookIdRole,       "bookId"},
        {TitleRole,        "title"},
        {AuthorsRole,      "authors"},
        {LanguageRole,     "language"},
        {SeriesNameRole,   "seriesName"},
        {SeriesIndexRole,  "seriesIndex"},
        {YearRole,         "year"},
        {GenresRole,       "genres"},
        {FormatRole,       "format"},
        {ManagedPathRole,  "managedPath"},
        {CoverPathRole,    "coverPath"},
        {SizeBytesRole,    "sizeBytes"},
        {AddedAtRole,      "addedAt"},
        {CollectionIdsRole, "collectionIds"},
        {CollectionNamesRole, "collectionNames"},
    };
}

} // namespace LibrovaQt
