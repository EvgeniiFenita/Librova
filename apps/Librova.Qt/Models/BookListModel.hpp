#pragma once

#include <vector>

#include <QAbstractListModel>

#include "App/LibraryCatalogFacade.hpp"

namespace LibrovaQt {

// Read-only list model exposing SBookListItem data to QML.
// Full reset on every refresh — incremental diffing deferred to Phase G.
class BookListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role
    {
        BookIdRole = Qt::UserRole + 1,
        TitleRole,
        AuthorsRole,
        LanguageRole,
        SeriesNameRole,
        SeriesIndexRole,
        YearRole,
        GenresRole,
        FormatRole,
        ManagedPathRole,
        CoverPathRole,
        SizeBytesRole,
        AddedAtRole,
        CollectionIdsRole,
        CollectionNamesRole,
    };

    explicit BookListModel(QObject* parent = nullptr);

    // Reset the model with a new list of items.
    void reset(const std::vector<Librova::Application::SBookListItem>& items);
    void append(const std::vector<Librova::Application::SBookListItem>& items);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

Q_SIGNALS:
    void countChanged();

private:
    std::vector<Librova::Application::SBookListItem> m_items;
};

} // namespace LibrovaQt
