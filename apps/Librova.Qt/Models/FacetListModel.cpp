#include "Models/FacetListModel.hpp"

#include <algorithm>

#include "Adapters/QtStringConversions.hpp"

namespace LibrovaQt {

FacetListModel::FacetListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void FacetListModel::reset(const std::vector<Librova::Domain::SFacetItem>& facets)
{
    // Preserve current selection state for values that survive the reset.
    beginResetModel();
    std::vector<SEntry> next;
    next.reserve(facets.size());
    for (const auto& f : facets)
    {
        bool wasSelected = false;
        for (const auto& e : m_items)
        {
            if (e.facet.Value == f.Value)
            {
                wasSelected = e.isSelected;
                break;
            }
        }
        next.push_back({f, wasSelected});
    }
    m_items = std::move(next);
    endResetModel();
    emit countChanged();
}

bool FacetListModel::setSelected(const QString& value, bool selected)
{
    const std::string key = qtToUtf8(value);
    for (std::size_t i = 0; i < m_items.size(); ++i)
    {
        if (m_items[i].facet.Value == key)
        {
            if (m_items[i].isSelected == selected)
                return selected;
            m_items[i].isSelected = selected;
            const QModelIndex idx = index(static_cast<int>(i));
            emit dataChanged(idx, idx, {IsSelectedRole});
            emit selectionChanged();
            return selected;
        }
    }
    return false;
}

QStringList FacetListModel::selectedValues() const
{
    QStringList result;
    for (const auto& e : m_items)
    {
        if (e.isSelected)
            result.append(qtFromUtf8(e.facet.Value));
    }
    return result;
}

void FacetListModel::clearSelection()
{
    bool changed = false;
    for (std::size_t i = 0; i < m_items.size(); ++i)
    {
        if (m_items[i].isSelected)
        {
            m_items[i].isSelected = false;
            const QModelIndex idx = index(static_cast<int>(i));
            emit dataChanged(idx, idx, {IsSelectedRole});
            changed = true;
        }
    }
    if (changed)
        emit selectionChanged();
}

int FacetListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_items.size());
}

QVariant FacetListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(m_items.size()))
        return {};

    const auto& e = m_items[static_cast<std::size_t>(index.row())];
    switch (role)
    {
        case ValueRole:      return qtFromUtf8(e.facet.Value);
        case CountRole:      return static_cast<int>(e.facet.Count);
        case IsSelectedRole: return e.isSelected;
        default:             return {};
    }
}

bool FacetListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (role != IsSelectedRole || !index.isValid()
        || index.row() < 0 || index.row() >= static_cast<int>(m_items.size()))
    {
        return false;
    }
    auto& e = m_items[static_cast<std::size_t>(index.row())];
    const bool newVal = value.toBool();
    if (e.isSelected == newVal)
        return false;
    e.isSelected = newVal;
    emit dataChanged(index, index, {IsSelectedRole});
    emit selectionChanged();
    return true;
}

QHash<int, QByteArray> FacetListModel::roleNames() const
{
    return {
        {ValueRole,      "value"},
        {CountRole,      "count"},
        {IsSelectedRole, "isSelected"},
    };
}

} // namespace LibrovaQt
