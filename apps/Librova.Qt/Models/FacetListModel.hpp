#pragma once

#include <vector>

#include <QAbstractListModel>
#include <QString>

#include "Domain/BookRepository.hpp"

namespace LibrovaQt {

// Model for language or genre filter facets.
// Each item carries a value, a result count, and an isSelected toggle
// that the UI can flip to build a filter set.
class FacetListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role
    {
        ValueRole = Qt::UserRole + 1,
        CountRole,
        IsSelectedRole,
    };

    explicit FacetListModel(QObject* parent = nullptr);

    // Replace facet list, preserving selection state for values that survive.
    void reset(const std::vector<Librova::Domain::SFacetItem>& facets);

    // Toggle or set selection for a specific value. Returns the new state.
    Q_INVOKABLE bool setSelected(const QString& value, bool selected);

    // Returns all currently selected values.
    [[nodiscard]] Q_INVOKABLE QStringList selectedValues() const;

    // Clear all selections.
    Q_INVOKABLE void clearSelection();

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

Q_SIGNALS:
    void countChanged();
    void selectionChanged();

private:
    struct SEntry
    {
        Librova::Domain::SFacetItem facet;
        bool isSelected = false;
    };

    std::vector<SEntry> m_items;
};

} // namespace LibrovaQt
