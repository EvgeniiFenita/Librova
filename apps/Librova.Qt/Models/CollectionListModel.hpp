#pragma once

#include <vector>

#include <QAbstractListModel>

#include "Domain/BookCollection.hpp"

namespace LibrovaQt {

// Read-only list model exposing SBookCollection data to QML.
class CollectionListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role
    {
        CollectionIdRole = Qt::UserRole + 1,
        NameRole,
        IconKeyRole,
        IsDeletableRole,
        KindRole,     // "user" or "preset"
    };

    explicit CollectionListModel(QObject* parent = nullptr);

    void reset(const std::vector<Librova::Domain::SBookCollection>& collections);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

Q_SIGNALS:
    void countChanged();

private:
    std::vector<Librova::Domain::SBookCollection> m_items;
};

} // namespace LibrovaQt
