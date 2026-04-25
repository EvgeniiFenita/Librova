#include "Models/CollectionListModel.hpp"

#include "Adapters/QtStringConversions.hpp"

namespace LibrovaQt {

CollectionListModel::CollectionListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void CollectionListModel::reset(const std::vector<Librova::Domain::SBookCollection>& collections)
{
    beginResetModel();
    m_items = collections;
    endResetModel();
    emit countChanged();
}

int CollectionListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_items.size());
}

QVariant CollectionListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_items.size()))
        return {};

    const auto& item = m_items[static_cast<std::size_t>(index.row())];

    switch (role)
    {
        case CollectionIdRole:
            return static_cast<qint64>(item.Id);
        case NameRole:
            return qtFromUtf8(item.NameUtf8);
        case IconKeyRole:
            return qtFromUtf8(item.IconKey);
        case IsDeletableRole:
            return item.IsDeletable;
        case KindRole:
            return item.Kind == Librova::Domain::EBookCollectionKind::User
                       ? QStringLiteral("user")
                       : QStringLiteral("preset");
        default:
            return {};
    }
}

QHash<int, QByteArray> CollectionListModel::roleNames() const
{
    return {
        {CollectionIdRole, "collectionId"},
        {NameRole,         "name"},
        {IconKeyRole,      "iconKey"},
        {IsDeletableRole,  "isDeletable"},
        {KindRole,         "kind"},
    };
}

} // namespace LibrovaQt
